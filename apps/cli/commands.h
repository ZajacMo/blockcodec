#pragma once

#include <string>

namespace blockcodec {
namespace cli {

// The exit codes of the blockcodec executable.
//
// The help is a successful completion and exits with zero. A usage error
// exits with 2 and a runtime error with 1, so that a script can tell a
// mistake in the command line from a failure of the operation.
constexpr int kExitSuccess = 0;
constexpr int kExitRuntimeError = 1;
constexpr int kExitUsageError = 2;

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
