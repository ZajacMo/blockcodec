#pragma once

#include <opencv2/core.hpp>

#include <cstddef>
#include <vector>

namespace blockcodec {
namespace bench {

// One measurement of the JPEG encoder at a given quality.
//
// The size is the size of the encoded JPEG file, and the bits per pixel and
// the compression ratio are taken from that file, so the figures can be
// compared with the ones of the blockcodec container.
struct JpegMeasurement {
    int quality = 0;
    std::size_t byte_size = 0;
    double bpp = 0.0;
    double cr = 0.0;
    double psnr = 0.0;
    double encode_ms = 0.0;
    double decode_ms = 0.0;
};

// Encodes `image` with the JPEG encoder at each of the given qualities and
// measures the resulting size and quality.
//
// The quality is measured with blockcodec::psnr, so that the JPEG and the
// blockcodec numbers are computed in exactly the same way and the two can be
// read against each other.
//
// A quality that the encoder refuses, or whose decoding does not reproduce the
// shape of the source, is left out of the result.
std::vector<JpegMeasurement> measure_jpeg(const cv::Mat& image,
                                           const std::vector<int>& qualities);

}  // namespace bench
}  // namespace blockcodec
