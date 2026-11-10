#include "blockcodec/format.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using blockcodec::ContainerHeader;
using blockcodec::read_container;
using blockcodec::write_container;

TEST(Format, RoundTripsTheHeader) {
    ContainerHeader header;
    header.rows = 512;
    header.cols = 512;
    header.channels = 3;
    header.depth_bits = 8;
    header.colour = true;
    header.block_count = 42;

    const std::vector<std::uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF};
    const std::vector<std::uint8_t> container = write_container(header, payload);

    std::vector<std::uint8_t> decoded_payload;
    const ContainerHeader decoded = read_container(container, decoded_payload);

    EXPECT_EQ(decoded.rows, 512u);
    EXPECT_EQ(decoded.cols, 512u);
    EXPECT_EQ(decoded.channels, 3u);
    EXPECT_EQ(decoded.depth_bits, 8u);
    EXPECT_TRUE(decoded.colour);
    EXPECT_EQ(decoded.block_count, 42u);
    EXPECT_EQ(decoded_payload, payload);
}

TEST(Format, RejectsAnEmptyContainer) {
    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container({}, payload), std::runtime_error);
}

TEST(Format, RejectsABadMagic) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    std::vector<std::uint8_t> container = write_container(header, {0x00});
    container[0] = 'X';

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsAnUnknownVersion) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    std::vector<std::uint8_t> container = write_container(header, {0x00});
    container[4] = 99;

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

namespace {

// Builds a container byte by byte, so that a malformed header can be
// produced. The writer refuses to emit one, and a malformed container
// reaches the reader from outside anyway.
std::vector<std::uint8_t> make_raw_container(std::uint32_t rows,
                                              std::uint32_t cols,
                                              std::uint8_t channels,
                                              std::uint8_t depth_bits) {
    std::vector<std::uint8_t> out{'B', 'C', 'C', 0};
    out.push_back(1);              // version
    out.push_back(0);              // flags
    out.push_back(static_cast<std::uint8_t>(rows & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((rows >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((rows >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((rows >> 24) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(cols & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((cols >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((cols >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((cols >> 24) & 0xFFu));
    out.push_back(channels);
    out.push_back(depth_bits);
    out.push_back(0);              // block count
    out.push_back(0);
    out.push_back(0);
    out.push_back(0);
    out.push_back(0);              // payload
    return out;
}

}  // namespace

TEST(Format, RejectsImplausibleDimensions) {
    const std::vector<std::uint8_t> container =
        make_raw_container(100000, 100000, 1, 8);

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsAnUnsupportedChannelCount) {
    const std::vector<std::uint8_t> container = make_raw_container(4, 4, 7, 8);

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsAnEmptyImage) {
    const std::vector<std::uint8_t> container = make_raw_container(0, 4, 1, 8);

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsAnUnsupportedDepth) {
    const std::vector<std::uint8_t> container = make_raw_container(4, 4, 1, 7);

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsATruncatedContainer) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    std::vector<std::uint8_t> container = write_container(header, {0x00});

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(std::vector<std::uint8_t>(container.begin(),
                                                          container.begin() + 8),
                                 payload),
                 std::runtime_error);
}

TEST(Format, RejectsAnInvalidHeaderOnWrite) {
    // A writer must never produce a container that the reader would reject.
    ContainerHeader header;
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

    header.depth_bits = 8;
    header.rows = 100000;
    header.cols = 100000;
    EXPECT_THROW(write_container(header, {0x00}), std::runtime_error);
}

TEST(Format, AcceptsOnlyTheEightBitDepth) {
    // The payload always carries 8 bit samples, so a container that declares
    // a deeper sample would be read at the wrong width.
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    header.depth_bits = 8;
    EXPECT_NO_THROW(write_container(header, {0x00}));

    for (const std::uint8_t depth : {16, 32, 24, 0}) {
        header.depth_bits = depth;
        EXPECT_THROW(write_container(header, {0x00}), std::runtime_error)
            << "depth=" << depth;
    }
}

TEST(Format, RejectsADeepSampleOnRead) {
    // The same rule on the reading side: a container that declares 16 bit
    // samples is rejected rather than decoded at the wrong width.
    const std::vector<std::uint8_t> container = make_raw_container(4, 4, 1, 16);

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}
