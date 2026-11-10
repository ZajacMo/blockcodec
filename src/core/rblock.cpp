#include "core/rblock.hpp"

#include "core/pixel.hpp"

#include <algorithm>
#include <cmath>

namespace blockcodec {
namespace {

// Linear interpolation between `a` and `b` at the ratio `rate`.
//
// The result is truncated to an unsigned char, exactly as the legacy helper
// did: it returned a uchar, so every intermediate value was rounded down
// before it was used again. The difference is visible in the golden test.
//
// The multiplication must not be contracted into a fused multiply-add. GCC
// permits that by default and MSVC does not, and a fused operation rounds
// once instead of twice, which can change the truncated result. The
// specification records this as a property of the format, and the
// intermediate values are therefore held in a volatile, which the compiler
// may not fold into a single instruction.
//
// The two properties together make the interpolation give the same answer on
// every implementation.
int interpolate(int a, int b, double rate) {
    volatile double product = static_cast<double>(b - a) * rate;
    volatile double sum = static_cast<double>(a) + product;
    return static_cast<int>(static_cast<unsigned char>(sum));
}

// The ratio of `i` within the range [a, b].
double ratio(int a, int b, int i) {
    if (b == a) {
        return 0.0;
    }
    return static_cast<double>(i - a) / static_cast<double>(b - a);
}

}  // namespace

std::vector<std::uint8_t> read_corners(const cv::Mat& image, cv::Point lt,
                                        cv::Point rb) {
    const int channels = image.channels();
    std::vector<std::uint8_t> corners(
        static_cast<std::size_t>(channels) * 4, 0);

    // The corners are ordered top-left, top-right, bottom-left, bottom-right.
    const int columns[4] = {lt.x, rb.x, lt.x, rb.x};
    const int rows[4] = {lt.y, lt.y, rb.y, rb.y};

    // The samples are stored per channel, which is the layout the
    // interpolation reads them in.
    for (int c = 0; c < channels; ++c) {
        for (int i = 0; i < 4; ++i) {
            corners[static_cast<std::size_t>(c) * 4 + static_cast<std::size_t>(i)] =
                static_cast<std::uint8_t>(sample_at(image, rows[i], columns[i], c));
        }
    }
    return corners;
}

RBlock::RBlock(cv::Point lt, cv::Point rb, int channels)
    : Block(lt, rb, channels),
      corners_(static_cast<std::size_t>(channels) * 4, 0) {}

RBlock::RBlock(cv::Point lt, cv::Point rb, const std::vector<std::uint8_t>& corners)
    : Block(lt, rb, static_cast<int>(corners.size()) / 4), corners_(corners) {}

int RBlock::interpolated(int column, int row, int c) const {
    const std::size_t base = static_cast<std::size_t>(c) * 4;
    const int top_left = corners_[base + 0];
    const int top_right = corners_[base + 1];
    const int bottom_left = corners_[base + 2];
    const int bottom_right = corners_[base + 3];

    const double row_ratio = ratio(lt_.y, rb_.y, row);
    const double column_ratio = ratio(lt_.x, rb_.x, column);

    // Interpolate down the left and the right edge first, then across.
    const int left = interpolate(top_left, bottom_left, row_ratio);
    const int right = interpolate(top_right, bottom_right, row_ratio);
    return interpolate(left, right, column_ratio);
}

bool RBlock::is_block(const cv::Mat& image, const char* marked,
                       int rows, int cols, int sigma) const {
    for (int y = lt_.y; y <= rb_.y; ++y) {
        for (int x = lt_.x; x <= rb_.x; ++x) {
            if (marked[static_cast<std::size_t>(y) * cols + x]) {
                return false;
            }

            // The largest deviation across the channels decides whether the
            // block is acceptable. For a single channel image this is the
            // same rule as the greyscale implementation.
            int largest = 0;
            for (int c = 0; c < channels_; ++c) {
                const int expected = interpolated(x, y, c);
                const int actual = sample_at(image, y, x, c);
                largest = std::max(largest, std::abs(actual - expected));
            }
            if (largest > sigma) {
                return false;
            }
        }
    }
    return true;
}

void RBlock::restore(cv::Mat& image) const {
    for (int y = lt_.y; y <= rb_.y; ++y) {
        for (int x = lt_.x; x <= rb_.x; ++x) {
            for (int c = 0; c < channels_; ++c) {
                set_sample_at(image, y, x, c, interpolated(x, y, c));
            }
        }
    }
}

}  // namespace blockcodec
