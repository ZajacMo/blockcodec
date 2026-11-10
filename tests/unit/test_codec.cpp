#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"

#include <gtest/gtest.h>

#include <opencv2/opencv.hpp>

#include <cmath>
#include <string>
#include <vector>

using blockcodec::Codec;
using blockcodec::CompressResult;

namespace {

cv::Mat make_gradient(int rows, int cols, int channels) {
    cv::Mat image(rows, cols, CV_8UC(channels));
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            // The samples are written through the row pointer, because
            // Mat::at<cv::Vec3b> is only valid on a three channel matrix and
            // reads past the element on a greyscale one.
            for (int c = 0; c < channels; ++c) {
                image.ptr<uchar>(y)[x * channels + c] =
                    static_cast<uchar>((x * 3 + y * 5 + c * 11) & 0xFF);
            }
        }
    }
    return image;
}

}  // namespace

TEST(Codec, RoundTripsGreyscale) {
    const cv::Mat source = make_gradient(64, 64, 1);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    const cv::Mat decoded = codec.decompress(container, &result);

    EXPECT_EQ(decoded.rows, source.rows);
    EXPECT_EQ(decoded.cols, source.cols);
    EXPECT_EQ(decoded.channels(), 1u);
    EXPECT_GT(result.psnr, 30.0);
    EXPECT_GT(result.block_count, 0u);
}

TEST(Codec, RoundTripsColour) {
    const cv::Mat source = make_gradient(64, 64, 3);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    const cv::Mat decoded = codec.decompress(container, &result);

    // The legacy implementation flattened the channels into the width, so a
    // 64x64 colour image decoded as 192x64 greyscale.
    EXPECT_EQ(decoded.rows, source.rows);
    EXPECT_EQ(decoded.cols, source.cols);
    EXPECT_EQ(decoded.channels(), 3u);
    EXPECT_GT(result.psnr, 30.0);
}

TEST(Codec, KeepsLandscapeGreyscaleGreyscale) {
    // The legacy decoder inferred colour from `cols > rows`, so a 256x128
    // greyscale image was reported as colour. The assertion therefore
    // targets the channel count carried by the container: asserting on the
    // decoded matrix would pass even when the inference is wrong, because
    // the legacy decoder always allocated a single channel result.
    const cv::Mat source = make_gradient(128, 256, 1);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    std::vector<std::uint8_t> payload;
    const blockcodec::ContainerHeader header = blockcodec::read_container(container, payload);
    EXPECT_EQ(header.channels, 1u);
    EXPECT_FALSE(header.colour);

    const cv::Mat decoded = codec.decompress(container, &result);
    EXPECT_EQ(decoded.cols, 256);
    EXPECT_EQ(decoded.rows, 128);
    EXPECT_EQ(decoded.channels(), 1u);
}

TEST(Codec, RejectsAnEmptyImage) {
    Codec codec;
    EXPECT_THROW(codec.compress(cv::Mat()), std::runtime_error);
}

TEST(Codec, RejectsANonEightBitImage) {
    cv::Mat source(16, 16, CV_32F);
    source.setTo(cv::Scalar(1.0));
    Codec codec;
    EXPECT_THROW(codec.compress(source), std::runtime_error);
}

TEST(Codec, RejectsAGarbageContainer) {
    Codec codec;
    EXPECT_THROW(codec.decompress(std::vector<std::uint8_t>{1, 2, 3, 4, 5}),
                 std::runtime_error);
}

TEST(Codec, ReportsBitsPerPixelConsistently) {
    const cv::Mat source = make_gradient(64, 64, 1);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    const double expected_bpp =
        static_cast<double>(result.bit_size) / (64.0 * 64.0);
    EXPECT_NEAR(result.bpp, expected_bpp, 1e-9);
    EXPECT_GT(result.byte_size, 0u);
    // The container is a 20 byte header plus the packed payload, whose
    // length is the bit count rounded up to a byte.
    EXPECT_LE(result.byte_size, result.bit_size / 8 + 20);
}
