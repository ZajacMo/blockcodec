#include "Block.h"
Block::Block()
{
	// Initialise the corner colours to an invalid marker
	color[0][0] = color[0][1] = color[1][0] = color[1][1] = -1;
}

Block::Block(const cv::Point lt, const cv::Point rb)
{
	// Initialise the top-left and bottom-right corners
	this->lt = lt;
	this->rb = rb;
}

Block::~Block()
{

}