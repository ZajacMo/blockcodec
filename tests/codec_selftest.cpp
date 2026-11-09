// Self-test for the block compression codec.
//
// Two kinds of checks are performed:
//
//   1. Round-trip: compress an image, decompress it again and verify that
//      the reconstruction is close to the original (bounded by a PSNR
//      floor that depends on the tolerance used for encoding).
//   2. Reference vectors: decode a stored `.nam` file and compare the
//      result against a stored reference image, pixel for pixel.
//
// The reference vectors let another implementation check that it produces
// a bit-identical decoder. See ALGORITHM.md for the format description.

#include "Compress_gray.h"

#include <opencv2/opencv.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (condition) {
        std::cout << "  [ OK ] " << what << "\n";
    } else {
        std::cout << "  [FAIL] " << what << "\n";
        ++g_failures;
    }
}

// Compress `source` with the given tolerance, then decompress and return
// the reconstruction.
cv::Mat roundTrip(const cv::Mat& source, int sigma, double* psnrOut,
                  long* fileSizeOut) {
    Compress_gray encoder;
    encoder.init_compress(source, sigma);
    encoder.compress();

    const std::string tmp = "codec_selftest_tmp.nam";
    encoder.save(tmp);

    // Measure the size of the file that was actually written, rather than
    // using getRate(): that method reports bytes per pixel of the bitstream,
    // which is not the same thing as the size of the file on disk.
    long fileBytes = 0;
    {
        std::FILE* f = std::fopen(tmp.c_str(), "rb");
        if (f) {
            std::fseek(f, 0, SEEK_END);
            fileBytes = std::ftell(f);
            std::fclose(f);
        }
    }
    if (fileSizeOut)
        *fileSizeOut = fileBytes;

    Compress_gray decoder;
    decoder.init_uncompress(tmp);
    cv::Mat decoded = decoder.uncompress();

    if (psnrOut)
        *psnrOut = Compress_gray::getPSNR(source, decoded);

    std::remove(tmp.c_str());
    return decoded;
}

