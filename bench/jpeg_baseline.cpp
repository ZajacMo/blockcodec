#include "bench/jpeg_baseline.h"

#include "blockcodec/codec.hpp"

#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <cstdint>
#include <vector>

namespace blockcodec {
namespace bench {

namespace {

double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

}  // namespace

std::vector<JpegMeasurement> measure_jpeg(const cv::Mat& image,
                                           const std::vector<int>& qualities) {
    std::vector<JpegMeasurement> measurements;
    measurements.reserve(qualities.size());

    const double pixels = static_cast<double>(image.rows) * image.cols;
    const double raw_bytes = pixels * image.channels();

    for (const int quality : qualities) {
        std::vector<std::uint8_t> buffer;
        const std::vector<int> params{ cv::IMWRITE_JPEG_QUALITY, quality };

        const double encode_started = now_seconds();
        const bool encoded = cv::imencode(".jpg", image, buffer, params);
        const double encode_ms = (now_seconds() - encode_started) * 1000.0;

        if (!encoded) {
            continue;
        }

        const double decode_started = now_seconds();
        const cv::Mat decoded = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
        const double decode_ms = (now_seconds() - decode_started) * 1000.0;

        if (decoded.empty()) {
            continue;
        }

        // The JPEG encoder drops the alpha channel, so a four channel image
        // comes back with three. The quality cannot be measured then, and
        // the measurement is left out rather than compared against a
        // mismatched image.
        if (decoded.rows != image.rows || decoded.cols != image.cols ||
            decoded.channels() != image.channels()) {
            continue;
        }

        JpegMeasurement measurement;
        measurement.quality = quality;
        measurement.byte_size = buffer.size();
        measurement.bpp =
            pixels > 0.0 ? static_cast<double>(buffer.size()) * 8.0 / pixels : 0.0;
        measurement.cr =
            buffer.empty() ? 0.0 : raw_bytes / static_cast<double>(buffer.size());
        measurement.psnr = psnr(image, decoded);
        measurement.encode_ms = encode_ms;
        measurement.decode_ms = decode_ms;
        measurements.push_back(measurement);
    }

    return measurements;
}

}  // namespace bench
}  // namespace blockcodec
