// Sweeps the tolerance over a set of images and reports the size and quality
// trade off.
//
// With --baseline jpeg the same images are also encoded by the JPEG encoder,
// so the numbers can be read against a reference that everybody knows.
//
// The comparison is made on the sizes of the files that are actually
// written: the size of the .bcc container and the size of the encoded JPEG.
// The bits per pixel and the compression ratio are derived from those file
// sizes, so the two codecs are measured in the same units. The payload of
// the container, which is what the library's own bpp and cr are measured on,
// is reported in the payload_* columns of the CSV.
//
// The JPEG baseline is also matched to the blockcodec quality. For every
// sigma the benchmark finds the lowest JPEG quality that still reaches the
// PSNR that the codec reached, and reports the size of the JPEG file at that
// quality. That is where the two curves cross, because it is the only place
// where they measure the same thing.
//
// The PSNR of both codecs is measured with blockcodec::psnr, so that the two
// numbers mean the same thing.
//
// The encode time of blockcodec includes the verification decode that
// Codec::compress performs to measure the PSNR it reports. The JPEG encode
// time does not include its decode, which is timed separately.
//
// JPEG has no alpha channel, so an image with four channels is compared in
// its three colour channels. The blockcodec measurement of the matched table
// is taken on the same three channels, so that the two sides of the
// comparison describe the same image.

#include "bench/jpeg_baseline.h"

#include "blockcodec/codec.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// The qualities that are measured for the baseline table.
const std::vector<int>& default_qualities() {
    static const std::vector<int> qualities{95, 90, 85, 80, 75, 70, 60,
                                            50, 40, 30, 20, 10};
    return qualities;
}

// The lowest and the highest quality the encoder is asked for.
constexpr int kMinQuality = 1;
constexpr int kMaxQuality = 100;

// One measurement of the blockcodec container.
struct BlockcodecMeasurement {
    std::string image;
    int rows = 0;
    int cols = 0;
    int channels = 1;
    int sigma = 0;
    std::size_t file_bytes = 0;   // the size of the .bcc file that is written
    double bpp = 0.0;             // bits per pixel of that file
    double cr = 0.0;              // raw pixels divided by that file
    double psnr = 0.0;
    double encode_ms = 0.0;
    double decode_ms = 0.0;
    std::size_t blocks = 0;
    std::size_t payload_bits = 0; // the bits of the bitstream
    double payload_bpp = 0.0;     // the library's own bits per pixel
    double payload_cr = 0.0;      // the library's own compression ratio
};

// The JPEG measurement that matches a blockcodec measurement.
struct MatchedMeasurement {
    std::string image;
    int sigma = 0;
    std::size_t bc_bytes = 0;
    double bc_bpp = 0.0;
    double bc_psnr = 0.0;
    int jpeg_quality = 0;
    std::size_t jpeg_bytes = 0;
    double jpeg_bpp = 0.0;
    double jpeg_psnr = 0.0;
    double psnr_delta = 0.0;     // the JPEG PSNR minus the blockcodec PSNR
    bool jpeg_reached = false;   // false when no JPEG quality reaches the target
    const char* match = "";      // "reached" or "short"
    const char* smaller = "";    // "jpeg", "blockcodec" or "equal"
};

// The measured JPEG curve of one image.
//
// The whole curve is measured once, from the lowest to the highest quality,
// so that the qualities that bracket a target PSNR can be looked up without
// assuming that the PSNR rises monotonically with the quality.
class JpegCurve {
public:
    explicit JpegCurve(const cv::Mat& source) : source_(source) {}

    // Measures the whole curve, if it has not been measured yet.
    const std::vector<blockcodec::bench::JpegMeasurement>& curve() {
        if (!measured_) {
            std::vector<int> qualities;
            for (int quality = kMinQuality; quality <= kMaxQuality; ++quality) {
                qualities.push_back(quality);
            }
            measurements_ = blockcodec::bench::measure_jpeg(source_, qualities);
            for (const blockcodec::bench::JpegMeasurement& measurement :
                 measurements_) {
                by_quality_[measurement.quality] = measurement;
            }
            measured_ = true;
        }
        return measurements_;
    }

