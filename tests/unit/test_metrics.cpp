#include "blockcodec/metrics.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>

using blockcodec::bpp;
using blockcodec::compression_ratio;

TEST(Metrics, BitsPerPixelIsBitsOverPixels) {
    EXPECT_DOUBLE_EQ(bpp(8000, 100, 100), 0.8);
    EXPECT_DOUBLE_EQ(bpp(0, 100, 100), 0.0);
}

TEST(Metrics, CompressionRatioIsRawOverEncoded) {
    // 100x100 pixels at 8 bits per channel: 80000 raw bits.
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 100, 100, 1, 8), 1.0);
    EXPECT_DOUBLE_EQ(compression_ratio(40000, 100, 100, 1, 8), 2.0);

    // A colour image has three times the raw bits.
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 100, 100, 3, 8), 3.0);
}

TEST(Metrics, CompressionRatioIsInfiniteForAnEmptyStream) {
    EXPECT_TRUE(std::isinf(compression_ratio(0, 100, 100, 1, 8)));
}

TEST(Metrics, BitsPerPixelOfADegenerateImageIsZero) {
    // An image without pixels cannot have a meaningful rate, so the
    // helpers report zero rather than dividing by zero.
    EXPECT_DOUBLE_EQ(bpp(8000, 0, 100), 0.0);
    EXPECT_DOUBLE_EQ(bpp(8000, 100, 0), 0.0);
    EXPECT_DOUBLE_EQ(bpp(8000, -1, -1), 0.0);
}

TEST(Metrics, CompressionRatioOfADegenerateImageIsZero) {
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 0, 100, 1, 8), 0.0);
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 100, 0, 1, 8), 0.0);
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 100, 100, 0, 8), 0.0);
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 100, 100, 1, 0), 0.0);
}
