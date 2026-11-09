#include "Compress_gray.h"
#include <chrono>
#include <stdexcept>
#include <limits>


Compress_gray::Compress_gray()
{
    sigma = 10;
    isMarked = nullptr;
    encoded_img = nullptr;
    isNull=true;
}

// Prepare the encoder for a source image
void Compress_gray::init_compress(const cv::Mat& imge, const double& sigma_) {
    // An empty Mat means the caller failed to load the file (cv::imread
    // returns an empty matrix rather than throwing), so refuse early
    // instead of producing an empty image.
    if (imge.empty())
        throw std::runtime_error("Compress_gray: the source image is empty");
    if (imge.depth() != CV_8U)
        throw std::runtime_error("Compress_gray: only 8-bit source images are supported");

    isNull=false;
    sigma = sigma_;
    img = imge;
    rows = img.rows;
    cols = img.cols * img.channels();
    newMatrix(isMarked, rows, cols, false);
    newMatrix(encoded_img, rows, cols);
}

// Prepare the decoder for a compressed file
void Compress_gray::init_uncompress(const std::string filename, const int& size) {
    data.init(filename, size);
    isNull = false;
    // Read the image dimensions
    rows = data.read(16);
    cols = data.read(16);
    isColorful = (cols>rows?true:false);

    // The dimensions come straight out of the file, so a truncated or
    // corrupt stream would otherwise ask for an arbitrary allocation.
    // Reject anything that cannot describe a real image.
    if (rows <= 0 || cols <= 0)
        throw std::runtime_error("Compress_gray: invalid image dimensions in the compressed file");
    if (static_cast<long long>(rows) * cols > kMaxPixels)
        throw std::runtime_error("Compress_gray: compressed file declares an implausible image size");

    // Initialise the working state
    result = cv::Mat::zeros(rows, cols, CV_8UC1);
    sigma = 10;
    if(isMarked)
        deleteMatrix(isMarked, rows);
    newMatrix(isMarked, rows, cols, false);
    if(encoded_img)
        deleteMatrix(encoded_img, rows);
    newMatrix(encoded_img, rows, cols);
}

// Destructor, releases the working matrices
Compress_gray::~Compress_gray()
{
    if(isMarked)
        deleteMatrix(isMarked, rows);
    if(encoded_img)
        deleteMatrix(encoded_img, rows);
}

// Release a 2D array, avoiding a memory leak
template<typename t>
void Compress_gray::deleteMatrix(t**& p, const int& len1) {
    if (p) {
        for (int i = 0; i < len1; i++)
            delete[] p[i];
        delete[] p;
    }
}

// Allocate a 2D array
template<typename t>
void Compress_gray::newMatrix(t**& p, const int& len1, const int& len2, t init) {
    p = new t * [len1];
    for (int i = 0; i < len1; i++) {
        p[i] = new t[len2];
        for (int j = 0; j < len2; j++)
            p[i][j] = init;
    }
}

// Access a cell of the marker matrix
uchar& Compress_gray::setEncodedImg(cv::Point& p) {
    return encoded_img[p.x][p.y];
}

// True when (lt, rb) is the diagonal pair of a block (decoder side)
bool Compress_gray::isPeer(const cv::Point& lt, const cv::Point& rb) {

    // The cell must be unclaimed and must not be an isolated pixel (3) or a block origin (1)
    if (!isMarked[rb.x][rb.y] && (encoded_img[rb.x][rb.y] == 2 || !encoded_img[rb.x][rb.y])) {

        // Walk the column of rb
        for (int i = lt.x; i < rb.x; i++)
            if (isMarked[i][rb.y] || encoded_img[i][rb.y] && !(lt.x == i && lt.y == rb.y))
                return false;

        // Walk the row of rb
        for (int j = lt.y; j < rb.y; j++)
            if (isMarked[rb.x][j] || encoded_img[rb.x][j] && !(lt.x == rb.x && lt.y == j))
                return false;

        return true;

    }
    return false;
}

// Find the bottom-right corner for a given top-left corner (encoder side)
cv::Point Compress_gray::findRb(const cv::Point& lt) {
    cv::Point rb = lt;
    // Extend along the diagonal
    while (rb.x + 1 < rows && rb.y + 1 < cols &&
           RBlock(lt, cv::Point(rb.x + 1, rb.y + 1)).isBlock(img, isMarked, sigma)) {
        rb.x++;
        rb.y++;
    }
    // Extend horizontally
    while (rb.y + 1 < cols && RBlock(lt, cv::Point(rb.x, rb.y + 1)).isBlock(img, isMarked, sigma))
        rb.y++;
    // Extend vertically
    while (rb.x + 1 < rows && RBlock(lt, cv::Point(rb.x + 1, rb.y)).isBlock(img, isMarked, sigma))
        rb.x++;

    return rb;
}


