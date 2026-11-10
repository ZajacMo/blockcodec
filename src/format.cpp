#include "blockcodec/format.hpp"

#include <cstring>
#include <stdexcept>

namespace blockcodec {
namespace {

constexpr char kMagic[4] = {'B', 'C', 'C', '\0'};

// magic(4) + version(1) + flags(1) + rows(4) + cols(4) + channels(1) +
// depth_bits(1) + block_count(4)
constexpr std::size_t kHeaderSize = 20;

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

std::uint32_t read_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::vector<std::uint8_t> write_container(const ContainerHeader& header,
                                           const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSize + payload.size());

    for (char c : kMagic) {
        out.push_back(static_cast<std::uint8_t>(c));
    }
    out.push_back(kFormatVersion);
    out.push_back(header.colour ? 1u : 0u);
    append_u32(out, header.rows);
    append_u32(out, header.cols);
    out.push_back(header.channels);
    out.push_back(header.depth_bits);
    append_u32(out, header.block_count);

    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

ContainerHeader read_container(const std::vector<std::uint8_t>& data,
                                std::vector<std::uint8_t>& payload) {
    if (data.size() < kHeaderSize) {
        throw std::runtime_error("blockcodec: the container is truncated");
    }
    if (std::memcmp(data.data(), kMagic, sizeof(kMagic)) != 0) {
        throw std::runtime_error("blockcodec: the container has a bad magic number");
    }
    if (data[4] != kFormatVersion) {
        throw std::runtime_error("blockcodec: unsupported container version");
    }

    ContainerHeader header;
    header.colour = (data[5] & 1u) != 0;
    header.rows = read_u32(data.data() + 6);
    header.cols = read_u32(data.data() + 10);
    header.channels = data[14];
    header.depth_bits = data[15];
    header.block_count = read_u32(data.data() + 16);

    if (header.rows == 0 || header.cols == 0) {
        throw std::runtime_error("blockcodec: the container declares an empty image");
    }
    if (header.channels < 1 || header.channels > 4) {
        throw std::runtime_error("blockcodec: the container declares an unsupported channel count");
    }
    if (header.depth_bits != 8 && header.depth_bits != 16 && header.depth_bits != 32) {
        throw std::runtime_error("blockcodec: the container declares an unsupported colour depth");
    }

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(header.rows) * static_cast<std::uint64_t>(header.cols);
    if (pixels > kMaxPixels) {
        throw std::runtime_error("blockcodec: the container declares an implausible image size");
    }

    payload.assign(data.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), data.end());
    return header;
}

}  // namespace blockcodec
