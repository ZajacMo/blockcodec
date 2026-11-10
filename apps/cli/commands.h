#pragma once

#include <string>

namespace blockcodec {
namespace cli {

// blockcodec compress <input> -o <output> [--sigma N] [--quiet]
int run_compress(const std::string& input, const std::string& output, int sigma,
                 bool quiet);

// blockcodec decompress <input> -o <output> [--compare <reference>]
int run_decompress(const std::string& input, const std::string& output,
                   const std::string& compare);

// blockcodec info <input>
int run_info(const std::string& input);

}  // namespace cli
}  // namespace blockcodec
