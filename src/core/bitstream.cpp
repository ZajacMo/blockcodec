#include "core/bitstream.hpp"

#include <stdexcept>

namespace blockcodec {

void BitWriter::write(std::uint64_t value, int bits) {
    if (bits < 1 || bits > 64) {
        throw std::runtime_error("BitWriter::write: the field width must be in [1, 64]");
    }

    // Shift within the width of the type. The legacy implementation used
    // `1 << i` on an int, which is undefined once i reaches 31.
    const std::uint64_t mask =
        (bits == 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << bits) - 1);
    value &= mask;

    for (int i = bits - 1; i >= 0; --i) {
        bits_.push_back(((value >> i) & 1u) != 0);
    }
}

void BitWriter::write_bit(bool bit) {
    bits_.push_back(bit);
}

void BitWriter::to_bytes(std::vector<std::uint8_t>& out) const {
    std::size_t whole_bytes = bits_.size() / 8;
    std::size_t remainder = bits_.size() % 8;

    out.reserve(out.size() + whole_bytes + (remainder ? 1 : 0));

    for (std::size_t i = 0; i < whole_bytes; ++i) {
        std::uint8_t byte = 0;
        for (int b = 0; b < 8; ++b) {
            if (bits_[i * 8 + static_cast<std::size_t>(b)]) {
                byte |= static_cast<std::uint8_t>(1u << (7 - b));
            }
        }
        out.push_back(byte);
    }

    if (remainder != 0) {
        std::uint8_t byte = 0;
        for (std::size_t b = 0; b < remainder; ++b) {
            if (bits_[whole_bytes * 8 + b]) {
                byte |= static_cast<std::uint8_t>(1u << (7 - static_cast<int>(b)));
            }
        }
        out.push_back(byte);
    }
}

BitReader::BitReader(const std::uint8_t* data, std::size_t size, std::size_t bit_count)
    : data_(data),
      size_(data == nullptr ? 0 : size),
      bit_count_(bit_count == 0 ? size_ * 8 : bit_count),
      position_(0) {
    if (bit_count_ > size_ * 8) {
        throw std::runtime_error("BitReader: the bit count exceeds the size of the buffer");
    }
}

std::uint64_t BitReader::read(int bits) {
    if (bits < 1 || bits > 64) {
        throw std::runtime_error("BitReader::read: the field width must be in [1, 64]");
    }
    if (bit_count_ - position_ < static_cast<std::size_t>(bits)) {
        throw std::runtime_error("BitReader::read: the stream ended unexpectedly");
    }

    std::uint64_t value = 0;
    for (int i = 0; i < bits; ++i) {
        const std::size_t index = position_ + static_cast<std::size_t>(i);
        const std::uint8_t byte = data_[index / 8];
        const std::uint8_t bit = (byte >> (7 - static_cast<int>(index % 8))) & 1u;
        value = (value << 1) | static_cast<std::uint64_t>(bit);
    }
    position_ += static_cast<std::size_t>(bits);
    return value;
}

bool BitReader::read_bit() {
    return read(1) != 0;
}

}  // namespace blockcodec
