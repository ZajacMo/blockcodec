#include "blockcodec/segment.hpp"

#include <opencv2/imgproc.hpp>

namespace blockcodec {

cv::Mat segmentation_overlay(const cv::Mat& image,
                             const std::vector<cv::Rect>& blocks) {
    cv::Mat overlay;
    if (image.channels() == 1) {
        cv::cvtColor(image, overlay, cv::COLOR_GRAY2BGR);
    } else {
        overlay = image.clone();
    }

    for (const cv::Rect& block : blocks) {
        cv::rectangle(overlay, block, cv::Scalar(0, 0, 0), 1);
    }

    return overlay;
}

}  // namespace blockcodec
