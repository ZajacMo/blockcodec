#include "blockcodec/codec.hpp"

#include "blockcodec/bitstream.hpp"
#include "blockcodec/format.hpp"
#include "blockcodec/metrics.hpp"
#include "core/pixel.hpp"
#include "core/rblock.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace blockcodec {
namespace {

double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// The number of bits needed to represent every value in [0, maximum].
int bits_for(int maximum) {
    int bits = 1;
    while ((std::uint64_t{1} << bits) <= static_cast<std::uint64_t>(maximum)) {
        ++bits;
    }
    return bits;
}

// The block spanning `lt` .. `rb`, with its corners read from the image.
//
// The block test compares the pixels against the interpolation of the corner
// samples, so a candidate block is only meaningful once its corners are
// known.
RBlock candidate_block(const cv::Mat& image, cv::Point lt, cv::Point rb) {
    return RBlock(lt, rb, read_corners(image, lt, rb));
}

// Finds the bottom right corner of the block that starts at `lt`.
cv::Point find_rb(const cv::Mat& image, const char* marked, int rows, int cols,
                   cv::Point lt, int sigma) {
    cv::Point rb = lt;

    // Extend along the diagonal first.
    while (rb.x + 1 < cols && rb.y + 1 < rows &&
           candidate_block(image, lt, cv::Point(rb.x + 1, rb.y + 1))
               .is_block(image, marked, rows, cols, sigma)) {
        ++rb.x;
        ++rb.y;
    }
    // Then along the columns.
    while (rb.x + 1 < cols &&
           candidate_block(image, lt, cv::Point(rb.x + 1, rb.y))
               .is_block(image, marked, rows, cols, sigma)) {
        ++rb.x;
    }
    // Then along the rows.
    while (rb.y + 1 < rows &&
           candidate_block(image, lt, cv::Point(rb.x, rb.y + 1))
               .is_block(image, marked, rows, cols, sigma)) {
        ++rb.y;
    }
    return rb;
}

}  // namespace

Codec::Codec(CompressOptions options) : options_(options) {}

std::vector<std::uint8_t> Codec::compress(const cv::Mat& source,
                                           CompressResult* result) {
    const double started = now_seconds();


    if (source.empty()) {
        throw std::runtime_error("cannot compress an empty image");
    }
    if (source.depth() != CV_8U) {
        throw std::runtime_error("only 8-bit images are supported");
    }
    if (options_.sigma < 0) {
        throw std::runtime_error("the tolerance must not be negative");
    }

    const int rows = source.rows;
    const int cols = source.cols;
    const int channels = source.channels();
    const int sigma = options_.sigma;

    std::vector<char> marked(static_cast<std::size_t>(rows) * cols, false);
    std::vector<RBlock> blocks;

    // Find the blocks. The scan order matches the legacy implementation:
    // rows from top to bottom, and within a row from left to right.
    for (int row = 0; row < rows; ++row) {
        int column = 0;
        while (column < cols) {
            if (marked[static_cast<std::size_t>(row) * cols + column]) {
                ++column;
                continue;
            }

            const cv::Point lt(column, row);
            const cv::Point rb =
                find_rb(source, marked.data(), rows, cols, lt, sigma);

            RBlock block(lt, rb, read_corners(source, lt, rb));

            blocks.push_back(block);
            block.mark(marked.data(), rows, cols);
            column = rb.x + 1;
        }
    }

    // The field widths are derived from the header, so both sides compute
    // them the same way without depending on their own position.
    const int row_bits = bits_for(rows - 1);
    const int column_bits = bits_for(cols - 1);
    const int extent_bits = bits_for(std::max(rows, cols) - 1);

    BitWriter writer;
    for (const RBlock& block : blocks) {
        writer.write(static_cast<std::uint64_t>(block.lt().y), row_bits);
        writer.write(static_cast<std::uint64_t>(block.lt().x), column_bits);
        writer.write(static_cast<std::uint64_t>(block.rb().x - block.lt().x),
                     extent_bits);
        writer.write(static_cast<std::uint64_t>(block.rb().y - block.lt().y),
                     extent_bits);

        for (const std::uint8_t corner : block.corners()) {
            writer.write(corner, 8);
        }
    }

    std::vector<std::uint8_t> payload;
    writer.to_bytes(payload);

    ContainerHeader header;
    header.rows = static_cast<std::uint32_t>(rows);
    header.cols = static_cast<std::uint32_t>(cols);
    header.channels = static_cast<std::uint8_t>(channels);
    header.depth_bits = 8;
    header.colour = channels > 1;
    header.block_count = static_cast<std::uint32_t>(blocks.size());

    std::vector<std::uint8_t> container = write_container(header, payload);

    if (result != nullptr) {
        result->bit_size = writer.bit_count();
        result->byte_size = container.size();
        result->bpp = bpp(result->bit_size, rows, cols);
        result->cr = compression_ratio(result->bit_size, rows, cols, channels, 8);
        result->block_count = blocks.size();
        result->compress_ms = (now_seconds() - started) * 1000.0;

        // The result is the caller's, so it may still hold the extents of
        // an earlier operation. They are replaced, not appended to.
        result->blocks.clear();
        result->blocks.reserve(blocks.size());
        for (const RBlock& block : blocks) {
            result->blocks.emplace_back(block.lt().x, block.lt().y,
                                        block.rb().x - block.lt().x + 1,
                                        block.rb().y - block.lt().y + 1);
        }

        // The quality is measured against the source, so it is computed by
        // decoding what was just written. The decoder does not have the
        // source, so it cannot do it itself.
        const cv::Mat reconstructed = decompress(container, nullptr);
        result->psnr = psnr(source, reconstructed);
    }

    return container;
}

