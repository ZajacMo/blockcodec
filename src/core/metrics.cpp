#include "blockcodec/metrics.hpp"

#include <limits>

namespace blockcodec {

double bpp(std::size_t bits, int rows, int cols) {
    if (rows <= 0 || cols <= 0) {
        return 0.0;
    }
    const double pixels = static_cast<double>(rows) * static_cast<double>(cols);
    return static_cast<double>(bits) / pixels;
}

double compression_ratio(std::size_t bits, int rows, int cols, int channels,
                         int depth_bits) {
    if (bits == 0) {
        return std::numeric_limits<double>::infinity();
    }
    if (rows <= 0 || cols <= 0 || channels <= 0 || depth_bits <= 0) {
        return 0.0;
    }
    const double raw_bits = static_cast<double>(rows) * cols * channels * depth_bits;
    return raw_bits / static_cast<double>(bits);
}

}  // namespace blockcodec
