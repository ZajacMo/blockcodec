#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

// Exercises the blockcodec command line end to end: compress an image,
// print the header of the container, and decompress it again.
//
// The test drives the executable rather than the library, so it also covers
// the argument handling and the error reporting.

namespace {

int failures = 0;
int checks = 0;

void check(bool condition, const std::string& what) {
    ++checks;
    if (condition) {
        std::printf("  [ OK ] %s\n", what.c_str());
    } else {
        std::printf("  [FAIL] %s\n", what.c_str());
        ++failures;
    }
}

// The path of the executable, passed in by the build system.
std::string executable;

// Runs the executable and returns its exit code.
int run(const std::string& arguments) {
    const std::string command = "\"" + executable + "\" " + arguments + " > NUL 2>&1";
    return std::system(command.c_str());
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
    if (const char* path = std::getenv("BLOCKCODEC_CLI")) {
        executable = path;
    } else {
        std::printf("BLOCKCODEC_CLI is not set\n");
        return 1;
    }

    std::printf("Blockcodec command line\n");
    std::printf("======================\n");

    const std::string container = "blockcodec_cli_test.bcc";
    const std::string output = "blockcodec_cli_test.png";

    // Compress.
    check(run("compress images/100gray.bmp -o " + container + " --sigma 10") == 0,
          "compress exits with 0");
    check(!read_file(container).empty(), "the container was written");

    // The header of the container describes the image.
    check(run("info " + container) == 0, "info exits with 0");

    // Decompress.
    check(run("decompress " + container + " -o " + output) == 0,
          "decompress exits with 0");
    check(!read_file(output).empty(), "the image was written");

    // Comparing against the source reports the quality.
    check(run("decompress " + container + " -o " + output +
              " --compare images/100gray.bmp") == 0,
          "decompress with a comparison exits with 0");

    // A missing file is an error, not a crash.
    check(run("info no_such_file.bcc") != 0, "a missing file is an error");

    // A file that is not a container is an error.
    check(run("info images/100gray.bmp") != 0, "a file that is not a container is an error");

    // An unknown command is an error.
    check(run("nonsense") != 0, "an unknown command is an error");

    std::remove(container.c_str());
    std::remove(output.c_str());

    std::printf("\n======================\n");
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
