// Generates the reference vectors for the .bcc container.
//
// A vector is a deterministic synthetic image, the container it compresses
// to, and the image that the container decodes to. An independent
// implementation must decode the container to exactly the pixels of the
// reference image.
//
// Two vectors are written: one greyscale and one colour. The colour one
// exists because the corner samples are stored per channel, and a decoder
// that mixes the channels up would still pass on a greyscale image.
//
// Run from the repository root:
//   blockcodec_vector_gen [output_directory]

#include "blockcodec/codec.hpp"

#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

// Writes a container to a file.
void write_container_to(const std::string& path,
                        const std::vector<std::uint8_t>& container) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "cannot open '%s' for writing\n", path.c_str());
        std::exit(1);
    }
    out.write(reinterpret_cast<const char*>(container.data()),
              static_cast<std::streamsize>(container.size()));
}

// Generates one vector and returns the container.
std::vector<std::uint8_t> make_vector(const std::string& outDir,
                                       const std::string& name,
                                       const cv::Mat& source,
                                       int sigma) {
    const std::string srcPath = outDir + "/" + name + "_source.bmp";
    const std::string containerPath = outDir + "/" + name + ".bcc";
    const std::string refPath = outDir + "/" + name + "_reference.bmp";

    if (!cv::imwrite(srcPath, source)) {
        std::fprintf(stderr, "cannot write '%s'\n", srcPath.c_str());
        std::exit(1);
    }

    blockcodec::Codec codec({sigma});
    blockcodec::CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);
    write_container_to(containerPath, container);

    // The reference is the decoded image, so that a decoder is checked
    // against the encoder rather than against a stored expectation.
    const cv::Mat decoded = codec.decompress(container, nullptr);
    if (!cv::imwrite(refPath, decoded)) {
        std::fprintf(stderr, "cannot write '%s'\n", refPath.c_str());
        std::exit(1);
    }

    std::printf("  %s\n", name.c_str());
    std::printf("    source     %s (%dx%d, %d channels)\n", srcPath.c_str(),
                source.cols, source.rows, source.channels());
    std::printf("    container  %s (%zu bytes)\n", containerPath.c_str(),
                container.size());
    std::printf("    reference  %s\n", refPath.c_str());
    std::printf("    blocks     %zu\n", result.block_count);
    std::printf("    psnr       %.4f dB\n", result.psnr);

    return container;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outDir = (argc > 1) ? argv[1] : "tests/vectors";

    // A deterministic synthetic image: a diagonal gradient with a constant
    // region in one corner. The constant region forces the encoder to
    // produce isolated pixels as well as rectangular blocks, so the vector
    // exercises both branches of the codec.
    const int size = 16;

    cv::Mat grey(size, size, CV_8UC1);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int value = x * 8 + y * 4;
            if (x < 4 && y < 4) {
                value = 200;
            }
            grey.at<uchar>(y, x) = static_cast<uchar>(value & 0xFF);
        }
    }

    // The colour vector holds the same picture in three channels with
    // different values per channel, so that a decoder that reads the
    // corner samples in the wrong order produces a different image.
    cv::Mat colour(size, size, CV_8UC3);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int value = x * 8 + y * 4;
            if (x < 4 && y < 4) {
                value = 200;
            }
            colour.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uchar>(value & 0xFF),
                static_cast<uchar>((value * 3) & 0xFF),
                static_cast<uchar>((value * 7 + 40) & 0xFF));
        }
    }

    std::printf("Wrote the reference vectors to %s\n", outDir.c_str());
    make_vector(outDir, "gradient_16x16", grey, 10);
    make_vector(outDir, "gradient_16x16_colour", colour, 10);
    return 0;
}
