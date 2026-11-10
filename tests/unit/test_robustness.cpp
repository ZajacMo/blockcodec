#include "blockcodec/bitstream.hpp"
#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"

#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

using blockcodec::BitWriter;
using blockcodec::Codec;
using blockcodec::ContainerHeader;
using blockcodec::write_container;

namespace {

// The number of bits the codec uses for a field that holds a value no larger
// than `maximum`. The container format derives every field width from the
// header, so a payload built by hand has to use the same widths.
int field_bits(int maximum) {
    int bits = 1;
    while ((static_cast<std::uint64_t>(1) << bits) <=
           static_cast<std::uint64_t>(maximum)) {
        ++bits;
    }
    return bits;
}

// Builds a container byte by byte, bypassing the writer's validation, so
// that a malformed header can reach the reader.
//
// The writer refuses to emit a malformed container, and a malformed container
// reaches the reader from outside anyway.
std::vector<std::uint8_t> make_raw_container(std::uint32_t rows,
                                              std::uint32_t cols,
                                              std::uint8_t channels,
                                              std::uint8_t depth_bits,
                                              std::uint32_t block_count,
                                              const std::vector<std::uint8_t>& payload = {}) {
    std::vector<std::uint8_t> out{'B', 'C', 'C', 0};
    out.push_back(1);  // version
    out.push_back(0);  // flags
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((rows >> (8 * i)) & 0xFFu));
    }
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((cols >> (8 * i)) & 0xFFu));
    }
    out.push_back(channels);
    out.push_back(depth_bits);
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((block_count >> (8 * i)) & 0xFFu));
    }
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

// The payload of a container that holds one block, placed by hand.
//
// The block is written in the order the format defines: the top left corner,
// the width, the height, and then the four corner samples per channel.
std::vector<std::uint8_t> make_block_payload(int rows, int cols, int channels,
                                             int start_row, int start_column,
                                             int width, int height) {
    const int row_bits = field_bits(rows - 1);
    const int column_bits = field_bits(cols - 1);
    const int extent_bits = field_bits(std::max(rows, cols) - 1);

    BitWriter writer;
    writer.write(static_cast<std::uint64_t>(start_row), row_bits);
    writer.write(static_cast<std::uint64_t>(start_column), column_bits);
    writer.write(static_cast<std::uint64_t>(width - 1), extent_bits);
    writer.write(static_cast<std::uint64_t>(height - 1), extent_bits);
    for (int i = 0; i < channels * 4; ++i) {
        writer.write(0x5A, 8);
    }

    std::vector<std::uint8_t> payload;
    writer.to_bytes(payload);
    return payload;
}

// A flat greyscale image, which the codec compresses into a single block.
cv::Mat make_flat_image(int rows, int cols) {
    return cv::Mat(rows, cols, CV_8UC1, cv::Scalar(120));
}

}  // namespace

TEST(Robustness, RejectsABadMagicNumber) {
    std::vector<std::uint8_t> container =
        make_raw_container(8, 8, 1, 8, 1, make_block_payload(8, 8, 1, 0, 0, 8, 8));
    container[0] = 'X';

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsAnUnknownVersion) {
    std::vector<std::uint8_t> container =
        make_raw_container(8, 8, 1, 8, 1, make_block_payload(8, 8, 1, 0, 0, 8, 8));
    container[4] = 99;

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsAnEmptyImage) {
    const std::vector<std::uint8_t> container = make_raw_container(0, 8, 1, 8, 0);

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsAnUnsupportedChannelCount) {
    const std::vector<std::uint8_t> container = make_raw_container(8, 8, 7, 8, 0);

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsAnUnsupportedDepth) {
    const std::vector<std::uint8_t> container = make_raw_container(8, 8, 1, 7, 0);

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsAnImplausiblePixelCount) {
    const std::vector<std::uint8_t> container =
        make_raw_container(100000, 100000, 1, 8, 0);

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsAPayloadShorterThanTheDeclaredBlocks) {
    // The header declares a thousand blocks, but the payload holds one.
    const std::vector<std::uint8_t> container =
        make_raw_container(32, 32, 1, 8, 1000, make_block_payload(32, 32, 1, 0, 0, 1, 1));

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsABlockOutsideTheImage) {
    // The block starts on the last row and is two rows tall, so it reaches
    // past the bottom of the image.
    const std::vector<std::uint8_t> container =
        make_raw_container(32, 32, 1, 8, 1, make_block_payload(32, 32, 1, 31, 0, 1, 2));

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsABlockOutsideTheImageHorizontally) {
    const std::vector<std::uint8_t> container =
        make_raw_container(32, 32, 1, 8, 1, make_block_payload(32, 32, 1, 0, 31, 2, 1));

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsANonZeroPadding) {
    // A one pixel image holds a single block of 36 bits, so the last byte
    // carries four bits of padding. Setting one of them must be detected.
    ContainerHeader header;
    header.rows = 1;
    header.cols = 1;
    header.channels = 1;
    header.depth_bits = 8;
    header.block_count = 1;

    std::vector<std::uint8_t> container =
        write_container(header, make_block_payload(1, 1, 1, 0, 0, 1, 1));

    // The padding is the low four bits of the last byte.
    ASSERT_FALSE(container.empty());
    container.back() |= 0x01u;

    Codec codec;
    EXPECT_THROW(codec.decompress(container), std::runtime_error);
}

