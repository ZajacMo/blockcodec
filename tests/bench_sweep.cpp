#include "Compress_gray.h"
#include <opencv2/opencv.hpp>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    const std::string path = (argc > 1) ? argv[1] : "images/lena512.bmp";
    cv::Mat src = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (src.empty()) { std::printf("cannot read %s\n", path.c_str()); return 1; }

    std::printf("image=%s %dx%d\n", path.c_str(), src.cols, src.rows);
    std::printf("sigma\tratio\tpsnr\tbytes\tbitlen\n");
    const int sigmas[] = {1, 5, 10, 20, 30, 50};
    for (int s : sigmas) {
        Compress_gray enc;
        enc.init_compress(src, s);
        enc.compress();
        enc.save("bench_tmp.nam");

        Compress_gray dec;
        dec.init_uncompress("bench_tmp.nam");
        cv::Mat out = dec.uncompress();
        double psnr = Compress_gray::getPSNR(src, out);
        std::printf("%d\t%.6f\t%.4f\t%lld\t%lld\n", s, enc.getRate(), psnr,
                    enc.getWriteLen() / 8, enc.getWriteLen());
    }
    std::remove("bench_tmp.nam");
    return 0;
}