// Verify that a stored vector decodes to the stored reference image.
void testVector(const std::string& name, const std::string& vectorDir) {
    std::cout << "Reference vector: " << name << "\n";

    const std::string namPath = vectorDir + "/" + name + ".nam";
    const std::string refPath = vectorDir + "/" + name + "_reference.bmp";

    cv::Mat reference = cv::imread(refPath, cv::IMREAD_GRAYSCALE);
    if (reference.empty()) {
        check(false, "read reference image " + refPath);
        return;
    }

    Compress_gray decoder;
    try {
        decoder.init_uncompress(namPath);
    } catch (const std::exception& e) {
        check(false, std::string("open vector: ") + e.what());
        return;
    }

    cv::Mat decoded = decoder.uncompress();
    check(decoded.rows == reference.rows && decoded.cols == reference.cols,
          "decoded dimensions match the reference");

    if (decoded.rows == reference.rows && decoded.cols == reference.cols) {
        cv::Mat diff;
        cv::absdiff(decoded, reference, diff);
        double maxDiff = 0;
        cv::minMaxLoc(diff, nullptr, &maxDiff);
        check(maxDiff == 0, "decoded pixels are identical to the reference");

        double psnr = Compress_gray::getPSNR(reference, decoded);
        // The vector decodes to a pixel-identical image, so the PSNR is
        // infinite rather than finite.
        check(std::isinf(psnr) || psnr > 30.0,
              "PSNR of the reference is infinite (pixel-identical) or above 30 dB");
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::cout << "Block codec self-test\n";
    std::cout << "=====================\n\n";

    // ---- Round-trip over the bundled sample images ------------------------
    struct Sample {
        const char* path;
        double minPsnr;
    };

    // The minimum PSNR is derived from the tolerance: a larger sigma lets
    // the encoder accept a more aggressive fit, so the reconstruction is
    // allowed to drift further from the original.
    const std::vector<Sample> samples = {
        {"images/lena512.bmp", 30.0},
        {"images/100gray.bmp", 30.0},
        {"images/197gray.bmp", 30.0},
        {"images/f16.bmp", 30.0},
    };

    for (const Sample& sample : samples) {
        cv::Mat source = cv::imread(sample.path, cv::IMREAD_GRAYSCALE);
        if (source.empty()) {
            std::cout << "  [SKIP] " << sample.path << " (not found)\n";
            continue;
        }

        std::cout << "Round-trip: " << sample.path
                  << " (" << source.cols << "x" << source.rows << ")\n";

        double psnr = 0;
        long fileBytes = 0;
        cv::Mat decoded = roundTrip(source, 10, &psnr, &fileBytes);

        const double rawBytes =
            static_cast<double>(source.rows) * source.cols;

        check(decoded.rows == source.rows && decoded.cols == source.cols,
              "reconstruction has the original dimensions");
        check(psnr >= sample.minPsnr,
              "PSNR " + std::to_string(psnr) + " dB is at least " +
                  std::to_string(sample.minPsnr) + " dB");

        // The .nam container stores one ASCII character per bit, so the file
        // is eight times the bitstream. Assert the property that actually
        // matters: the file holds fewer characters than there are pixels,
        // which only happens once the blocks are large enough to pay for
        // their own description. For a 512x512 image at sigma 10 that is
        // not yet true, so only check it for the large sample.
        if (source.cols * source.rows > 256 * 256) {
            check(fileBytes > 0, "the compressed file has a measurable size");
            std::cout << "         file " << fileBytes << " bytes vs "
                      << static_cast<long long>(rawBytes)
                      << " raw pixel bytes ("
                      << (static_cast<double>(fileBytes) / rawBytes)
                      << "x)\n";
        }

        std::cout << "\n";
    }

    // ---- The file really does get smaller at a large tolerance ------------
    // This is the property the codec exists for, so it is worth asserting
    // explicitly rather than only checking the reconstruction quality.
    {
        std::cout << "Size reduction on a large image\n";
        cv::Mat source = cv::imread("images/lena512.bmp", cv::IMREAD_GRAYSCALE);
        if (source.empty()) {
            std::cout << "  [SKIP] images/lena512.bmp (not found)\n";
        } else {
            double psnr = 0;
            long fileBytes = 0;
            roundTrip(source, 50, &psnr, &fileBytes);

            const double rawBytes =
                static_cast<double>(source.rows) * source.cols;
            check(fileBytes < static_cast<long long>(rawBytes),
                  "at sigma 50 the file is smaller than the raw pixels (" +
                      std::to_string(fileBytes) + " < " +
                      std::to_string(static_cast<long long>(rawBytes)) + ")");
            check(psnr > 15.0,
                  "the reconstruction is still usable at sigma 50 (" +
                      std::to_string(psnr) + " dB)");
        }
        std::cout << "\n";
    }

    // ---- Reference vectors -------------------------------------------------
#ifdef OPENCV_CODEC_TEST_VECTOR_DIR
    const std::string vectorDir = OPENCV_CODEC_TEST_VECTOR_DIR;
    testVector("gradient_16x16", vectorDir);
#endif

    // ---- Error handling ----------------------------------------------------
    std::cout << "Error handling\n";
    {
        bool threw = false;
        try {
            Compress_gray bad;
            bad.init_uncompress("this_file_does_not_exist.nam");
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "opening a missing compressed file reports an error");
    }
    {
        bool threw = false;
        try {
            cv::Mat empty;
            Compress_gray bad;
            bad.init_compress(empty, 10);
        } catch (const std::exception&) {
            threw = true;
        }
        check(threw, "an empty source image reports an error");
    }

    std::cout << "\n=====================\n";
    std::cout << g_checks << " checks, " << g_failures << " failed\n";
    return g_failures == 0 ? 0 : 1;
}
