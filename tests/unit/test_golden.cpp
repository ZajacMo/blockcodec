// Proves that the rewrite did not change the behaviour of the codec on
// single channel images.
//
// The legacy implementation is kept as an oracle. The two are compared on
// the block structure and on the reconstructed pixels, not on the encoded
// bytes: the new container is densely packed and transmits the gap width
// explicitly, so its bytes necessarily differ from the legacy stream.
//
// The legacy container holds one ASCII character per bit, so the file it
// writes is eight times the size of the bitstream and reaches several
// megabytes for a 512x512 image. It is written to a temporary name and
// removed again, including when the test fails.

#include "blockcodec/codec.hpp"

#include "Compress_gray.h"

#include <gtest/gtest.h>

#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace {

// The sample images the comparison runs on.
std::vector<std::string> image_paths() {
    return {
        "images/100gray.bmp",
        "images/197gray.bmp",
        "images/f16.bmp",
        "images/lena512.bmp",
        "images/baboonzyp512.bmp",
    };
}

// Removes the file when it goes out of scope, so that a failed test does
// not leave a multi-megabyte file behind.
class TemporaryFile {
public:
    explicit TemporaryFile(std::string path) : path_(std::move(path)) {}
    ~TemporaryFile() { std::remove(path_.c_str()); }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

// The path of the file the legacy encoder writes.
//
// The file is placed in the system temporary directory, so that the test
// does not leave anything behind in the source tree. The name carries a
// timestamp, so that two runs at the same time do not fight over it.
std::string legacy_temporary_path() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() /
            ("blockcodec_golden_" + std::to_string(stamp) + ".nam")).string();
}

// The number of pixels that differ between two images of the same shape.
//
// The comparison is done by hand rather than with cv::absdiff, because
// absdiff requires both operands to have the same type, and the count of
// differing pixels is what the test is about.
std::size_t differing_pixels(const cv::Mat& a, const cv::Mat& b) {
    std::size_t count = 0;
    for (int row = 0; row < a.rows; ++row) {
        const uchar* left = a.ptr<uchar>(row);
        const uchar* right = b.ptr<uchar>(row);
        for (int column = 0; column < a.cols; ++column) {
            if (left[column] != right[column]) {
                ++count;
            }
        }
    }
    return count;
}

// What the two implementations produced for one image and one tolerance.
struct Comparison {
    int rows = 0;
    int cols = 0;
    int legacy_channels = 0;
    int new_channels = 0;
    std::size_t differing_pixels = 0;
    std::size_t legacy_blocks = 0;
    std::size_t new_blocks = 0;
    double legacy_psnr = 0.0;
    double new_psnr = 0.0;
};

// Runs the legacy encoder and decoder and the new codec over the same
// image, and compares what they produced.
Comparison compare_with_the_legacy(const cv::Mat& source, int sigma,
                                   const std::string& temporary_path) {
    // The legacy encoder.
    Compress_gray encoder;
    encoder.init_compress(source, sigma);
    encoder.compress();
    encoder.save(temporary_path);

    // The legacy decoder.
    Compress_gray decoder;
    decoder.init_uncompress(temporary_path);
    const cv::Mat legacy_out = decoder.uncompress();

    // The new codec.
    blockcodec::Codec codec(blockcodec::CompressOptions{sigma});
    blockcodec::CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);
    const cv::Mat new_out = codec.decompress(container);

    Comparison comparison;
    comparison.rows = legacy_out.rows;
    comparison.cols = legacy_out.cols;
    comparison.legacy_channels = legacy_out.channels();
    comparison.new_channels = new_out.channels();
    comparison.legacy_blocks = encoder.blocks().size();
    comparison.new_blocks = result.block_count;
    comparison.legacy_psnr = Compress_gray::getPSNR(source, legacy_out);
    comparison.new_psnr = result.psnr;

    if (legacy_out.size() == new_out.size() &&
        legacy_out.channels() == new_out.channels()) {
        comparison.differing_pixels = differing_pixels(legacy_out, new_out);
    } else {
        // The shapes differ, so the pixels cannot be compared. The count is
        // set to the number of pixels of the legacy image, so that the test
        // fails rather than passing on an uncompared image.
        comparison.differing_pixels =
            static_cast<std::size_t>(legacy_out.rows) * legacy_out.cols;
    }

    return comparison;
}

}  // namespace

TEST(Golden, MatchesTheLegacyReconstruction) {
    const TemporaryFile temporary(legacy_temporary_path());

    for (const std::string& path : image_paths()) {
        // The sample images are versioned, so a missing one is a broken
        // checkout rather than a reason to skip the comparison.
        const cv::Mat source = cv::imread(path, cv::IMREAD_GRAYSCALE);
        ASSERT_FALSE(source.empty())
            << "the sample image " << path << " is missing; the test runs "
            << "with the source directory as the working directory";

        for (const int sigma : {1, 10, 30}) {
            const Comparison comparison =
                compare_with_the_legacy(source, sigma, temporary.path());

            EXPECT_EQ(comparison.rows, source.rows) << path << " sigma=" << sigma;
            EXPECT_EQ(comparison.cols, source.cols) << path << " sigma=" << sigma;
            EXPECT_EQ(comparison.new_channels, comparison.legacy_channels)
                << path << " sigma=" << sigma;

            // The reconstructed pixels must be identical.
            EXPECT_EQ(comparison.differing_pixels, 0u)
                << path << " sigma=" << sigma
                << ": the reconstruction differs from the legacy implementation";

            // The block count must match.
            EXPECT_EQ(comparison.new_blocks, comparison.legacy_blocks)
                << path << " sigma=" << sigma
                << ": the new encoder chose a different number of blocks";
        }
    }
}

TEST(Golden, MatchesTheLegacyQualityOnLena512) {
    // The quality of the two encoders must agree at every tolerance, and
    // both must match the published table.
    struct Expected {
        int sigma;
        double psnr;
    };
    const std::vector<Expected> expected = {
        {1, 59.2259},
        {5, 42.2910},
        {10, 36.3887},
        {20, 30.3825},
        {30, 26.6933},
        {50, 22.3604},
    };

    const cv::Mat source = cv::imread("images/lena512.bmp", cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(source.empty())
        << "the sample image images/lena512.bmp is missing; the test runs "
        << "with the source directory as the working directory";

    const TemporaryFile temporary(legacy_temporary_path());

    for (const Expected& entry : expected) {
        const Comparison comparison =
            compare_with_the_legacy(source, entry.sigma, temporary.path());

        EXPECT_EQ(comparison.differing_pixels, 0u)
            << "sigma=" << entry.sigma
            << ": the reconstruction differs from the legacy implementation";
        EXPECT_EQ(comparison.new_blocks, comparison.legacy_blocks)
            << "sigma=" << entry.sigma
            << ": the new encoder chose a different number of blocks";

        // The legacy implementation computes the ratio with log10f, so the
        // two differ in the last decimals. Four decimal places is what the
        // published table quotes.
        EXPECT_NEAR(comparison.new_psnr, comparison.legacy_psnr, 1e-4)
            << "sigma=" << entry.sigma;
        EXPECT_NEAR(comparison.new_psnr, entry.psnr, 1e-4)
            << "sigma=" << entry.sigma << ": the quality is not the published one";
    }
}
