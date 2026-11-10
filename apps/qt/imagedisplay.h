#ifndef IMAGEDISPLAY_H
#define IMAGEDISPLAY_H

#include <QImage>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
namespace gui {

// Converts a decoded image into a QImage.
//
// The QImage is given a copy of the pixels. A QImage built over the data of
// a matrix does not copy it, so it would stop being valid as soon as the
// matrix went out of scope.
inline QImage to_qimage(const cv::Mat& image) {
    if (image.empty()) {
        return QImage();
    }

    if (image.channels() == 3 || image.channels() == 4) {
        // OpenCV keeps the channels in blue, green, red order and QImage
        // expects red, green, blue.
        cv::Mat rgb;
        cv::cvtColor(image, rgb, image.channels() == 4 ? cv::COLOR_BGRA2RGB
                                                        : cv::COLOR_BGR2RGB);
        const QImage view(rgb.data, rgb.cols, rgb.rows,
                          static_cast<int>(rgb.step), QImage::Format_RGB888);
        return view.copy();
    }

    if (image.channels() == 2) {
        // Grey and alpha. The alpha channel is dropped, because a preview
        // is drawn on an opaque background.
        cv::Mat grey;
        cv::extractChannel(image, grey, 0);
        const QImage view(grey.data, grey.cols, grey.rows,
                          static_cast<int>(grey.step), QImage::Format_Grayscale8);
        return view.copy();
    }

    const QImage view(image.data, image.cols, image.rows,
                      static_cast<int>(image.step), QImage::Format_Grayscale8);
    return view.copy();
}

}  // namespace gui

#endif // IMAGEDISPLAY_H
