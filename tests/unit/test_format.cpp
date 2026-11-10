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

TEST(Format, RejectsImplausibleDimensions) {
    ContainerHeader header;
    header.rows = 100000;
    header.cols = 100000;
    std::vector<std::uint8_t> container = write_container(header, {0x00});

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsAnUnsupportedChannelCount) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    header.channels = 7;
    std::vector<std::uint8_t> container = write_container(header, {0x00});

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
