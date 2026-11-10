// Checks that the benchmark runs and writes the CSV it promises.
//
// The test drives the executable rather than the library, so that the
// argument handling, the table and the file writing are covered together.

#include <gtest/gtest.h>

#include "blockcodec/codec.hpp"

#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// The path of the benchmark executable, passed in by the build system.
std::string executable;

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// The first line of a file, without the line ending.
std::string first_line(const std::string& path) {
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    return line;
}

// The fields of a comma separated line.
std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator)) {
        parts.push_back(part);
    }
    return parts;
}

// Runs the benchmark with the given arguments and returns its exit code.
//
// The output of the benchmark is discarded, so that a passing test run does
// not fill the log with the tables.
int run(const std::string& arguments) {
    const std::string command = "\"" + executable + "\" " + arguments + " > NUL 2>&1";
    return std::system(command.c_str());
}

// Runs the benchmark and returns what it printed.
//
// The log file is not quoted, because the command interpreter strips the
// quotes of the executable path and then fails to find the file, which has
// no spaces in it anyway.
std::string run_capturing(const std::string& arguments) {
    const std::string log = "bench_test_output.txt";
    const std::string command =
        "\"" + executable + "\" " + arguments + " > " + log + " 2>&1";
    const int status = std::system(command.c_str());
    EXPECT_EQ(status, 0);
    const std::string output = read_file(log);
    std::remove(log.c_str());
    return output;
}

class Bench : public ::testing::Test {
protected:
    void SetUp() override {
        const char* path = std::getenv("BLOCKCODEC_BENCH");
        ASSERT_NE(path, nullptr) << "BLOCKCODEC_BENCH is not set";
        executable = path;
    }

    void TearDown() override {
        std::remove("bench_test.csv");
        std::remove("bench_test_jpeg.csv");
        std::remove("bench_test_matched.csv");
    }
};

TEST_F(Bench, RunsOverASingleImageAndWritesTheCsv) {
    const int status = run("--images images/100gray.bmp --sigmas 10 "
                            "--csv bench_test.csv");
    EXPECT_EQ(status, 0);

    ASSERT_TRUE(read_file("bench_test.csv").size() > 0)
        << "the CSV was not written";

    // The header names the columns the table is printed with.
    EXPECT_EQ(first_line("bench_test.csv"),
              "image,rows,cols,channels,sigma,bytes,bpp,cr,psnr,encode_ms,"
              "decode_ms,blocks,payload_bits,payload_bpp,payload_cr");

    // The header is followed by one row for the one image and the one sigma.
    std::ifstream in("bench_test.csv");
    std::string line;
    std::getline(in, line); // the header
    ASSERT_TRUE(static_cast<bool>(std::getline(in, line)));
    EXPECT_NE(line.find("100gray.bmp"), std::string::npos);
    EXPECT_NE(line.find(",10,"), std::string::npos);
}

TEST_F(Bench, WritesTheJpegBaselineAndTheMatchedTable) {
    const int status = run("--images images/100gray.bmp --sigmas 10 "
                            "--baseline jpeg --csv bench_test.csv");
    EXPECT_EQ(status, 0);

    ASSERT_TRUE(read_file("bench_test_jpeg.csv").size() > 0)
        << "the JPEG CSV was not written";
    EXPECT_EQ(first_line("bench_test_jpeg.csv"),
              "image,rows,cols,channels,quality,bytes,bpp,cr,psnr,encode_ms,"
              "decode_ms");

    ASSERT_TRUE(read_file("bench_test_matched.csv").size() > 0)
        << "the matched CSV was not written";
    EXPECT_EQ(first_line("bench_test_matched.csv"),
              "image,sigma,bc_bytes,bc_bpp,bc_psnr,jpeg_quality,jpeg_bytes,"
              "jpeg_bpp,jpeg_psnr,psnr_delta,jpeg_reached,match,smaller");
}

TEST_F(Bench, PrintsTheBlockcodecAndTheJpegTables) {
    const std::string output = run_capturing("--images images/100gray.bmp "
                                             "--sigmas 10 --baseline jpeg");

    // The blockcodec table carries the sizes and the quality.
    EXPECT_NE(output.find("blockcodec"), std::string::npos);
    EXPECT_NE(output.find("file_bytes"), std::string::npos);
    EXPECT_NE(output.find("100gray.bmp"), std::string::npos);

    // The JPEG table is printed as well, with the same units.
    EXPECT_NE(output.find("JPEG baseline"), std::string::npos);
    EXPECT_NE(output.find("JPEG at the matched quality"), std::string::npos);
}

