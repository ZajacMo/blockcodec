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
//
// The output of the command is discarded. The device that discards it is
// called NUL on Windows and /dev/null everywhere else, and using the
// wrong one would create a file named NUL in the working directory.
int run(const std::string& arguments) {
#ifdef _WIN32
    const std::string null_device = "NUL";
#else
    const std::string null_device = "/dev/null";
#endif
    const std::string command =
        "\"" + executable + "\" " + arguments + " > " + null_device + " 2>&1";
    return std::system(command.c_str());
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// Runs the executable and returns what it printed.
std::string run_capturing(const std::string& arguments) {
    const std::string command = "\"" + executable + "\" " + arguments;
#ifdef _WIN32
    std::FILE* pipe = _popen(command.c_str(), "r");
#else
    std::FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return "";
    }
    std::string output;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return output;
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

    // Asking for the help is a successful completion, so it exits with
    // zero. A script that runs `blockcodec --help` and checks the exit
    // status must not see a failure.
    check(run("--help") == 0, "--help exits with 0");
    check(run("-h") == 0, "-h exits with 0");

    // The dimensions are printed as width by height, the order the image
    // formats use. The test image is square, so it cannot tell the two
    // orders apart; the landscape greyscale image is 256 pixels wide and
    // 128 pixels tall, so it can.
    const std::string landscape_container = "blockcodec_cli_landscape.bcc";
    check(run("compress images/landscape_grey.bmp -o " + landscape_container +
              " --sigma 10") == 0,
          "compressing a landscape image exits with 0");

    const std::string info = run_capturing("info " + landscape_container);
    check(info.find("dimensions  256 x 128") != std::string::npos,
          "info prints the dimensions as width by height");
    check(info.find("channels    1") != std::string::npos,
          "info reports the channel count of a greyscale image as one");

    std::remove(container.c_str());
    std::remove(output.c_str());
    std::remove(landscape_container.c_str());

    std::printf("\n======================\n");
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
