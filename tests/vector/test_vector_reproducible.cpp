// Checks that the reference vectors are reproducible.
//
// The vectors in `tests/vectors/` are what an independent implementation is
// compared against, so they must not depend on the compiler that produced
// them. If a compiler rounds differently, the vectors have to change with
// it, and this test fails until they are regenerated and committed.
//
// What this test guards is that the **vectors agree with the encoder as it
// stands**. It is not a check that the encoder has not changed. The
// vectors are insensitive to a change in the block search: their three
// pictures compress to the same container at every tolerance from 8 to 12,
// because their blocks are chosen the same way whatever the tolerance is.
// So an encoder whose block search changed leaves this test passing, and
// the vectors are regenerated and committed with that change. That is the
// intended behaviour: the vectors belong to the format, and a change of
// the format changes them.
//
// The workflow adds the other half of the check: it regenerates the
// vectors and asks git whether the files in the repository changed. That
// catches a vector that was edited by hand or that was left behind when
// the encoder changed.

#include "blockcodec/codec.hpp"

#include <gtest/gtest.h>

#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

// The directory the repository keeps its vectors in.
std::string vector_directory() {
#ifdef BLOCKCODEC_VECTOR_DIR
    return BLOCKCODEC_VECTOR_DIR;
#else
    return "tests/vectors";
#endif
}

// The names of the vectors, without their extension.
const char* const kVectorNames[] = {
    "gradient_16x16",
    "gradient_16x16_colour",
    "truncation_3x16",
};

// Reads a file whole.
std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

// Builds the same pictures the generator builds, and checks that each one
// compresses to the container in the repository.
//
// This is the property that matters: the containers an independent
// implementation is handed must be the same whoever produced them.
void check_vector_is_reproducible(const std::string& name) {
    const std::string directory = vector_directory();

    // The container in the repository.
    const std::vector<std::uint8_t> expected =
        read_file(directory + "/" + name + ".bcc");
    ASSERT_FALSE(expected.empty()) << "cannot read " << name << ".bcc";

    // The source image in the repository.
    const cv::Mat source = cv::imread(directory + "/" + name + "_source.bmp",
                                      cv::IMREAD_UNCHANGED);
    ASSERT_FALSE(source.empty()) << "cannot read " << name << "_source.bmp";

    // Compressing the source again must produce the same container.
    blockcodec::Codec codec({10});
    const std::vector<std::uint8_t> produced = codec.compress(source, nullptr);

    ASSERT_EQ(produced.size(), expected.size())
        << name << ": the regenerated container has a different length";
    EXPECT_EQ(produced, expected)
        << name << ": the container in the repository does not match what the "
                   "encoder produces now. If the encoder changed on purpose, "
                   "regenerate the vectors and commit them with the change.";
}

}  // namespace

TEST(Vector, IsReproducibleFromItsSource) {
    for (const char* name : kVectorNames) {
        check_vector_is_reproducible(name);
    }
}