// The matched table compares the two codecs at the same quality, so a row
// that says the JPEG quality reached the blockcodec PSNR has to show a JPEG
// PSNR that is not below the blockcodec one.
TEST_F(Bench, TheMatchedRowOnlyComparesAtTheSameQuality) {
    ASSERT_EQ(run("--images images/100gray.bmp --sigmas 10 --baseline jpeg "
                  "--csv bench_test.csv"),
              0);

    std::ifstream in("bench_test_matched.csv");
    std::string line;
    ASSERT_TRUE(static_cast<bool>(std::getline(in, line)));  // the header

    int rows = 0;
    while (std::getline(in, line)) {
        const std::vector<std::string> fields = split(line, ',');
        ASSERT_EQ(fields.size(), 13u);

        const double bc_psnr = std::atof(fields[4].c_str());
        const double jpeg_psnr = std::atof(fields[8].c_str());
        const std::string& match = fields[11];
        const std::string& smaller = fields[12];

        EXPECT_TRUE(match == "reached" || match == "short");

        if (match == "reached") {
            // The JPEG quality reaches the blockcodec PSNR, so the sizes can
            // be compared.
            EXPECT_GE(jpeg_psnr, bc_psnr);
            EXPECT_TRUE(smaller == "jpeg" || smaller == "blockcodec" ||
                        smaller == "equal");
        } else {
            // The JPEG quality does not reach it, so no winner is named.
            EXPECT_EQ(smaller, "-");
        }
        ++rows;
    }
    EXPECT_GT(rows, 0);
}

// The benchmark compares the size of the file that is written, not the size
// of the bitstream. The reported byte count must therefore be the size of the
// container that the library produces, and the bits per pixel must follow
// from it.
TEST_F(Bench, ReportsTheSizeOfTheContainerThatIsWritten) {
    const int sigma = 10;
    ASSERT_EQ(run("--images images/100gray.bmp --sigmas 10 --csv bench_test.csv"),
              0);

    const cv::Mat source = cv::imread("images/100gray.bmp", cv::IMREAD_UNCHANGED);
    ASSERT_FALSE(source.empty());

    blockcodec::Codec codec({sigma});
    blockcodec::CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    std::ifstream in("bench_test.csv");
    std::string line;
    ASSERT_TRUE(static_cast<bool>(std::getline(in, line)));  // the header
    ASSERT_TRUE(static_cast<bool>(std::getline(in, line)));

    const std::vector<std::string> fields = split(line, ',');
    ASSERT_EQ(fields.size(), 15u);

    // The reported size is the size of the file, not the size of the
    // bitstream.
    EXPECT_EQ(std::atol(fields[5].c_str()),
              static_cast<long>(container.size()));
    EXPECT_NE(result.bit_size / 8, container.size());

    // The bits per pixel follow from the file size.
    const double pixels = static_cast<double>(source.rows) * source.cols;
    EXPECT_NEAR(std::atof(fields[6].c_str()),
                static_cast<double>(container.size()) * 8.0 / pixels, 1e-6);
}

// The matched table compares two file sizes, so the one it reports for the
// blockcodec container has to be the size of the file as well.
TEST_F(Bench, TheMatchedTableReportsTheSizeOfTheContainer) {
    const int sigma = 10;
    ASSERT_EQ(run("--images images/100gray.bmp --sigmas 10 --baseline jpeg "
                  "--csv bench_test.csv"),
              0);

    const cv::Mat source = cv::imread("images/100gray.bmp", cv::IMREAD_UNCHANGED);
    ASSERT_FALSE(source.empty());

    blockcodec::Codec codec({sigma});
    blockcodec::CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    std::ifstream in("bench_test_matched.csv");
    std::string line;
    ASSERT_TRUE(static_cast<bool>(std::getline(in, line)));  // the header
    ASSERT_TRUE(static_cast<bool>(std::getline(in, line)));

    const std::vector<std::string> fields = split(line, ',');
    ASSERT_EQ(fields.size(), 13u);

    EXPECT_EQ(std::atol(fields[2].c_str()), static_cast<long>(container.size()));
}

TEST_F(Bench, ReportsAMissingImageDirectory) {
    EXPECT_NE(run("--images no_such_directory --sigmas 10"), 0);
}

}  // namespace
