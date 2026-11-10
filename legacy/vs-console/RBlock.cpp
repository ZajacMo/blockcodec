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

// Linear interpolation between two colour values.
inline uchar RBlock::getMedian(const uchar& a, const uchar& b, const double rate) const {
	return a + (b - a) * rate;
}

// Position of `i` within the interval [a, b], normalised to [0, 1].
inline double RBlock::getGamma(const int& a, const int& b, const int& i) const {
	if (b == a)
		return 0;
	return (i - a) / (double)(b - a);
}

// Decides whether the region spanned by lt..rb can be represented by the
// bilinear interpolation of its four corner colours, within the tolerance `xgm`.
bool RBlock::isBlock(const cv::Mat& img, bool** const isMarked, const  int& xgm)const {
	// Read the four corner colours.
	uchar point_color[2][3] = { {img.at<uchar>(lt.x, lt.y),					img.at<uchar>(lt.x, rb.y)},
														{img.at<uchar>(rb.x, lt.y),			img.at<uchar>(rb.x, rb.y) } };

	// Walk every row.
	for (int i = lt.x; i <= rb.x; i++) {

		// Position of row i between lt.x and rb.x.
		double gamma_i = getGamma(lt.x, rb.x, i);

		// Interpolate the left edge colour at row i.
		point_color[0][2] = getMedian(point_color[0][0], point_color[1][0], gamma_i);

		// Interpolate the right edge colour at row i.
		point_color[1][2] = getMedian(point_color[0][1], point_color[1][1], gamma_i);

		// Walk every column.
		for (int j = lt.y; j <= rb.y; j++) {

			// The pixel is already covered by another block.
			if (isMarked[i][j])
				return false;

			// Position of column j between lt.y and rb.y.
			double gamma_j = getGamma(lt.y, rb.y, j);

			// Interpolate the expected colour at (i, j).
			double gest = getMedian(point_color[0][2], point_color[1][2], gamma_j);

			// Reject the block when a pixel deviates by more than the tolerance.
			if (abs(img.at<uchar>(i, j) - gest) > xgm)
				return false;
		}
	}
	return true;
}

// Marks every pixel of the block as covered.
void RBlock::mark(bool** isMarked) const {
	for (int i = lt.x; i <= rb.x; i++)
		for (int j = lt.y; j <= rb.y; j++)
			isMarked[i][j] = true;
}



// Reconstructs the block region from its four corner colours.
void RBlock::regain(cv::Mat& img, uchar** encoded_img) {
	if (lt == rb)
	{
		img.at<uchar>(lt.x, lt.y) = color[0][0];
		return;
	}
	uchar point_color[2][3] = { {color[0][0], color[0][1]},
														 {color[1][0], color[1][1]}};
	// Walk every row.
	for (int i = lt.x; i <= rb.x; i++) {

		// Position of row i between lt.x and rb.x.
		double gamma_i = getGamma(lt.x, rb.x, i);

		// Interpolate the left edge colour at row i.
		point_color[0][2] = getMedian(point_color[0][0], point_color[1][0], gamma_i);

		// Interpolate the right edge colour at row i.
		point_color[1][2] = getMedian(point_color[0][1], point_color[1][1], gamma_i);

		// Walk every column.
		for (int j = lt.y; j <= rb.y; j++) {

			// Position of column j between lt.y and rb.y.
			double gamma_j = getGamma(lt.y, rb.y, j);

			// Write the interpolated colour.
			img.at<uchar>(i, j) = getMedian(point_color[0][2], point_color[1][2], gamma_j);
		}
	}
}
