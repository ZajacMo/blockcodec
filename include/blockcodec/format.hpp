#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace blockcodec {

// The version of the .bcc container written by this library.
constexpr std::uint8_t kFormatVersion = 1;

// The largest number of pixels a container may declare. The dimensions come
// from the file, so they are validated before they size an allocation.
constexpr std::uint64_t kMaxPixels = 1ULL << 28;

// The header of a .bcc container.
//
// The channel count is stored explicitly. The legacy format inferred it from
// `cols > rows`, which misreported every image wider than it is tall.
struct ContainerHeader {
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;        // pixel width, excluding channels
    std::uint32_t block_count = 0; // number of blocks in the payload
    std::uint8_t  channels = 1;
    std::uint8_t  depth_bits = 8;  // bits per colour sample
    bool          colour = false;
};

// Builds a complete container: the header followed by the packed payload.
std::vector<std::uint8_t> write_container(const ContainerHeader& header,
                                           const std::vector<std::uint8_t>& payload);

// Parses a container. Throws std::runtime_error if the container is
// malformed, truncated, or declares implausible dimensions.
ContainerHeader read_container(const std::vector<std::uint8_t>& data,
                                std::vector<std::uint8_t>& payload);

}  // namespace blockcodec
