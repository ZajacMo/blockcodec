// Generates the reference test vector used by codec_selftest.
//
// The vector is a deterministic 16x16 greyscale gradient. It is compressed
// with the reference encoder and the decoded result is stored alongside it,
// so an independent implementation can check that its decoder produces
// exactly the same pixels.
//
// Run from the repository root:
//   codec_vector_gen [output_directory]

#include "Compress_gray.h"

#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    const std::string outDir = (argc > 1) ? argv[1] : "tests/vectors";

    // A deterministic synthetic image: a diagonal gradient with a small
    // amount of structure, so the encoder produces both rectangular blocks
    // and isolated pixels.
    const int size = 16;
    cv::Mat source(size, size, CV_8UC1);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int value = x * 8 + y * 4;
            // A constant region in one corner produces isolated pixels,
            // which exercises a different branch of the codec.
            if (x < 4 && y < 4)
                value = 200;
            source.at<uchar>(y, x) = static_cast<uchar>(value & 0xFF);
        }
    }

    const std::string srcPath = outDir + "/gradient_16x16_source.bmp";
    const std::string namPath = outDir + "/gradient_16x16.nam";
    const std::string refPath = outDir + "/gradient_16x16_reference.bmp";

    cv::imwrite(srcPath, source);

    Compress_gray encoder;
    encoder.init_compress(source, 10);
    encoder.compress();
    encoder.save(namPath);

    Compress_gray decoder;
    decoder.init_uncompress(namPath);
    cv::Mat decoded = decoder.uncompress();

    cv::imwrite(refPath, decoded);

    std::cout << "Wrote:\n";
    std::cout << "  " << srcPath << "\n";
    std::cout << "  " << namPath << "\n";
    std::cout << "  " << refPath << "\n";
    std::cout << "PSNR: " << Compress_gray::getPSNR(source, decoded) << " dB\n";
    std::cout << "Ratio: " << encoder.getRate() << "\n";
    return 0;
}
