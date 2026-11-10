#pragma once

#include "core/block.hpp"

#include <cstdint>
#include <vector>

namespace blockcodec {

// A block whose interior is reconstructed by bilinear interpolation between
// its four corner samples.
//
// The corner samples are stored per channel, so the same class describes
// both greyscale and colour blocks. The block test compares the largest
// deviation across the channels against the tolerance, which for a single
// channel image reduces to the greyscale rule exactly.
//
// The interpolation runs vertically first and then horizontally, and each
// step truncates towards zero. The legacy implementation interpolated in
// the same order with the same truncation, so the reconstruction of a
// single channel image is unchanged.
class RBlock : public Block {
public:
    RBlock() = default;
    RBlock(cv::Point lt, cv::Point rb, int channels);
    RBlock(cv::Point lt, cv::Point rb, const std::vector<std::uint8_t>& corners);

    // Returns true when every pixel inside the block differs from the
    // interpolated value by no more than `sigma`.
    //
    // The corner samples are read from `image` rather than from the stored
    // ones, because the block search evaluates a candidate block before its
    // corners have been recorded.
    bool is_block(const cv::Mat& image, const char* marked,
                  int rows, int cols, int sigma) const override;

    // Restores the block into `image`.
    void restore(cv::Mat& image) const;

    // The four corner samples, in the order top-left, top-right,
    // bottom-left, bottom-right, each holding `channels` values.
    const std::vector<std::uint8_t>& corners() const { return corners_; }

    void set_corners(const std::vector<std::uint8_t>& corners) { corners_ = corners; }

    // The value of the block at (column, row) for channel `c`, by bilinear
    // interpolation of the four corner samples.
    int interpolated(int column, int row, int c) const;

private:
    std::vector<std::uint8_t> corners_;
};

}  // namespace blockcodec
