#include "Compress_gray.h"
#include <opencv2/opencv.hpp>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "images/lena512.bmp";
    cv::Mat src = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (src.empty()) { std::printf("cannot read %s\n", path.c_str()); return 1; }

    std::printf("image=%s %dx%d\n", path.c_str(), src.cols, src.rows);
    std::printf("sigma\tbits\tfile_bytes\tfile_over_raw\ttrue_cr\tpsnr\n");
    const int sigmas[] = {1, 5, 10, 20, 30, 50};
    for (int s : sigmas) {
        Compress_gray enc;
        enc.init_compress(src, s);
        enc.compress();
        enc.save("bench_tmp.nam");

        // The .nam container stores one ASCII character per bit, so the file
        // on disk is one byte per bit and is eight times larger than the
        // bitstream it carries. Report the real file size next to the
        // bitstream length, because the two differ by a factor of 8.
        std::FILE* f = std::fopen("bench_tmp.nam", "rb");
        long fileBytes = 0;
        if (f) {
            std::fseek(f, 0, SEEK_END);
            fileBytes = std::ftell(f);
            std::fclose(f);
        }

        Compress_gray dec;
        dec.init_uncompress("bench_tmp.nam");
        cv::Mat out = dec.uncompress();
        double psnr = Compress_gray::getPSNR(src, out);

        const double rawBytes = static_cast<double>(src.rows) * src.cols;
        // true_cr < 1 means the .nam file is smaller than the raw pixels.
        const double trueCr = rawBytes / static_cast<double>(fileBytes);
        std::printf("%d\t%lld\t%ld\t%.4f\t%.4f\t%.4f\n", s, enc.getWriteLen(),
                    fileBytes, static_cast<double>(fileBytes) / rawBytes,
                    trueCr, psnr);
    }
    std::remove("bench_tmp.nam");
    return 0;
}
