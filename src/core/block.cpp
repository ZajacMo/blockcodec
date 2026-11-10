#include "core/block.hpp"

namespace blockcodec {

Block::Block(cv::Point lt, cv::Point rb, int channels)
    : lt_(lt), rb_(rb), channels_(channels) {}

void Block::mark(bool* marked, int rows, int cols) const {
    for (int y = lt_.y; y <= rb_.y; ++y) {
        for (int x = lt_.x; x <= rb_.x; ++x) {
            if (y >= 0 && y < rows && x >= 0 && x < cols) {
                marked[static_cast<std::size_t>(y) * cols + x] = true;
            }
        }
    }
}

}  // namespace blockcodec
