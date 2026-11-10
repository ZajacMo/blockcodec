// Console front-end for the block-based image compression codec.
//
// The default invocation reproduces the benchmark table: every image in the
// sample set is compressed at a range of sigma values, then decompressed, and
// the compression ratio and PSNR are reported for each combination.
//
// A small hand-rolled argument parser is used so the project keeps zero
// third-party dependencies beyond OpenCV itself.

#include "Compress_gray.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Sample images used by the default benchmark, matching the original project.
const char* const kDefaultImages[] = {
    "100gray", "197gray", "baboonzyp512", "barbarazyp512", "broke256",
    "decho256", "f16", "flightzyp512", "lena512", "lenazyp512"
};

// Sigma values used by the default benchmark.
const int kDefaultSigmas[] = { 1, 5, 10, 20, 30, 50 };

// Parsed command line options.
struct Options {
    std::string input_dir = "images";
    std::string output_dir = ".";
    std::vector<std::string> images;
    std::vector<int> sigmas;
    bool selftest = false;
    bool help = false;
};

void print_usage() {
    std::cout <<
        "OpenCV image compression codec (console)\n"
        "\n"
        "Usage:\n"
        "  compression_tool [options]\n"
        "\n"
        "Options:\n"
        "  --input <dir>      Directory holding the source images (default: images)\n"
        "  --output <dir>     Directory the compressed streams are written to (default: .)\n"
        "  --images <list>    Comma separated image names, without extension\n"
        "                     (default: the built-in sample set)\n"
        "  --sigma <list>     Comma separated sigma values (default: 1,5,10,20,30,50)\n"
        "  --selftest         Run a synthetic round-trip check and exit\n"
        "  -h, --help         Show this message\n"
        "\n"
        "Examples:\n"
        "  compression_tool --selftest\n"
        "  compression_tool --images lena512,f16 --sigma 5,20\n"
        "  compression_tool --input images --output out --sigma 10\n";
}

// Splits a comma separated list.
std::vector<std::string> split_list(const std::string& value) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : value) {
        if (c == ',') {
            if (!current.empty()) parts.push_back(current);
            current.clear();
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

// Parses argv. Returns false when an argument is malformed.
bool parse_args(int argc, char** argv, Options& opts, std::string& error) {
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        auto require_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                error = std::string("missing value for ") + name;
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "-h" || arg == "--help") {
            opts.help = true;
        } else if (arg == "--selftest") {
            opts.selftest = true;
        } else if (arg == "--input") {
            const char* v = require_value("--input");
            if (!v) return false;
            opts.input_dir = v;
        } else if (arg == "--output") {
            const char* v = require_value("--output");
            if (!v) return false;
            opts.output_dir = v;
        } else if (arg == "--images") {
            const char* v = require_value("--images");
            if (!v) return false;
            opts.images = split_list(v);
        } else if (arg == "--sigma") {
            const char* v = require_value("--sigma");
            if (!v) return false;
            for (const std::string& s : split_list(v)) {
                try {
                    opts.sigmas.push_back(std::stoi(s));
                } catch (const std::exception&) {
                    error = "invalid sigma value: " + s;
                    return false;
                }
            }
        } else {
            error = "unknown argument: " + arg;
            return false;
        }
    }
    return true;
}

// Compresses one image at one sigma value and reports the result.
bool run_single(const std::string& image_path,
                const std::string& stream_path,
                int sigma,
                int& out_rows,
                int& out_cols,
                long long& out_bits,
                double& rate,
                double& psnr,
                double& compress_seconds,
                double& decompress_seconds) {
    // imread returns an empty Mat when the file cannot be read; continuing
    // with it would silently produce garbage, so it is checked explicitly.
    cv::Mat source = cv::imread(image_path, cv::IMREAD_GRAYSCALE);
    if (source.empty()) {
        std::cerr << "error: cannot read image '" << image_path << "'\n";
        return false;
    }

    Compress_gray encoder;
    encoder.init_compress(source, sigma);
    encoder.compress();
    encoder.save(stream_path);

    Compress_gray decoder;
    decoder.init_uncompress(stream_path);
    cv::Mat decoded = decoder.uncompress();

    if (decoded.empty() || decoded.size() != source.size()) {
        std::cerr << "error: decoded image for '" << image_path
                  << "' does not match the source dimensions\n";
        return false;
    }

    out_rows = source.rows;
    out_cols = source.cols;
    out_bits = encoder.getWriteLen();
    rate = encoder.getRate();
    psnr = Compress_gray::getPSNR(source, decoded);
    compress_seconds = encoder.getTime();
    decompress_seconds = decoder.getTime();
    return true;
}