TEST(Robustness, RejectsATruncatedContainer) {
    const cv::Mat source = make_flat_image(32, 32);
    Codec codec;
    const std::vector<std::uint8_t> container = codec.compress(source);

    for (std::size_t cut = 1; cut < container.size(); ++cut) {
        const std::vector<std::uint8_t> truncated(
            container.begin(),
            container.begin() + static_cast<std::ptrdiff_t>(cut));
        EXPECT_THROW(codec.decompress(truncated), std::runtime_error)
            << "cut=" << cut;
    }
}

TEST(Robustness, RejectsACorruptedPayload) {
    const cv::Mat source = make_flat_image(32, 32);
    Codec codec;
    std::vector<std::uint8_t> container = codec.compress(source);

    // The header is 20 bytes long, so the payload is corrupted from there on.
    ASSERT_GT(container.size(), 20u);
    for (std::size_t i = 20; i < container.size(); ++i) {
        container[i] = static_cast<std::uint8_t>(container[i] ^ 0xFF);
    }

    // A corrupted payload may decode into nonsense, but it must never crash
    // or read out of bounds.
    try {
        const cv::Mat decoded = codec.decompress(container);
        EXPECT_EQ(decoded.rows, 32);
        EXPECT_EQ(decoded.cols, 32);
        EXPECT_EQ(decoded.channels(), 1);
    } catch (const std::runtime_error&) {
        SUCCEED() << "the corrupted payload was rejected";
    }
}

TEST(Robustness, RejectsRandomBytes) {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    Codec codec;
    // The trials are bounded, and the decoder is bounded by the size of the
    // container it is given, so the test cannot run away.
    const auto started = std::chrono::steady_clock::now();
    for (int trial = 0; trial < 200; ++trial) {
        std::vector<std::uint8_t> garbage(64);
        for (std::uint8_t& byte : garbage) {
            byte = static_cast<std::uint8_t>(byte_dist(rng));
        }
        // The call must fail cleanly. It must not crash or read out of bounds.
        EXPECT_THROW(codec.decompress(garbage), std::runtime_error)
            << "trial=" << trial;
    }

    // The decoder must terminate. A generous bound is enough: the point is
    // to catch a decoder that loops forever, not to measure its speed.
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    EXPECT_LT(seconds, 30.0);
}

TEST(Robustness, RejectsRandomBytesBehindAValidHeader) {
    // The random bytes of the test above are rejected at the magic number
    // check. This test keeps the header valid, so that the payload of the
    // container is what is under test.
    std::mt19937 rng(6789);
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::uniform_int_distribution<int> block_dist(0, 64);
    std::uniform_int_distribution<int> size_dist(0, 32);

    Codec codec;
    const auto started = std::chrono::steady_clock::now();
    for (int trial = 0; trial < 200; ++trial) {
        const std::uint32_t block_count =
            static_cast<std::uint32_t>(block_dist(rng));
        std::vector<std::uint8_t> payload(static_cast<std::size_t>(size_dist(rng)));
        for (std::uint8_t& byte : payload) {
            byte = static_cast<std::uint8_t>(byte_dist(rng));
        }

        const std::vector<std::uint8_t> container =
            make_raw_container(32, 32, 1, 8, block_count, payload);

        // The container is either rejected or decodes into an image of the
        // declared shape. It must never crash and it must never hang.
        try {
            const cv::Mat decoded = codec.decompress(container);
            EXPECT_EQ(decoded.rows, 32);
            EXPECT_EQ(decoded.cols, 32);
            EXPECT_EQ(decoded.channels(), 1);
        } catch (const std::runtime_error&) {
            SUCCEED() << "the random payload was rejected";
        }
    }

    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    EXPECT_LT(seconds, 30.0);
}

TEST(Robustness, TheWriterNeverEmitsAContainerTheReaderRejects) {
    // Every header the writer accepts is a header the reader accepts.
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    header.depth_bits = 8;
    header.block_count = 0;

    const std::vector<std::uint8_t> container = write_container(header, {0x00});
    std::vector<std::uint8_t> payload;
    EXPECT_NO_THROW(blockcodec::read_container(container, payload));

    // And the headers the reader rejects are refused by the writer.
    header.rows = 0;
    header.cols = 4;
    EXPECT_THROW(write_container(header, {0x00}), std::runtime_error);

    header.rows = 4;
    header.cols = 4;
    header.channels = 7;
    EXPECT_THROW(write_container(header, {0x00}), std::runtime_error);

    header.channels = 1;
    header.depth_bits = 7;
    EXPECT_THROW(write_container(header, {0x00}), std::runtime_error);

    // The format only defines 8 bit samples.
    header.depth_bits = 16;
    EXPECT_THROW(write_container(header, {0x00}), std::runtime_error);

    header.depth_bits = 8;
    header.rows = 100000;
    header.cols = 100000;
    EXPECT_THROW(write_container(header, {0x00}), std::runtime_error);
}

TEST(Robustness, TheCodecRefusesImagesItCannotEncode) {
    Codec codec;
    EXPECT_THROW(codec.compress(cv::Mat()), std::runtime_error);

    cv::Mat deep(16, 16, CV_32F);
    deep.setTo(cv::Scalar(1.0));
    EXPECT_THROW(codec.compress(deep), std::runtime_error);
}
