#include "Compress_gray.h"


Compress_gray::Compress_gray()
{
    sigma = 10;
    isMarked = nullptr;
}

// Prepares the encoder (compression).
void Compress_gray::init_compress(const cv::Mat& imge, const double& sigma_) {
    sigma = sigma_;
    img = imge;
    rows = img.rows;
    cols = img.cols * img.channels();
    newMatrix(isMarked, rows, cols, false);
    newMatrix(encoded_img, rows, cols);
}

// Prepares the decoder (decompression).
void Compress_gray::init_uncompress(const std::string filename, const int& size) {
    data.init(filename, size);

    // Read the image dimensions.
    rows = data.read(16);
    cols = data.read(16);

    // Colour-image support is not implemented; the stream is grayscale only.
    result = cv::Mat::zeros(rows, cols, CV_8UC1);
    sigma = 10;
    newMatrix(isMarked, rows, cols, false);
    newMatrix(encoded_img, rows, cols);
}

// Destructor; releases the matrices.
Compress_gray::~Compress_gray()
{
    deleteMatrix(isMarked, rows);
    deleteMatrix(encoded_img, rows);
}

// Frees a matrix, guarding against double frees.
template<typename t>
void Compress_gray::deleteMatrix(t**& p, const int& len1) {
    if (p) {
        for (int i = 0; i < len1; i++)
            delete[] p[i];
        delete[] p;
        p = nullptr;
    }
}

// Allocates a len1 x len2 matrix and fills it with `init`.
template<typename t>
void Compress_gray::newMatrix(t**& p, const int& len1, const int& len2, t init) {
    p = new t * [len1];
    for (int i = 0; i < len1; i++) {
        p[i] = new t[len2];
        for (int j = 0; j < len2; j++)
            p[i][j] = init;
    }
}

// Returns a reference to the encoding map entry at `p`.
uchar& Compress_gray::setEncodedImg(cv::Point& p) {
    return encoded_img[p.x][p.y];
}

// Decides whether `lt` and `rb` are the two opposite corners of a block (decoding).
bool Compress_gray::isPeer(const cv::Point& lt, const cv::Point& rb) {

    // The corner must not already be covered, and must be either a block end (2)
    // or an untouched pixel (0).
    if (!isMarked[rb.x][rb.y] && (encoded_img[rb.x][rb.y] == 2 || !encoded_img[rb.x][rb.y])) {

        // Walk down the column that contains rb.
        for (int i = lt.x; i < rb.x; i++)
            if (isMarked[i][rb.y] || encoded_img[i][rb.y] && !(lt.x == i && lt.y == rb.y))
                return false;

        // Walk across the row that contains rb.
        for (int j = lt.y; j < rb.y; j++)
            if (isMarked[rb.x][j] || encoded_img[rb.x][j] && !(lt.x == rb.x && lt.y == j))
                return false;

        return true;

    }
    return false;
}

// Given the top-left corner, finds the matching bottom-right corner (encoding).
cv::Point Compress_gray::findRb(const cv::Point& lt) {
    cv::Point rb = lt;
    // Extend along the diagonal.
    while (rb.x + 1 < rows && rb.y + 1 < cols &&
           RBlock(lt, cv::Point(rb.x + 1, rb.y + 1)).isBlock(img, isMarked, sigma)) {
        rb.x++;
        rb.y++;
    }
    // Extend horizontally.
    while (rb.y + 1 < cols && RBlock(lt, cv::Point(rb.x, rb.y + 1)).isBlock(img, isMarked, sigma))
        rb.y++;
    // Extend vertically.
    while (rb.x + 1 < rows && RBlock(lt, cv::Point(rb.x + 1, rb.y)).isBlock(img, isMarked, sigma))
        rb.x++;

    return rb;
}


// Given the top-left corner, finds the matching bottom-right corner (decoding).
RBlock Compress_gray::findBlock(const cv::Point& lt, int& blockNum) {
    cv::Point rb = lt;
    // Isolated pixels (3) have no extent to grow.
    if (encoded_img[lt.x][lt.y] != 3)
    {
        // Extend horizontally.
        while (rb.y + 1 < cols && isPeer(lt, cv::Point(rb.x, rb.y + 1)))
            rb.y++;
        // Extend vertically.
        while (rb.x + 1 < rows && isPeer(lt, cv::Point(rb.x + 1, rb.y)))
            rb.x++;
    }

    // Reaching this point with an out-of-range block number means the encoder
    // and decoder disagree about the block layout.
    if (blockNum >= allBlocks.size())
        throw std::runtime_error("blockNum out of range");

    // Update the bottom-right corner of the block being read.
    allBlocks[blockNum].rb = rb;

    // Paint the reconstructed pixels into the result image.
    allBlocks[blockNum].regain(result, encoded_img);

    return allBlocks[blockNum];
}


// Encodes the image into a 0/1/2/3 encoding map.
std::vector<RBlock> Compress_gray::encode() {
    std::vector<RBlock> blocks;
    // Scan the image.
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
        {
            // Skip pixels already covered by an earlier block.
            if (!isMarked[i][j])
            {
                // Find the bottom-right corner of the rectangle.
                cv::Point start = cv::Point(i, j), end = findRb(start);

                // Build the block.
                RBlock block(img, start, end);

                // Isolated pixel.
                if (start.x == end.x && start.y == end.y)
                {
                    setEncodedImg(start) = 3;
                }
                // Rectangle: mark the start corner (1) and the end corner (2).
                else if (start.x <= end.x && start.y <= end.y)
                {
                    setEncodedImg(start) = 1;
                    setEncodedImg(end) = 2;
                }

                // Record the block.
                blocks.push_back(block);

                // Mark the covered region.
                block.mark(isMarked);

                // Skip to the end of the block to improve scan efficiency.
                j = end.y;
            }
        }

    return blocks;
}