// Builds a deterministic synthetic image so the self test does not depend on
// any file being present.
cv::Mat make_synthetic_image(int rows, int cols) {
    cv::Mat img(rows, cols, CV_8UC1);
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            // A smooth gradient plus a deterministic checkerboard, which gives
            // the block matcher both flat regions and sharp edges to work on.
            int base = (x * 255) / (cols > 1 ? cols - 1 : 1);
            int checker = (((x / 8) + (y / 8)) % 2) ? 40 : 0;
            img.at<uchar>(y, x) = static_cast<uchar>((base + checker) % 256);
        }
    }
    return img;
}

// Round-trips a synthetic image through the codec and checks the result.
int run_selftest(const std::string& output_dir) {
    const int rows = 64;
    const int cols = 96;
    const int sigma = 20;

    cv::Mat source = make_synthetic_image(rows, cols);

    const std::string stream_path = output_dir + "/selftest.nam";
    const std::string decoded_path = output_dir + "/selftest_decoded.pgm";

    Compress_gray encoder;
    encoder.init_compress(source, sigma);
    encoder.compress();
    encoder.save(stream_path);

    Compress_gray decoder;
    decoder.init_uncompress(stream_path);
    cv::Mat decoded = decoder.uncompress();

    if (decoded.empty()) {
        std::cerr << "selftest FAILED: decoded image is empty\n";
        return 1;
    }
    if (decoded.rows != rows || decoded.cols != cols) {
        std::cerr << "selftest FAILED: expected " << rows << "x" << cols
                  << ", got " << decoded.rows << "x" << decoded.cols << "\n";
        return 1;
    }

    // Write the decoded image as a binary PGM so the vector can be checked
    // by tools/verify_vector.py without an OpenCV dependency.
    if (!cv::imwrite(decoded_path, decoded)) {
        std::cerr << "selftest FAILED: cannot write '" << decoded_path << "'\n";
        return 1;
    }

    const double psnr = Compress_gray::getPSNR(source, decoded);
    const double rate = encoder.getRate();

    // The codec is lossy, so the check is a lower bound on the quality rather
    // than exact equality.
    const double kMinPsnr = 20.0;
    if (!(psnr >= kMinPsnr)) {
        std::cerr << "selftest FAILED: PSNR " << psnr << " dB is below the "
                  << kMinPsnr << " dB threshold\n";
        return 1;
    }

    std::cout << "selftest PASSED\n"
              << "  image      : " << rows << "x" << cols << " synthetic\n"
              << "  sigma      : " << sigma << "\n"
              << "  PSNR       : " << std::fixed << std::setprecision(4) << psnr << " dB\n"
              << "  ratio      : " << rate << "\n"
              << "  stream     : " << stream_path << "\n"
              << "  decoded    : " << decoded_path << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options opts;
    std::string error;

    if (!parse_args(argc, argv, opts, error)) {
        std::cerr << "error: " << error << "\n\n";
        print_usage();
        return 2;
    }

    if (opts.help) {
        print_usage();
        return 0;
    }

    if (opts.selftest) {
        return run_selftest(opts.output_dir);
    }

    if (opts.images.empty()) {
        opts.images.assign(std::begin(kDefaultImages), std::end(kDefaultImages));
    }
    if (opts.sigmas.empty()) {
        opts.sigmas.assign(std::begin(kDefaultSigmas), std::end(kDefaultSigmas));
    }

    std::cout << "image\toriginal size\tsigma\tcompressed bits\tratio\t"
                 "compress time (s)\tdecompress time (s)\tPSNR (dB)\n";

    int failures = 0;
    for (const std::string& name : opts.images) {
        const std::string image_path = opts.input_dir + "/" + name + ".bmp";
        for (int sigma : opts.sigmas) {
            const std::string stream_path =
                opts.output_dir + "/" + name + "_sigma" + std::to_string(sigma) + ".nam";

            int rows = 0, cols = 0;
            long long bits = 0;
            double rate = 0, psnr = 0, ct = 0, dt = 0;
            if (!run_single(image_path, stream_path, sigma, rows, cols, bits,
                            rate, psnr, ct, dt)) {
                failures++;
                continue;
            }

            std::cout << name << ".bmp\t"
                      << rows << "*" << cols << "\t"
                      << sigma << "\t"
                      << bits << "\t"
                      << std::fixed << std::setprecision(4) << rate << "\t"
                      << std::setprecision(6) << ct << "\t"
                      << std::setprecision(6) << dt << "\t"
                      << std::setprecision(4) << psnr << "\n";
        }
    }

    if (failures > 0) {
        std::cerr << failures << " image(s) failed to process\n";
        return 1;
    }
    return 0;
}