// Find the block extent for a given top-left corner (decoder side)
RBlock Compress_gray::findBlock(const cv::Point& lt, int& blockNum) {
    cv::Point rb = lt;
    // Isolated pixels have no extent to recover
    if (encoded_img[lt.x][lt.y] != 3)
    {
        // Extend horizontally
        while (rb.y + 1 < cols && isPeer(lt, cv::Point(rb.x, rb.y + 1)))
            rb.y++;
        // Extend vertically
        while (rb.x + 1 < rows && isPeer(lt, cv::Point(rb.x + 1, rb.y)))
            rb.x++;
    }

    // A failure here means the encoder and decoder disagree
    if (blockNum >= allBlocks.size())
        throw std::runtime_error("blockNum out of range");

    // Record the recovered bottom-right corner of the block
    allBlocks[blockNum].rb = rb;

    // Paint the reconstructed pixels back into the image
    allBlocks[blockNum].regain(result, encoded_img);

    return allBlocks[blockNum];
}



// Read the monotonic clock (seconds)
double Compress_gray::now() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// Build the block list and the 0/1/2/3 marker matrix
std::vector<RBlock> Compress_gray::encode() {
    std::vector<RBlock> blocks;
    // Walk the image
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
        {
            // Skip pixels that are already encoded
            if (!isMarked[i][j])
            {
                // Find the bottom-right corner of the block
                cv::Point start = cv::Point(i, j), end = findRb(start);

                // Create the block
                RBlock block(img, start, end);

                // Isolated pixel
                if (start.x == end.x && start.y == end.y)
                {
                    setEncodedImg(start) = 3;
                }
                // Rectangular block
                else if (start.x <= end.x && start.y <= end.y)
                {
                    setEncodedImg(start) = 1;
                    setEncodedImg(end) = 2;
                }

                // Record the block
                blocks.push_back(block);

                // Mark the encoded region
                block.mark(isMarked);

                // Skip to the end of the marked region to speed up the scan
                j = end.y;
            }
        }

    return blocks;
}

// Rebuild the image from the 0/1/2/3 marker matrix
std::vector<RBlock> Compress_gray::decode() {

    // Decode
    std::vector<RBlock> blocks;
    int blockNum = 0;

    // Walk the marker matrix
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
        {
            // Find the next unclaimed cell
            if (!isMarked[i][j])
            {
                // Find the bottom-right corner of the block
                RBlock block = findBlock(cv::Point(i, j), blockNum);

                // Record the block
                blocks.push_back(block);

                // Mark the reconstructed region
                block.mark(isMarked);
                blockNum++;

                // Skip to the end of the marked region to speed up the scan
                j = block.rb.y;
            }
        }

    return blocks;
}


// Encode the image into the bitstream
void Compress_gray::compress()
{
    // Record the start time
    time[0] = now();
    //data.write((int)log2(rows), 4);
    data.write(rows, 16);
    data.write(cols, 16);

    // Detect whether the source image is colour
    //if (img.channels() == 3)
    //    data.add(1);
    //else
    //    data.add(0);

    // Build the block list
    allBlocks = encode();

    // Serialise the marker matrix
    for (int i = 0; i < rows; i++)
    {
        int last_position = 0;
        bool isNull = true;
        for (int j = 0; j < cols; j++) {
            int value = encoded_img[i][j];
            if (0 == value) {
                // No encoded pixel on this row: emit a 0
                if (j == cols - 1)
                    data.add(0);
            }
            else {
                // This row has an encoded pixel: emit a 1, then the block kind
                data.add(1);
                if (1 == value)
                    data.add(1);
                else if (2 == value) {
                    data.add(0);
                    data.add(1);
                }
                else if (3 == value) {
                    data.add(0);
                    data.add(0);
                }
                // Number of bits needed for the position delta
                // last_position is an index, so subtract one extra when the row already has a pixel
                int b = ceil(log((double)(cols - last_position - !isNull)) / log(2.0));

                // Guard against a zero-width field
                if (!b) b++;

                // Compute the position delta
                int gap = j - last_position - !isNull;

                // The row now holds at least one encoded pixel
                isNull = false;

                // Record the current position
                last_position = j;

                // Write the position delta as a bit field
                data.write(gap, b);

                // Write the pixel colours
                // An isolated pixel stores a single colour
                if (value == 3)
                    data.write(img.at<uchar>(i, j), getDepth());
                // A rectangular block stores four corner colours
                else if (value == 1) {
                    for (int m = 0; m < allBlocks.size(); m++) {
                        if (allBlocks[m].lt == cv::Point(i, j)) {
                            data.write(allBlocks[m].color[0][0], getDepth());
                            data.write(allBlocks[m].color[0][1], getDepth());
                            data.write(allBlocks[m].color[1][0], getDepth());
                            data.write(allBlocks[m].color[1][1], getDepth());
                            break;
                        }
                    }
                }

            }
        }
    }

    // Record the end time
    time[1] = now();
}

