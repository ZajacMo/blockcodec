#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace blockcodec {

// Writes a sequence of bits, most significant bit first, into a byte buffer.
//
// The legacy implementation stored one ASCII character per bit, which made
// the file eight times larger than the bitstream it carried. This writer
// packs eight bits to a byte.
class BitWriter {
public:
    // Appends the low `bits` bits of `value`.
    // Throws std::runtime_error if `bits` is not in [1, 64].
    void write(std::uint64_t value, int bits);

    void write_bit(bool bit);

    std::size_t bit_count() const { return bits_.size(); }

    // Appends the written bits to `out`, packed eight to a byte.
    // The final byte is zero padded.
    void to_bytes(std::vector<std::uint8_t>& out) const;

private:
    std::vector<bool> bits_;
};

// Reads a sequence of bits written by BitWriter.
class BitReader {
public:
    // Reads the bit stream held in `data`.
    //
    // `bit_count` is the number of meaningful bits in the buffer. Zero means
    // "all of them", that is `size * 8`. A writer that did not fill the
    // final byte passes the exact count so that the zero padding of that
    // byte is not mistaken for data.
    //
    // Note that a decoder which must verify that the padding is zero wants
    // the default: it needs to read the padding bits, which only the
    // `size * 8` count exposes.
    explicit BitReader(const std::uint8_t* data, std::size_t size,
                       std::size_t bit_count = 0);

    // Reads `bits` bits and returns them as an unsigned integer.
    // Throws std::runtime_error if fewer than `bits` bits remain.
    std::uint64_t read(int bits);

    bool read_bit();

    std::size_t bit_position() const { return position_; }

    // The total number of readable bits.
    std::size_t bit_count() const { return bit_count_; }

    bool exhausted() const { return position_ >= bit_count_; }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t bit_count_ = 0;
    std::size_t position_ = 0;
};

}  // namespace blockcodec
