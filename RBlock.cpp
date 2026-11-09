#include "RBlock.h"

RBlock::RBlock() {
}

RBlock::RBlock(const cv::Point lt, const cv::Point rb, uchar p) :Block(lt, rb) {
	color[0][0] = p;
}
RBlock::RBlock(const cv::Point lt, const cv::Point rb, uchar** p) :Block(lt, rb) {
	if (p)
		for (int i = 0; i < 2; i++)
			for (int j = 0; j < 2; j++)
				color[i][j] = p[i][j];
}

RBlock::RBlock(const cv::Mat& img, const cv::Point lt, const cv::Point rb) :Block(lt, rb) {
	color[0][0] = img.at<uchar>(lt.x, lt.y);
	color[0][1] = img.at<uchar>(lt.x, rb.y);
	color[1][0] = img.at<uchar>(rb.x, lt.y);
	color[1][1] = img.at<uchar>(rb.x, rb.y);
}

inline uchar RBlock::getMedian(const uchar& a, const uchar& b, const double rate) const {
	return a + (b - a) * rate;
}

inline double RBlock::getGamma(const int& a, const int& b, const int& i) const {
	if (b == a)
		return 0;
	return (i - a) / (double)(b - a);
}

bool RBlock::isBlock(const cv::Mat& img, bool** const isMarked, const  int& xgm)const {
	// Read the four corner colours
	uchar point_color[2][3] = { {img.at<uchar>(lt.x, lt.y),					img.at<uchar>(lt.x, rb.y)},
														{img.at<uchar>(rb.x, lt.y),			img.at<uchar>(rb.x, rb.y) } };

	// Walk every row of the block
	for (int i = lt.x; i <= rb.x; i++) {

		// Position of row i between lt.x and rb.x, as a 0..1 ratio
		double gamma_i = getGamma(lt.x, rb.x, i);

		// Interpolated colour on the left edge of row i
		point_color[0][2] = getMedian(point_color[0][0], point_color[1][0], gamma_i);

		// Interpolated colour on the right edge of row i
		point_color[1][2] = getMedian(point_color[0][1], point_color[1][1], gamma_i);

		// Walk every column of the block
		for (int j = lt.y; j <= rb.y; j++) {

			// A previously encoded pixel cannot be reused
			if (isMarked[i][j])
				return false;

			// Position of column j between lt.y and rb.y, as a 0..1 ratio
			double gamma_j = getGamma(lt.y, rb.y, j);

			// Bilinearly interpolated colour at (i, j)
			double gest = getMedian(point_color[0][2], point_color[1][2], gamma_j);

			// Reject the block when the pixel deviates too much from the model
			if (abs(img.at<uchar>(i, j) - gest) > xgm)
				return false;
		}
	}
	return true;
}

void RBlock::mark(bool** isMarked) const {
	for (int i = lt.x; i <= rb.x; i++)
		for (int j = lt.y; j <= rb.y; j++)
			isMarked[i][j] = true;
}



void RBlock::regain(cv::Mat& img, uchar** encoded_img) {
	if (lt == rb)
	{
		img.at<uchar>(lt.x, lt.y) = color[0][0];
		return;
	}
	uchar point_color[2][3] = { {color[0][0], color[0][1]},
														 {color[1][0], color[1][1]}};
	// Walk every row of the block
	for (int i = lt.x; i <= rb.x; i++) {

		// Position of row i between lt.x and rb.x, as a 0..1 ratio
		double gamma_i = getGamma(lt.x, rb.x, i);

		// Interpolated colour on the left edge of row i
		point_color[0][2] = getMedian(point_color[0][0], point_color[1][0], gamma_i);

		// Interpolated colour on the right edge of row i
		point_color[1][2] = getMedian(point_color[0][1], point_color[1][1], gamma_i);

		// Walk every column of the block
		for (int j = lt.y; j <= rb.y; j++) {

			// Position of column j between lt.y and rb.y, as a 0..1 ratio
			double gamma_j = getGamma(lt.y, rb.y, j);

			// Bilinearly interpolated colour at (i, j)
			img.at<uchar>(i, j) = getMedian(point_color[0][2], point_color[1][2], gamma_j);
		}
	}
}