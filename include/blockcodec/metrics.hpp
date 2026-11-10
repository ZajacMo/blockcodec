#pragma once

#include <cstddef>

namespace blockcodec {

// The bits per pixel of a stream of `bits` bits over an image of
// `rows` x `cols` pixels.
double bpp(std::size_t bits, int rows, int cols);

// The compression ratio: the raw size in bits divided by the encoded size.
//
// Returns infinity when the encoded size is zero.
double compression_ratio(std::size_t bits, int rows, int cols, int channels,
                         int depth_bits);

}  // namespace blockcodec
