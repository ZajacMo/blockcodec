#include "cli/commands.h"

#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"
#include "blockcodec/metrics.hpp"

#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace blockcodec {
namespace cli {
namespace {

// Writes `data` to `path`. Throws std::runtime_error on failure.
void write_file(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot open '" + path + "' for writing");
    }
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    if (!out) {
        throw std::runtime_error("failed to write '" + path + "'");
    }
}

// Reads a whole file. Throws std::runtime_error on failure.
std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open '" + path + "' for reading");
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

// Prints the measurements of an operation.
void report(const CompressResult& result, const std::string& path) {
    std::printf("wrote %s\n", path.c_str());
    std::printf("  bits        %zu\n", result.bit_size);
    std::printf("  bytes       %zu\n", result.byte_size);
    std::printf("  bpp         %.6f\n", result.bpp);
    std::printf("  cr          %.6f\n", result.cr);
    std::printf("  psnr        %.4f dB\n", result.psnr);
    std::printf("  blocks      %zu\n", result.block_count);
    std::printf("  encode      %.2f ms\n", result.compress_ms);
}

}  // namespace

int run_compress(const std::string& input, const std::string& output, int sigma,
                 bool quiet) {
    const cv::Mat source = cv::imread(input, cv::IMREAD_UNCHANGED);
    if (source.empty()) {
        throw std::runtime_error("cannot read the image '" + input + "'");
    }

    Codec codec({sigma});
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    write_file(output, container);

    if (!quiet) {
        report(result, output);
    }
    return 0;
}

int run_decompress(const std::string& input, const std::string& output,
                   const std::string& compare) {
    const std::vector<std::uint8_t> container = read_file(input);

    Codec codec;
    CompressResult result;
    const cv::Mat decoded = codec.decompress(container, &result);

    if (!cv::imwrite(output, decoded)) {
        throw std::runtime_error("cannot write the image '" + output + "'");
    }

    std::printf("wrote %s (%dx%d, %d channels)\n", output.c_str(), decoded.cols,
                decoded.rows, decoded.channels());
    std::printf("  blocks      %zu\n", result.block_count);
    std::printf("  decode      %.2f ms\n", result.decompress_ms);

    if (!compare.empty()) {
        const cv::Mat reference = cv::imread(compare, cv::IMREAD_UNCHANGED);
        if (reference.empty()) {
            throw std::runtime_error("cannot read the image '" + compare + "'");
        }
        if (reference.rows != decoded.rows || reference.cols != decoded.cols ||
            reference.channels() != decoded.channels()) {
            throw std::runtime_error(
                "the reference '" + compare + "' is " +
                std::to_string(reference.cols) + "x" +
                std::to_string(reference.rows) + " with " +
                std::to_string(reference.channels()) + " channels, but the " +
                "decoded image is " + std::to_string(decoded.cols) + "x" +
                std::to_string(decoded.rows) + " with " +
                std::to_string(decoded.channels()) + " channels");
        }
        std::printf("  psnr        %.4f dB\n", psnr(reference, decoded));
    }
    return 0;
}

int run_info(const std::string& input) {
    const std::vector<std::uint8_t> container = read_file(input);

    std::vector<std::uint8_t> payload;
    const ContainerHeader header = read_container(container, payload);

    std::printf("file          %s\n", input.c_str());
    std::printf("  version     %u\n", static_cast<unsigned>(kFormatVersion));
    std::printf("  dimensions  %u x %u\n", header.rows, header.cols);
    std::printf("  channels    %u\n", static_cast<unsigned>(header.channels));
    std::printf("  depth       %u bits\n", static_cast<unsigned>(header.depth_bits));
    std::printf("  blocks      %u\n", header.block_count);
    std::printf("  payload     %zu bytes\n", payload.size());
    return 0;
}

}  // namespace cli
}  // namespace blockcodec