cv::Mat Codec::decompress(const std::vector<std::uint8_t>& container,
                           CompressResult* result) {
    const double started = now_seconds();


    std::vector<std::uint8_t> payload;
    const ContainerHeader header = read_container(container, payload);

    const int rows = static_cast<int>(header.rows);
    const int cols = static_cast<int>(header.cols);
    const int channels = static_cast<int>(header.channels);

    const int row_bits = bits_for(rows - 1);
    const int column_bits = bits_for(cols - 1);
    const int extent_bits = bits_for(std::max(rows, cols) - 1);

    // Every block has the same size: the four position fields and the
    // corner samples, four per channel.
    const int block_bits = row_bits + column_bits + 2 * extent_bits +
                           8 * channels * 4;
    const std::size_t required_bits =
        static_cast<std::size_t>(header.block_count) * block_bits;

    // The reader is given the whole buffer, so that the padding after the
    // last block can be read and checked.
    BitReader reader(payload.data(), payload.size());

    if (reader.bit_count() < required_bits) {
        throw std::runtime_error(
            "the payload is shorter than the declared blocks require");
    }

    cv::Mat image(rows, cols, CV_8UC(channels), cv::Scalar(0));

    std::vector<cv::Rect> extents;
    extents.reserve(header.block_count);

    for (std::uint32_t i = 0; i < header.block_count; ++i) {
        const int start_row = static_cast<int>(reader.read(row_bits));
        const int start_column = static_cast<int>(reader.read(column_bits));
        const int width = static_cast<int>(reader.read(extent_bits)) + 1;
        const int height = static_cast<int>(reader.read(extent_bits)) + 1;

        if (start_row >= rows || start_column >= cols ||
            start_row + height > rows || start_column + width > cols) {
            throw std::runtime_error("a block lies outside the image");
        }

        const cv::Point lt(start_column, start_row);
        const cv::Point rb(start_column + width - 1, start_row + height - 1);

        std::vector<std::uint8_t> corners(static_cast<std::size_t>(channels) * 4);
        for (std::uint8_t& corner : corners) {
            corner = static_cast<std::uint8_t>(reader.read(8));
        }

        RBlock block(lt, rb, corners);
        block.restore(image);

        extents.emplace_back(lt.x, lt.y, width, height);
    }

    // The payload must hold exactly the declared number of blocks. The
    // block count is a fact about the format, not a hint, so a mismatch
    // means the file is corrupt.
    if (reader.bit_position() != required_bits) {
        throw std::runtime_error(
            "the payload does not hold the declared number of blocks");
    }

    // The bits between the last block and the end of its byte are padding
    // and must be zero. The specification requires this check, so that a
    // corrupted file is rejected rather than silently accepted.
    //
    // Only the padding of the last byte is checked. Trailing bytes after
    // the payload are tolerated, as the specification says they may be.
    while (reader.bit_position() < required_bits + (8 - required_bits % 8) % 8) {
        if (reader.read_bit()) {
            throw std::runtime_error(
                "the padding bits after the last block are not zero");
        }
    }

    if (result != nullptr) {
        result->bit_size = required_bits;
        result->byte_size = container.size();
        result->bpp = bpp(result->bit_size, rows, cols);
        result->cr = compression_ratio(result->bit_size, rows, cols, channels,
                                        header.depth_bits);
        result->block_count = header.block_count;
        result->decompress_ms = (now_seconds() - started) * 1000.0;
        result->blocks = std::move(extents);
    }

    return image;
}

cv::Mat Codec::decompress(const std::string& path, CompressResult* result) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open '" + path + "' for reading");
    }
    const std::vector<std::uint8_t> container((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
    return decompress(container, result);
}

double psnr(const cv::Mat& a, const cv::Mat& b) {
    if (a.empty() || b.empty()) {
        throw std::runtime_error("blockcodec::psnr: an image is empty");
    }
    if (a.rows != b.rows || a.cols != b.cols || a.channels() != b.channels()) {
        throw std::runtime_error("blockcodec::psnr: the images have different shapes");
    }

    double sum = 0.0;
    for (int row = 0; row < a.rows; ++row) {
        for (int column = 0; column < a.cols; ++column) {
            for (int c = 0; c < a.channels(); ++c) {
                const double difference =
                    static_cast<double>(sample_at(a, row, column, c)) -
                    static_cast<double>(sample_at(b, row, column, c));
                sum += difference * difference;
            }
        }
    }

    if (sum == 0.0) {
        return std::numeric_limits<double>::infinity();
    }

    const double maximum = 255.0;
    const double pixels = static_cast<double>(a.rows) * a.cols * a.channels();
    return 10.0 * std::log10((maximum * maximum * pixels) / sum);
}

}  // namespace blockcodec