    // The measurement of one quality, or nullptr if it could not be made.
    const blockcodec::bench::JpegMeasurement* find(int quality) const {
        const auto found = by_quality_.find(quality);
        return found == by_quality_.end() ? nullptr : &found->second;
    }

    // The lowest quality whose PSNR reaches `target`.
    //
    // This is the point where the two curves are compared: the JPEG file is
    // at least as good as the blockcodec one, and it is the cheapest JPEG
    // file that is so.
    //
    // `reached` is false when not even the highest quality reaches the
    // target, which means JPEG cannot deliver that quality at all.
    //
    // Returns false when no quality could be measured at all.
    bool cheapest_reaching(double target, blockcodec::bench::JpegMeasurement& out,
                           bool& reached) {
        const std::vector<blockcodec::bench::JpegMeasurement>& all = curve();
        if (all.empty()) {
            return false;
        }

        // The measurements come back in the order of the qualities, so the
        // first one that reaches the target is the lowest one that does.
        const blockcodec::bench::JpegMeasurement* best = nullptr;
        for (const blockcodec::bench::JpegMeasurement& measurement : all) {
            if (measurement.psnr >= target) {
                best = &measurement;
                break;
            }
        }

        if (best != nullptr) {
            reached = true;
            out = *best;
            return true;
        }

        // Not even the highest quality reaches the target.
        reached = false;
        out = all.back();
        return true;
    }

private:
    const cv::Mat& source_;
    std::vector<blockcodec::bench::JpegMeasurement> measurements_;
    std::map<int, blockcodec::bench::JpegMeasurement> by_quality_;
    bool measured_ = false;
};

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator)) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

// The path of the file that holds a companion table. It is written next to
// the main CSV, with a suffix in front of the extension.
fs::path with_suffix(const fs::path& path, const std::string& suffix) {
    fs::path result = path;
    result.replace_filename(path.stem().string() + suffix + path.extension().string());
    return result;
}

// The image without its alpha channel.
//
// JPEG has no alpha channel, so a four channel image is compared in its
// three colour channels. The alpha of a screenshot is constant, so nothing
// that the comparison measures is lost.
cv::Mat without_alpha(const cv::Mat& source) {
    if (source.channels() != 4) {
        return source;
    }
    cv::Mat result;
    cv::cvtColor(source, result, cv::COLOR_RGBA2RGB);
    return result;
}

void print_usage() {
    std::printf(
        "usage: blockcodec_bench [options]\n"
        "\n"
        "  --images PATH        the directory to sweep, or a single image\n"
        "  --sigmas LIST        the tolerances to sweep, comma separated\n"
        "  --csv PATH           write the measurements to a CSV file\n"
        "  --baseline NAME      'jpeg' to add the JPEG baseline\n"
        "  --qualities LIST     the JPEG qualities of the baseline table\n"
        "  --help               print this text\n");
}

void print_blockcodec_header() {
    std::printf("%-24s %5s %5s %4s %6s %10s %8s %8s %9s %8s %8s %8s\n", "image",
                "rows", "cols", "ch", "sigma", "file_bytes", "bpp", "cr",
                "psnr", "enc_ms", "dec_ms", "blocks");
}

void print_blockcodec_row(const BlockcodecMeasurement& row) {
    std::printf("%-24s %5d %5d %4d %6d %10zu %8.4f %8.4f %9.4f %8.2f %8.2f %8zu\n",
                row.image.c_str(), row.rows, row.cols, row.channels, row.sigma,
                row.file_bytes, row.bpp, row.cr, row.psnr, row.encode_ms,
                row.decode_ms, row.blocks);
}

void print_jpeg_header() {
    std::printf("%-24s %7s %10s %8s %8s %9s %8s %8s\n", "image", "quality",
                "file_bytes", "bpp", "cr", "psnr", "enc_ms", "dec_ms");
}

void print_jpeg_row(const std::string& image,
                    const blockcodec::bench::JpegMeasurement& row) {
    std::printf("%-24s %7d %10zu %8.4f %8.4f %9.4f %8.2f %8.2f\n", image.c_str(),
                row.quality, row.byte_size, row.bpp, row.cr, row.psnr,
                row.encode_ms, row.decode_ms);
}

