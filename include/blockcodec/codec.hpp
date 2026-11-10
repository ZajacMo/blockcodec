#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace blockcodec {

// The tolerance of the block test.
//
// A small sigma accepts only near-uniform blocks, which produces large files
// at a high PSNR. A large sigma accepts coarse blocks, which produces small
// files at a lower PSNR.
struct CompressOptions {
    int sigma = 10;
};

// The measurements taken during an encode or decode.
struct CompressResult {
    std::size_t bit_size = 0;       // bits in the bitstream
    std::size_t byte_size = 0;      // bytes in the container
    double      bpp = 0.0;          // bits per pixel
    double      cr = 0.0;           // compression ratio
    double      psnr = 0.0;         // dB, infinity for a perfect reconstruction
    double      compress_ms = 0.0;
    double      decompress_ms = 0.0;
    std::size_t block_count = 0;

    // The extents of the blocks, in image coordinates. They are filled in
    // by both the encoder and the decoder, so an overlay can be built from
    // a container that was decoded by a different codec instance.
    std::vector<cv::Rect> blocks;
};

// Encodes and decodes images with a block based codec.
//
// The codec is stateless: each call is independent, and the results are
// returned rather than held.
class Codec {
public:
    explicit Codec(CompressOptions options = {});

    // Encodes `source` and returns the container.
    // Throws std::runtime_error if the image is empty or is not 8-bit.
    std::vector<std::uint8_t> compress(const cv::Mat& source,
                                       CompressResult* result = nullptr);

    // Decodes a container produced by compress().
    // Throws std::runtime_error on malformed input.
    cv::Mat decompress(const std::vector<std::uint8_t>& container,
                       CompressResult* result = nullptr);

    cv::Mat decompress(const std::string& path, CompressResult* result = nullptr);

    const CompressOptions& options() const { return options_; }

private:
    CompressOptions options_;
};

// The peak signal-to-noise ratio between two images, in decibels.
// Returns infinity when the reconstruction is identical to the reference.
double psnr(const cv::Mat& a, const cv::Mat& b);

}  // namespace blockcodec
