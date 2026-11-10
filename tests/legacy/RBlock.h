#pragma once
#include "Block.h"

class RBlock :
	public Block
{
protected:

	inline uchar getMedian(const uchar& a, const uchar& b, const double rate) const;

	inline double getGamma(const int& a, const int& b, const int& i) const;

public:
	RBlock();

	RBlock(const cv::Point lt, const cv::Point rb = cv::Point(0, 0), uchar p = 0);

	RBlock(const cv::Point lt, const cv::Point rb, uchar** p);

	RBlock(const cv::Mat& img, const cv::Point lt, const cv::Point rb);

	virtual bool isBlock(const cv::Mat& img, bool** const isMarked, const int& xgm) const final;

	virtual void mark(bool** isMarked) const final;

	void regain(cv::Mat& img, uchar** encoded_img);
};