void print_matched_header() {
    std::printf("%-24s %6s %10s %8s %9s %7s %11s %8s %9s %8s %-9s %s\n", "image",
                "sigma", "bc_bytes", "bc_bpp", "bc_psnr", "jpeg_q", "jpeg_bytes",
                "jpeg_bpp", "jpeg_psnr", "d_psnr", "match", "smaller");
}

void print_matched_row(const MatchedMeasurement& row) {
    std::printf("%-24s %6d %10zu %8.4f %9.4f %7d %11zu %8.4f %9.4f %8.3f %-9s %s\n",
                row.image.c_str(), row.sigma, row.bc_bytes, row.bc_bpp,
                row.bc_psnr, row.jpeg_quality, row.jpeg_bytes, row.jpeg_bpp,
                row.jpeg_psnr, row.psnr_delta, row.match, row.smaller);
}

}  // namespace

int main(int argc, char** argv) {
    std::string image_path = "images";
    std::string csv_path;
    std::vector<int> sigmas{1, 5, 10, 20, 30, 50};
    std::vector<int> qualities = default_qualities();
    bool jpeg_baseline = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", name);
                std::exit(2);
            }
            return argv[++i];
        };

        if (argument == "--images") {
            image_path = next("--images");
        } else if (argument == "--csv") {
            csv_path = next("--csv");
        } else if (argument == "--sigmas") {
            sigmas.clear();
            for (const std::string& part : split(next("--sigmas"), ',')) {
                sigmas.push_back(std::atoi(part.c_str()));
            }
        } else if (argument == "--qualities") {
            qualities.clear();
            for (const std::string& part : split(next("--qualities"), ',')) {
                qualities.push_back(std::atoi(part.c_str()));
            }
        } else if (argument == "--baseline") {
            jpeg_baseline = next("--baseline") == "jpeg";
        } else if (argument == "--help" || argument == "-h") {
            print_usage();
            return 0;
        } else {
            std::fprintf(stderr, "unknown option %s\n", argument.c_str());
            print_usage();
            return 2;
        }
    }

    // The images are taken from a directory, so that a new sample can be
    // added without touching the benchmark.
    std::vector<fs::path> images;
    const fs::path root(image_path);
    if (fs::is_directory(root)) {
        std::error_code error;
        for (const auto& entry : fs::directory_iterator(root, error)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const std::string extension = entry.path().extension().string();
            if (extension == ".bmp" || extension == ".png" ||
                extension == ".jpg" || extension == ".jpeg" ||
                extension == ".tif" || extension == ".tiff") {
                images.push_back(entry.path());
            }
        }
    } else if (fs::is_regular_file(root)) {
        images.push_back(root);
    }

    std::sort(images.begin(), images.end());

    if (images.empty()) {
        std::fprintf(stderr, "no images found in '%s'\n", image_path.c_str());
        return 1;
    }

    // The CSV files are written as the measurements are produced, so that a
    // run that is interrupted still leaves the measurements behind.
    std::ofstream csv;
    std::ofstream jpeg_csv;
    std::ofstream matched_csv;
    if (!csv_path.empty()) {
        const fs::path path(csv_path);
        csv.open(path);
        csv << "image,rows,cols,channels,sigma,bytes,bpp,cr,psnr,encode_ms,"
               "decode_ms,blocks,payload_bits,payload_bpp,payload_cr\n";

        jpeg_csv.open(with_suffix(path, "_jpeg"));
        jpeg_csv << "image,rows,cols,channels,quality,bytes,bpp,cr,psnr,"
                    "encode_ms,decode_ms\n";

        matched_csv.open(with_suffix(path, "_matched"));
        matched_csv << "image,sigma,bc_bytes,bc_bpp,bc_psnr,jpeg_quality,"
                       "jpeg_bytes,jpeg_bpp,jpeg_psnr,psnr_delta,jpeg_reached,"
                       "match,smaller\n";
    }

    std::printf("blockcodec benchmark\n");
    std::printf("====================\n");
    std::printf("images  : %s\n", image_path.c_str());
    std::printf("sigmas  :");
    for (const int sigma : sigmas) {
        std::printf(" %d", sigma);
    }
    std::printf("\n");

    std::printf("\nblockcodec\n");
    std::printf("----------\n");
    std::printf("bytes is the size of the .bcc file, bpp and cr are derived "
                "from it.\n");
    print_blockcodec_header();

    // The JPEG measurements of one image.
    struct JpegTable {
        std::string image;
        int rows = 0;
        int cols = 0;
        int channels = 1;
        std::vector<blockcodec::bench::JpegMeasurement> measurements;
    };
    std::vector<JpegTable> jpeg_tables;
    std::vector<MatchedMeasurement> matched_rows;

    for (const fs::path& path : images) {
        const cv::Mat source = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
        if (source.empty()) {
            std::fprintf(stderr, "cannot read '%s'\n", path.string().c_str());
            continue;
        }

        const double pixels = static_cast<double>(source.rows) * source.cols;
        const double raw_bytes = pixels * source.channels();

        // The comparison is made on the colour channels, because JPEG has no
        // alpha channel.
        const cv::Mat comparison = without_alpha(source);
        const bool dropped_alpha = comparison.channels() != source.channels();

        JpegCurve curve(comparison);

        if (jpeg_baseline) {
            // The whole curve is measured once, so that the qualities that
            // bracket a target PSNR can be looked up.
            curve.curve();
        }

        for (const int sigma : sigmas) {
            blockcodec::Codec codec({sigma});
            blockcodec::CompressResult encoded;
            const std::vector<std::uint8_t> container =
                codec.compress(source, &encoded);

            // The decode is timed on its own, so that the verification
            // decode that Codec::compress performs is not counted twice.
            blockcodec::CompressResult decoded_result;
            codec.decompress(container, &decoded_result);

            BlockcodecMeasurement row;
            row.image = path.filename().string();
            row.rows = source.rows;
            row.cols = source.cols;
            row.channels = source.channels();
            row.sigma = sigma;
            row.file_bytes = encoded.byte_size;
            row.bpp =
                pixels > 0.0 ? static_cast<double>(encoded.byte_size) * 8.0 / pixels
                             : 0.0;
            row.cr = encoded.byte_size > 0
                          ? raw_bytes / static_cast<double>(encoded.byte_size)
                          : 0.0;
            row.psnr = encoded.psnr;
            row.encode_ms = encoded.compress_ms;
            row.decode_ms = decoded_result.decompress_ms;
            row.blocks = encoded.block_count;
            row.payload_bits = encoded.bit_size;
            row.payload_bpp = encoded.bpp;
            row.payload_cr = encoded.cr;

            print_blockcodec_row(row);

            if (csv.is_open()) {
                csv << row.image << ',' << row.rows << ',' << row.cols << ','
                     << row.channels << ',' << row.sigma << ',' << row.file_bytes
                     << ',' << row.bpp << ',' << row.cr << ',' << row.psnr << ','
                     << row.encode_ms << ',' << row.decode_ms << ',' << row.blocks
                     << ',' << row.payload_bits << ',' << row.payload_bpp << ','
                     << row.payload_cr << '\n';
            }

            if (!jpeg_baseline) {
                continue;
            }

            // The blockcodec measurement of the comparison image, so that
            // both sides of the comparison describe the same pixels.
            blockcodec::CompressResult matched_encoded;
            if (dropped_alpha) {
                blockcodec::Codec comparison_codec({sigma});
                comparison_codec.compress(comparison, &matched_encoded);
            } else {
                matched_encoded = encoded;
            }

            const double comparison_pixels =
                static_cast<double>(comparison.rows) * comparison.cols;

            // The lowest JPEG quality that still reaches the PSNR of the
            // blockcodec measurement, so that the two can be compared at the
            // same quality. It is the point where the curves meet.
            blockcodec::bench::JpegMeasurement match;
            bool reached = false;
            if (!curve.cheapest_reaching(matched_encoded.psnr, match, reached)) {
                continue;
            }

            MatchedMeasurement row_matched;
            row_matched.image = row.image;
            row_matched.sigma = sigma;
            row_matched.bc_bytes = matched_encoded.byte_size;
            row_matched.bc_bpp =
                comparison_pixels > 0.0
                    ? static_cast<double>(matched_encoded.byte_size) * 8.0 /
                          comparison_pixels
                    : 0.0;
            row_matched.bc_psnr = matched_encoded.psnr;
            row_matched.jpeg_quality = match.quality;
            row_matched.jpeg_bytes = match.byte_size;
            row_matched.jpeg_bpp = match.bpp;
            row_matched.jpeg_psnr = match.psnr;
            row_matched.psnr_delta = match.psnr - matched_encoded.psnr;
            row_matched.jpeg_reached = reached;
            row_matched.match = reached ? "reached" : "short";

            // The two are only comparable when JPEG reaches the PSNR of the
            // blockcodec measurement. When it falls short, the JPEG file is
            // the largest one that was measured, so the sizes say nothing
            // about the trade off and no winner is named.
            if (reached) {
                row_matched.smaller =
                    match.byte_size < matched_encoded.byte_size
                        ? "jpeg"
                        : (match.byte_size > matched_encoded.byte_size
                               ? "blockcodec"
                               : "equal");
            } else {
                row_matched.smaller = "-";
            }
            matched_rows.push_back(row_matched);

            if (matched_csv.is_open()) {
                matched_csv << row_matched.image << ',' << row_matched.sigma
                            << ',' << row_matched.bc_bytes << ','
                            << row_matched.bc_bpp << ',' << row_matched.bc_psnr
                            << ',' << row_matched.jpeg_quality << ','
                            << row_matched.jpeg_bytes << ','
                            << row_matched.jpeg_bpp << ','
                            << row_matched.jpeg_psnr << ','
                            << row_matched.psnr_delta << ','
                            << (row_matched.jpeg_reached ? 1 : 0) << ','
                            << row_matched.match << ','
                            << row_matched.smaller << '\n';
            }
        }

        if (jpeg_baseline) {
            JpegTable table;
            table.image = path.filename().string();
            table.rows = comparison.rows;
            table.cols = comparison.cols;
            table.channels = comparison.channels();
            for (const int quality : qualities) {
                const blockcodec::bench::JpegMeasurement* measurement =
                    curve.find(quality);
                if (measurement == nullptr) {
                    continue;
                }
                table.measurements.push_back(*measurement);
                if (jpeg_csv.is_open()) {
                    jpeg_csv << table.image << ',' << table.rows << ','
                             << table.cols << ',' << table.channels << ','
                             << measurement->quality << ','
                             << measurement->byte_size << ',' << measurement->bpp
                             << ',' << measurement->cr << ',' << measurement->psnr
                             << ',' << measurement->encode_ms << ','
                             << measurement->decode_ms << '\n';
                }
            }
            jpeg_tables.push_back(table);
        }
    }

    if (jpeg_baseline) {
        std::printf("\nJPEG baseline\n");
        std::printf("------------\n");
        std::printf("bytes is the size of the encoded JPEG, bpp and cr are "
                    "derived from it.\n");
        print_jpeg_header();
        for (const JpegTable& table : jpeg_tables) {
            for (const blockcodec::bench::JpegMeasurement& measurement :
                 table.measurements) {
                print_jpeg_row(table.image, measurement);
            }
        }

        std::printf("\nJPEG at the matched quality\n");
        std::printf("---------------------------\n");
        std::printf("jpeg_q is the lowest measured JPEG quality that still "
                    "reaches the PSNR of the\n");
        std::printf("blockcodec measurement at the same sigma, so the two are "
                    "compared at the same\n");
        std::printf("quality. d_psnr is the JPEG PSNR minus the blockcodec "
                    "PSNR, so a positive\n");
        std::printf("value means the JPEG file is slightly better. match is "
                    "'reached' when the JPEG\n");
        std::printf("quality reaches the blockcodec PSNR and 'short' when not "
                    "even the highest JPEG\n");
        std::printf("quality does; a short row names no winner, because the "
                    "two are not compared at\n");
        std::printf("the same quality. smaller names the codec with the "
                    "smaller file at that quality.\n");
        print_matched_header();
        for (const MatchedMeasurement& row : matched_rows) {
            print_matched_row(row);
        }
    }

    return 0;
}
