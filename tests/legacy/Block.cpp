#include "Block.h"

Block::Block()
{
	// Initialize the colors to a sentinel value.
	color[0][0] = color[0][1] = color[1][0] = color[1][1] = -1;
}

Block::Block(const cv::Point lt, const cv::Point rb)
{
	// Store the corner coordinates.
	this->lt = lt;
	this->rb = rb;
}

Block::~Block()
{

}
