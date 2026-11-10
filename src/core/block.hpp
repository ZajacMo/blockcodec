#pragma once

#include <opencv2/core.hpp>

namespace blockcodec {

// A rectangular region of an image.
//
// The region is described by its four corner samples; the interior is
// reconstructed by bilinear interpolation. A block covers one or more colour
// channels.
//
// The corners use the OpenCV convention: `x` is the column and `y` is the
// row.
class Block {
public:
    Block() = default;
    Block(cv::Point lt, cv::Point rb, int channels);

    virtual ~Block() = default;

    // Returns true when every pixel inside the block differs from the
    // interpolated value by no more than `sigma`.
    virtual bool is_block(const cv::Mat& image, const bool* marked,
                          int rows, int cols, int sigma) const = 0;

    // Marks every pixel inside the block as covered.
    virtual void mark(bool* marked, int rows, int cols) const;

    cv::Point lt() const { return lt_; }
    cv::Point rb() const { return rb_; }
    int channels() const { return channels_; }

protected:
    cv::Point lt_;
    cv::Point rb_;
    int channels_ = 1;
};

}  // namespace blockcodec
