#pragma once
#include "RBlock.h"
#include "Bits.h"
#include <windows.h>

class Compress_gray
{
public:

	// Source image (grayscale, CV_8UC1).
	cv::Mat img;

	bool isColorful;

public:

	// Default constructor.
	Compress_gray();

	// Prepares the encoder for the given image. `sigma` is the colour tolerance
	// that decides how aggressively pixels are grouped into blocks.
	void init_compress(const cv::Mat& imge, const double& sigma_ = 10);

	// Prepares the decoder for a previously saved stream.
	void init_uncompress(const std::string filename, const int& size = 2);

	// Destructor.
	~Compress_gray();

	// Computes the peak signal-to-noise ratio between two images, in dB.
	static double getPSNR(cv::Mat img1, cv::Mat img2);

	// Encodes the image into the internal bit stream.
	void compress();

	// Decodes the internal bit stream back into an image.
	cv::Mat& uncompress();

	// Writes the compressed stream to disk.
	void save(const std::string& filename = "compressed.nam");

	// Number of bits read back from disk.
	long long getReadLen() const;

	// Number of bits written to the stream.
	long long getWriteLen() const;

	// Wall-clock duration of the last compress()/uncompress() call, in seconds.
	double getTime()const;

	// Compressed size as a fraction of the raw size (lower is better).
	double getRate()const;

protected:

	// Reconstructed image.
	cv::Mat result;

	// Image dimensions. `cols` already accounts for the channel count.
	int rows , cols;

	// Per-pixel encoding map: 0 = untouched, 1 = block start, 2 = block end, 3 = isolated pixel.
	uchar** encoded_img;

	// Bit stream holding the compressed payload.
	Bits data;

	// Blocks recovered during decoding.
	std::vector<RBlock> allBlocks;

	// Marks regions already covered by an encoded block.
	bool** isMarked;

	// Colour tolerance used when deciding whether pixels form a block.
	int sigma;

	// Timestamps: [0] = start, [1] = end.
	LARGE_INTEGER time[2];

protected:

	// Returns a reference to the encoding map entry at `p`.
	uchar& setEncodedImg(cv::Point&);

	// Decides whether `lt` and `rb` are the two opposite corners of a block.
	bool isPeer(const cv::Point& lt, const cv::Point& rb);

	// Given the top-left corner, finds the matching bottom-right corner (encoding).
	cv::Point findRb(const cv::Point& lt);

	// Given the top-left corner, finds the matching bottom-right corner (decoding).
	RBlock findBlock(const cv::Point& start, int& blockNum);

	// Encodes the image into a list of blocks.
	std::vector<RBlock> encode();

	// Decodes the encoding map back into a list of blocks.
	std::vector<RBlock> decode();

	// Frees a matrix allocated by newMatrix().
	template<typename t> void deleteMatrix(t**&, const int& len1);

	// Allocates a len1 x len2 matrix and fills it with `init`.
	template<typename t> void newMatrix(t**&, const int& len1, const int& len2, t init = 0);

	// Returns the bit depth of the source image.
	int getDepth();
};

