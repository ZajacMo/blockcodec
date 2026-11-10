#include "blockcodec/bitstream.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using blockcodec::BitReader;
using blockcodec::BitWriter;

TEST(BitStream, WritesBitsMostSignificantFirst) {
    BitWriter w;
    w.write_bit(true);
    w.write_bit(false);
    w.write_bit(true);
    ASSERT_EQ(w.bit_count(), 3u);

    std::vector<std::uint8_t> bytes;
    w.to_bytes(bytes);
    ASSERT_EQ(bytes.size(), 1u);
    // 1010 0000
    EXPECT_EQ(bytes[0], 0xA0u);
}

TEST(BitStream, RoundTripsEveryFieldWidth) {
    for (int bits = 1; bits <= 64; ++bits) {
        const std::uint64_t mask =
            (bits == 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << bits) - 1);

        BitWriter w;
        w.write(0x5A5A5A5A5A5A5A5AULL & mask, bits);
        w.write(mask, bits);

        std::vector<std::uint8_t> bytes;
        w.to_bytes(bytes);

        // The writer zero pads the last byte, so the reader is told how many
        // bits are meaningful. Without that count the padding would look
        // like unread data and `exhausted()` would never become true.
        BitReader r(bytes.data(), bytes.size(), w.bit_count());
        EXPECT_EQ(r.read(bits), 0x5A5A5A5A5A5A5A5AULL & mask) << "bits=" << bits;
        EXPECT_EQ(r.read(bits), mask) << "bits=" << bits;
        EXPECT_TRUE(r.exhausted()) << "bits=" << bits;
    }
}

TEST(BitStream, RoundTripsAcrossByteBoundaries) {
    BitWriter w;
    for (int i = 0; i < 300; ++i) {
        w.write(static_cast<std::uint64_t>(i), 9);
    }

    std::vector<std::uint8_t> bytes;
    w.to_bytes(bytes);

    BitReader r(bytes.data(), bytes.size(), w.bit_count());
    for (int i = 0; i < 300; ++i) {
        EXPECT_EQ(r.read(9), static_cast<std::uint64_t>(i)) << "i=" << i;
    }
    EXPECT_TRUE(r.exhausted());
}

TEST(BitStream, WritesTheFullSixtyFourBitRange) {
    // The legacy implementation evaluated `1 << 63`, which is undefined.
    BitWriter w;
    w.write(0xFFFFFFFFFFFFFFFFULL, 64);

    std::vector<std::uint8_t> bytes;
    w.to_bytes(bytes);
    ASSERT_EQ(bytes.size(), 8u);

    BitReader r(bytes.data(), bytes.size());
    EXPECT_EQ(r.read(64), 0xFFFFFFFFFFFFFFFFULL);
}

TEST(BitStream, ReadingPastTheEndThrows) {
    const std::vector<std::uint8_t> bytes{0xFF};
    BitReader r(bytes.data(), bytes.size());
    EXPECT_THROW(r.read(9), std::runtime_error);
}

TEST(BitStream, ZeroBitCountIsRejected) {
    BitWriter w;
    EXPECT_THROW(w.write(1, 0), std::runtime_error);
    EXPECT_THROW(w.write(1, 65), std::runtime_error);
}
