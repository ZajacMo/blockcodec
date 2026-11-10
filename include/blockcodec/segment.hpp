#pragma once

#include <opencv2/core.hpp>

#include <vector>

namespace blockcodec {

// Draws the boundaries of the blocks on top of an image.
//
// The blocks are the regions the codec chose, recovered by decoding a
// container, so the overlay shows exactly what was encoded. The prototype of
// the project had a function called `segmentDisplay` that did the same
// thing.
//
// The image is returned as a copy, so the caller keeps its own.
cv::Mat segmentation_overlay(const cv::Mat& image,
                             const std::vector<cv::Rect>& blocks);

}  // namespace blockcodec