// Decodes the 0/1/2/3 encoding map.
std::vector<RBlock> Compress_gray::decode() {

    // Decode.
    std::vector<RBlock> blocks;
    int blockNum = 0;

    // Scan the encoding map.
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < cols; j++)
        {
            // Find a pixel that has not been covered yet.
            if (!isMarked[i][j])
            {
                // Find the bottom-right corner of the rectangle.
                RBlock block = findBlock(cv::Point(i, j), blockNum);

                // Record the block.
                blocks.push_back(block);

                // Mark the covered region.
                block.mark(isMarked);
                blockNum++;

                // Skip to the end of the block to improve scan efficiency.
                j = block.rb.y;
            }
        }

    return blocks;
}


// Compresses the image into the binary stream.
void Compress_gray::compress()
{
    // Record the start time.
    QueryPerformanceCounter(&time[0]);
    data.write(rows, 16);
    data.write(cols, 16);

    // Encode.
    allBlocks = encode();

    // Write the encoded image.
    for (int i = 0; i < rows; i++)
    {
        int last_position = 0;
        bool isNull = true;
        for (int j = 0; j < cols; j++) {
            int value = encoded_img[i][j];
            if (0 == value) {
                // This row has no encoded pixels at this position; write a 0 marker.
                if (j == cols - 1)
                    data.add(0);
            }
            else {
                // This row has an encoded pixel: write a 1 marker, then the code.
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
                // Compute how many bits the position delta needs.
                // last_position is an index, so it must be decremented by 1 when it is non-zero.
                int b = ceil(log((double)(cols - last_position - !isNull)) / log(2.0));

                // Guard against b being 0.
                if (!b) b++;

                // The position delta.
                int gap = j - last_position - !isNull;

                // The row now contains an encoded pixel.
                isNull = false;

                // Record the current position.
                last_position = j;

                // Write the position delta.
                data.write(gap, b);

                // Write the colours.
                // An isolated pixel stores a single colour.
                if (value == 3)
                    data.write(img.at<uchar>(i, j), getDepth());
                // A rectangle stores four colours.
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

    // Record the end time.
    QueryPerformanceCounter(&time[1]);
}

// Decompresses the binary stream into an image.
cv::Mat& Compress_gray::uncompress()
{
    // Record the start time.
    QueryPerformanceCounter(&time[0]);
    bool isNull = true;
    // Position of the previous encoded pixel, and the value read for it.
    int col = 0, row = 0, value = 0;
    while ( data.readIndex < data.readLen)
    {
        // Read the row marker.
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

        // Compute how many bits the position delta needs.
        int b = ceil(log((double)(cols - col - !isNull)) / log(2.0));
        if (!b) b++;

        // Read the position delta.
        int gap = data.read(b);

        // Compute the absolute position.
        col += gap + !isNull;

        // The row now contains an encoded pixel.
        isNull = false;

        // Record the value.
        encoded_img[row][col] = value;

        // Read the colours.
        if (value == 3) {
            // Isolated pixel.
            allBlocks.push_back(
                RBlock(
                    cv::Point(row, col),
                    cv::Point(0, 0),
                    data.read(getDepth())));
        }
        else if (value == 1) {
            // Rectangle.
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

        // If the last pixel of the row was encoded, move to the next row.
        if (col == (cols - 1))
        {
            row++;
            isNull = true;
            col = 0;
        }
    }
    decode();
    QueryPerformanceCounter(&time[1]);
    return result;
}


// Computes the peak signal-to-noise ratio between two images, in dB.
double Compress_gray::getPSNR(cv::Mat img1, cv::Mat img2) {
    double mid = 0;;
    for (int i = 0; i < img1.rows; i++)
        for (int j = 0; j < img1.cols*img1.channels(); j++) {
            mid += pow(img1.at<uchar>(i, j) - img2.at<uchar>(i, j), 2);
        }
    return 10 * log10f(pow(255., 2) * img1.cols * img1.rows *img1.channels() / mid);
}

// Returns the bit depth of the source image.
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
        return -1; // Unknown or unsupported depth.
    }
}

// Writes the compressed stream to disk.
void Compress_gray::save(const std::string& filename) {
    data.save(filename);
}

// Number of bits read back from disk.
long long Compress_gray::getReadLen() const {
    return data.readLen;
}

// Number of bits written to the stream.
long long Compress_gray::getWriteLen() const {
    return data.vecLen * PERLEN + data.index;
}

// Wall-clock duration of the last compress()/uncompress() call, in seconds.
double Compress_gray::getTime() const{
    LARGE_INTEGER tmp;
    QueryPerformanceFrequency(&tmp);
    return (time[1].QuadPart - time[0].QuadPart) * 1.0 / tmp.QuadPart;
}

// Compressed size as a fraction of the raw size (lower is better).
double Compress_gray::getRate()const {
    return getWriteLen() / (rows * cols * img.channels() * 8.0);
}
