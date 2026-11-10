#include "cli/commands.h"

#include <CLI/CLI.hpp>

#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    CLI::App app{"blockcodec, a block based image compression codec"};
    app.require_subcommand(1);

    int sigma = 10;
    bool quiet = false;
    std::string compare;

    // The options that take a variable are read back from the subcommand
    // that owns them, not from the parent application.
    std::string compress_input;
    std::string compress_output;
    std::string decompress_input;
    std::string decompress_output;
    std::string info_input;

    auto* compress = app.add_subcommand("compress", "Compress an image");
    compress->add_option("input", compress_input, "The image to compress")
        ->required();
    compress->add_option("-o,--output", compress_output, "The container to write")
        ->required();
    compress->add_option("-s,--sigma", sigma, "The tolerance of the block test");
    compress->add_flag("-q,--quiet", quiet, "Print nothing");

    auto* decompress = app.add_subcommand("decompress", "Decompress a container");
    decompress->add_option("input", decompress_input, "The container to read")
        ->required();
    decompress->add_option("-o,--output", decompress_output, "The image to write")
        ->required();
    decompress->add_option("--compare", compare, "Compare against this image");

    auto* info = app.add_subcommand("info", "Print the header of a container");
    info->add_option("input", info_input, "The container to read")->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        // A usage error is not a runtime failure, so it gets its own exit
        // code rather than the one the runtime errors use.
        //
        // The message is printed by CLI11. The exit code is not taken from
        // it, because asking for the help is a successful completion and
        // CLI11 gives it the code zero: `blockcodec --help` has to exit
        // with zero, or a script that checks the exit status sees a
        // failure where there was none.
        app.exit(e);
        return e.get_exit_code() == 0 ? blockcodec::cli::kExitSuccess
                                       : blockcodec::cli::kExitUsageError;
    }

    try {
        if (app.got_subcommand(compress)) {
            return blockcodec::cli::run_compress(compress_input, compress_output,
                                                 sigma, quiet);
        }
        if (app.got_subcommand(decompress)) {
            return blockcodec::cli::run_decompress(decompress_input, decompress_output,
                                                   compare);
        }
        if (app.got_subcommand(info)) {
            return blockcodec::cli::run_info(info_input);
        }
    } catch (const std::exception& e) {
        std::cerr << "blockcodec: " << e.what() << "\n";
        return blockcodec::cli::kExitRuntimeError;
    }

    std::cerr << "blockcodec: no command given\n";
    return blockcodec::cli::kExitUsageError;
}
