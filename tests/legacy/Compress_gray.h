#pragma once
#include "RBlock.h"
#include "Bits.h"

// The codec is platform-agnostic; only the wall-clock timer needs a
// platform-specific backend, so keep the Windows header out of the
// public interface (it would otherwise leak into every translation unit
// and break the Linux CI build).
#ifdef _WIN32
#include <windows.h>
#endif

class Compress_gray
{
public:

    // Upper bound on the number of pixels a decoded image may declare.
    // The dimensions are read from the compressed file, so they must be
    // validated before they are used to size an allocation.
    static const long long kMaxPixels = 1LL << 28;   // 268 megapixels

    // Source image
    cv::Mat img;

    bool isNull;

    bool isColorful;

    // Reconstructed image
    cv::Mat result;

public:

    // Constructor
    Compress_gray();

    // Prepare the encoder for a source image
    void init_compress(const cv::Mat& imge, const double& sigma_ = 10);

    // Prepare the decoder for a compressed file
    void init_uncompress(const std::string filename, const int& size = 2);

    // Destructor
    ~Compress_gray();

    // Compute the PSNR between two images
    static double getPSNR(cv::Mat img1, cv::Mat img2);

    // Run the encoder
    void compress();

    // Run the decoder
    cv::Mat& uncompress();

    // Write the compressed bitstream to disk
    void save(const std::string& filename = "compressed.nam");

    // Number of bits read back from a compressed file
    long long getReadLen() const;

    // Number of bits produced by the encoder
    long long getWriteLen() const;

    // Elapsed time of the last compress/uncompress call, in seconds
    double getTime()const;

    // Compressed size divided by the raw size
    double getRate()const;

protected:

    // Image dimensions (cols already includes the channel count)
    int rows , cols;

    // Marker matrix: 0 = unencoded, 1 = block origin, 2 = block end, 3 = isolated pixel
    uchar** encoded_img;

    // Bitstream sink/source
    Bits data;

    // Blocks reconstructed while decoding
    std::vector<RBlock> allBlocks;

    // Marker matrix tracking which pixels have already been encoded
    bool** isMarked;

    // Maximum tolerated deviation when deciding whether a block is valid
    int sigma;

    // Timestamps of the last operation, in seconds on a monotonic clock
    double time[2];

protected:

    // Access a cell of the marker matrix
    uchar& setEncodedImg(cv::Point&);

    // Read the monotonic clock, used to time compress/uncompress
    static double now();

    // True when (lt, rb) is the diagonal pair of a block (decoder side)
    bool isPeer(const cv::Point& lt, const cv::Point& rb);

    // Find the bottom-right corner for a given top-left corner (encoder side)
    cv::Point findRb(const cv::Point& lt);

    // Find the block extent for a given top-left corner (decoder side)
    RBlock findBlock(const cv::Point& start, int& blockNum);

    // Build the block list and the marker matrix
    std::vector<RBlock> encode();

    // Rebuild the image from the marker matrix and the block list
    std::vector<RBlock> decode();

    // Release a 2D array
    template<typename t> void deleteMatrix(t**&, const int& len1);

    // Allocate a 2D array filled with `init`
    template<typename t> void newMatrix(t**&, const int& len1, const int& len2, t init = 0);

    // Number of bits used to store one sample of the source image
    int getDepth();
};