// Decode the bitstream back into an image
cv::Mat& Compress_gray::uncompress()
{
    // Record the start time
    time[0] = now();
    bool isNull = true;
    // Position of the previous non-zero element
    int col = 0, row = 0, value = 0;
    while ( data.readIndex < data.readLen)
    {
        // Read the row-continuation flag
        if (!data.read(1)) {
            row++;
            if (row >= rows)
                break;
            isNull = true;
            col = 0;
            continue;
        }
        else {
            if (data.read(1))
                value = 1;
            else
                value = (data.read(1)? 2 : 3);
        }

        // Number of bits needed for the position delta
        int b = ceil(log((double)(cols - col - !isNull)) / log(2.0));
        if (!b) b++;

        // Read the position delta
        int gap = data.read(b);

        // Compute the current position
        col += gap + !isNull;

        // The row now holds at least one encoded pixel
        isNull = false;

        // Store the marker
        encoded_img[row][col] = value;

        // Read the pixel colours
        if (value == 3) {
            // Isolated pixel
            allBlocks.push_back(
                RBlock(
                    cv::Point(row, col),
                    cv::Point(0, 0),
                    data.read(getDepth())));
        }
        else if (value == 1) {
            // Rectangular block
            uchar** p = nullptr;
            newMatrix(p, 2, 2);
            p[0][0] = data.read(getDepth());
            p[0][1] = data.read(getDepth());
            p[1][0] = data.read(getDepth());
            p[1][1] = data.read(getDepth());
            allBlocks.push_back(
                RBlock(cv::Point(row, col), cv::Point(0, 0), p));
            deleteMatrix(p, 2);
        }

        // The last pixel of the row was encoded: move to the next row
        if (col == (cols - 1))
        {
            row++;
            isNull = true;
            col = 0;
        }
    }
    decode();
    time[1] = now();
    return result;
}


// Compute the PSNR between two images, in dB.
double Compress_gray::getPSNR(cv::Mat img1, cv::Mat img2) {
    // The two images must describe the same samples, otherwise the
    // element-wise comparison below reads out of bounds.
    if (img1.empty() || img2.empty())
        throw std::runtime_error("Compress_gray::getPSNR: empty image");
    if (img1.rows != img2.rows || img1.cols != img2.cols || img1.channels() != img2.channels())
        throw std::runtime_error("Compress_gray::getPSNR: image dimensions do not match");

    double mid = 0;
    for (int i = 0; i < img1.rows; i++)
        for (int j = 0; j < img1.cols*img1.channels(); j++) {
            mid += pow(img1.at<uchar>(i, j) - img2.at<uchar>(i, j), 2);
        }

    // A perfect reconstruction has zero mean squared error, so the PSNR
    // is unbounded. Report infinity instead of dividing by zero.
    if (mid == 0)
        return std::numeric_limits<double>::infinity();

    return 10 * log10f(pow(255., 2) * img1.cols * img1.rows *img1.channels() / mid);
}

int Compress_gray::getDepth() {
    switch (img.depth()) {
    case CV_8U:
    case CV_8S:
        return 8;
    case CV_16U:
    case CV_16S:
        return  16;
    case CV_32S:
    case CV_32F:
        return 32;
    case CV_64F:
        return 64;
    default:
        return -1; // Unknown or unsupported sample depth
    }
}

void Compress_gray::save(const std::string& filename) {
    data.save(filename);
}

long long Compress_gray::getReadLen() const {
    return data.readLen;
}

long long Compress_gray::getWriteLen() const {
    if(data.readLen)
        return data.readLen;
    return data.vecLen * PERLEN + data.index;
}

double Compress_gray::getTime() const{
    return time[1] - time[0];
}

double Compress_gray::getRate()const {
    return getWriteLen() / (rows * cols  * 8.0);
}
