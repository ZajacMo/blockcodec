#pragma once
#include <opencv2/opencv.hpp>
class Block
{
	friend class Compress_gray;
protected:

	// Top-left and bottom-right corners of the block.
	cv::Point lt, rb;

	// Colors recorded for the block: the four corners of the region.
	uchar color[2][2];

public:

	Block();

	Block(const cv::Point lt, const cv::Point rb);

	~Block();

	virtual bool isBlock(const cv::Mat& img, bool** const isMarked, const int& xgm) const = 0;

	virtual void mark(bool** isMarked) const = 0;

};

