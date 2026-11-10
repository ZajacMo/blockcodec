#pragma once

#include <opencv2/core.hpp>

namespace blockcodec {

// Reads the sample of channel `c` at (column, row).
//
// `Mat::at<cv::Vec3b>` is only valid on a three channel matrix. Calling it
// on a greyscale matrix reads past the element, so the samples are read
// through the row pointer instead, which works for any channel count.
inline int sample_at(const cv::Mat& image, int row, int column, int c) {
    return static_cast<int>(image.ptr<uchar>(row)[column * image.channels() + c]);
}

// Writes the sample of channel `c` at (column, row).
inline void set_sample_at(cv::Mat& image, int row, int column, int c, int value) {
    image.ptr<uchar>(row)[column * image.channels() + c] =
        static_cast<uchar>(value);
}

}  // namespace blockcodec
