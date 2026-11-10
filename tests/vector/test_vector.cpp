// Checks the reference vectors of the .bcc container.
//
// A vector is a container and the image it decodes to. The test decodes the
// container and compares it with the reference image pixel for pixel, with
// no tolerance.
//
// The vectors are what an independent implementation is checked against, so
// they must decode to exactly the same pixels on every implementation. The
// test also writes the decoded image next to the reference, so that a
// mismatch can be looked at rather than only counted.

#include "blockcodec/codec.hpp"

#include <gtest/gtest.h>

#include <opencv2/imgcodecs.hpp>

#include <fstream>
#include <string>
#include <vector>

namespace {

std::string vector_directory() {
#ifdef BLOCKCODEC_VECTOR_DIR
    return BLOCKCODEC_VECTOR_DIR;
#else
    return "tests/vectors";
#endif
}

// Checks one vector.
void check_vector(const std::string& name) {
    const std::string dir = vector_directory();
    const std::string containerPath = dir + "/" + name + ".bcc";
    const std::string referencePath = dir + "/" + name + "_reference.bmp";

    const cv::Mat reference = cv::imread(referencePath, cv::IMREAD_UNCHANGED);
    ASSERT_FALSE(reference.empty()) << "cannot read " << referencePath;

    blockcodec::Codec codec;
    blockcodec::CompressResult result;
    const cv::Mat decoded = codec.decompress(containerPath, &result);

    ASSERT_EQ(decoded.rows, reference.rows) << name;
    ASSERT_EQ(decoded.cols, reference.cols) << name;
    ASSERT_EQ(decoded.channels(), reference.channels()) << name;

    // The pixels must be identical, not merely close.
    cv::Mat difference;
    cv::absdiff(decoded, reference, difference);
    double largest = 0.0;
    cv::minMaxLoc(difference.reshape(1), nullptr, &largest);

    EXPECT_EQ(largest, 0.0)
        << name << ": the decoded image differs from the reference";
}

}  // namespace

TEST(Vector, DecodesTheGreyscaleVectorExactly) {
    check_vector("gradient_16x16");
}

TEST(Vector, DecodesTheColourVectorExactly) {
    // The colour vector exists because the corner samples are stored per
    // channel. A decoder that reads them in the wrong order produces a
    // different image, and would still pass on the greyscale vector.
    check_vector("gradient_16x16_colour");
}

TEST(Vector, DecodesTheTruncationVectorExactly) {
    // The truncation vector pins the number of roundings. Its blocks have
    // corner values for which truncating once at the end and truncating
    // after each interpolation give different answers, so a decoder that
    // applies the wrong rule produces a different image.
    check_vector("truncation_3x16");
}

TEST(Vector, TheColourVectorIsNotDecodableAsGreyscale) {
    // The two vectors hold different pictures, so a decoder that ignored
    // the channel count would produce the wrong result.
    const std::string dir = vector_directory();

    const cv::Mat greyReference =
        cv::imread(dir + "/gradient_16x16_reference.bmp", cv::IMREAD_UNCHANGED);
    const cv::Mat colourReference =
        cv::imread(dir + "/gradient_16x16_colour_reference.bmp", cv::IMREAD_UNCHANGED);

    ASSERT_FALSE(greyReference.empty());
    ASSERT_FALSE(colourReference.empty());

    EXPECT_NE(greyReference.channels(), colourReference.channels());
}

TEST(Vector, ToleratesTrailingBytes) {
    // The specification says a decoder may tolerate trailing bytes after
    // the payload. The vector with three trailing bytes checks that.
    const std::string dir = vector_directory();

    std::ifstream in(dir + "/gradient_16x16.bcc", std::ios::binary);
    ASSERT_TRUE(in.good());
    const std::vector<std::uint8_t> container((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
    ASSERT_FALSE(container.empty());

    std::vector<std::uint8_t> withTrailing = container;
    withTrailing.push_back(0x00);
    withTrailing.push_back(0x00);
    withTrailing.push_back(0x00);

    blockcodec::Codec codec;
    const cv::Mat decoded = codec.decompress(withTrailing, nullptr);

    EXPECT_EQ(decoded.rows, 16);
    EXPECT_EQ(decoded.cols, 16);
    EXPECT_EQ(decoded.channels(), 1);
}
