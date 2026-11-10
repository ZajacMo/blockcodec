#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"
#include "blockcodec/segment.hpp"

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

// An image whose three channels hold the same values.
cv::Mat make_flat_colour(int rows, int cols) {
    cv::Mat image(rows, cols, CV_8UC3);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const uchar value = static_cast<uchar>((x * 3 + y * 5) & 0xFF);
            for (int c = 0; c < 3; ++c) {
                image.ptr<uchar>(y)[x * 3 + c] = value;
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

TEST(Codec, ColourCostsTheSameAsGreyscale) {
    // The three channels of this image hold the same values, so the colour
    // image describes the same picture as the greyscale one and must
    // compress to the same number of blocks.
    //
    // The corner samples are stored per channel, and a block that mixes the
    // channels up still passes the block test, so the mistake shows up here
    // as a much larger block count rather than as a failure to decode.
    const cv::Mat grey = make_gradient(64, 64, 1);
    const cv::Mat colour = make_flat_colour(64, 64);

    Codec codec;
    CompressResult grey_result;
    CompressResult colour_result;
    codec.compress(grey, &grey_result);
    codec.compress(colour, &colour_result);

    EXPECT_EQ(colour_result.block_count, grey_result.block_count);
    EXPECT_NEAR(colour_result.psnr, grey_result.psnr, 1e-6);
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

TEST(Codec, ReportsTheBlockExtents) {
    const cv::Mat source = make_gradient(64, 64, 1);

    Codec codec;
    CompressResult result;
    codec.compress(source, &result);

    const std::vector<cv::Rect>& blocks = result.blocks;
    ASSERT_FALSE(blocks.empty());

    // Every block lies inside the image and has a positive extent.
    for (const cv::Rect& block : blocks) {
        EXPECT_GE(block.x, 0);
        EXPECT_GE(block.y, 0);
        EXPECT_LE(block.x + block.width, source.cols);
        EXPECT_LE(block.y + block.height, source.rows);
        EXPECT_GT(block.width, 0);
        EXPECT_GT(block.height, 0);
    }
}

TEST(Segment, DrawsTheOverlayOnACopy) {
    const cv::Mat source = make_gradient(64, 64, 1);

    Codec codec;
    CompressResult result;
    codec.compress(source, &result);

    const cv::Mat overlay = blockcodec::segmentation_overlay(source, result.blocks);

    // The overlay is a colour image even when the source is greyscale, and
    // the source is left untouched.
    EXPECT_EQ(overlay.rows, source.rows);
    EXPECT_EQ(overlay.cols, source.cols);
    EXPECT_EQ(overlay.channels(), 3);

    const cv::Mat untouched = source.clone();
    codec.compress(source);
    EXPECT_EQ(cv::countNonZero(source != untouched), 0);
}

TEST(Codec, ReportsTheBlockExtentsOfADecodedContainer) {
    // The overlay is meant to be buildable from a container the codec did
    // not compress itself, so the decoder has to report the extents too.
    const cv::Mat source = make_gradient(64, 64, 1);

    Codec encoder;
    CompressResult encoder_result;
    const std::vector<std::uint8_t> container = encoder.compress(source, &encoder_result);

    Codec decoder;
    CompressResult decoder_result;
    decoder.decompress(container, &decoder_result);

    ASSERT_EQ(decoder_result.blocks.size(), encoder_result.blocks.size());
    for (std::size_t i = 0; i < decoder_result.blocks.size(); ++i) {
        EXPECT_EQ(decoder_result.blocks[i].x, encoder_result.blocks[i].x);
        EXPECT_EQ(decoder_result.blocks[i].y, encoder_result.blocks[i].y);
        EXPECT_EQ(decoder_result.blocks[i].width, encoder_result.blocks[i].width);
        EXPECT_EQ(decoder_result.blocks[i].height, encoder_result.blocks[i].height);
    }
}

TEST(Codec, ReportsNoExtentsForAFailedOperation) {
    // A rejected operation must not report the extents of an earlier one,
    // because they would describe a picture that is not the one the caller
    // last asked about. The result of a failed call is left empty.
    Codec codec;

    CompressResult failed_compress;
    EXPECT_THROW(codec.compress(cv::Mat(), &failed_compress), std::runtime_error);
    EXPECT_TRUE(failed_compress.blocks.empty());

    CompressResult failed_decompress;
    EXPECT_THROW(codec.decompress(std::vector<std::uint8_t>{1, 2, 3, 4, 5},
                                  &failed_decompress),
                 std::runtime_error);
    EXPECT_TRUE(failed_decompress.blocks.empty());
}
