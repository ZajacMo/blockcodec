#pragma once
#include <bitset>
#include <vector>
#include <fstream>
#include <string.h>
#include <cmath>
#include <stdexcept>

// Number of bits stored in each element of the backing vector.
#define PERLEN 1024

class Bits {

    friend class Compress_gray;

private:

    // Binary payload accumulated during encoding.
    std::vector<std::bitset<PERLEN>> bits;

    // Binary payload loaded from disk during decoding.
    char* loadedInfo;

protected:

    // Number of filled chunks in `bits`, plus the bit index within the last chunk.
    int vecLen, index;

    // Total length (in characters) of the payload read from disk.
    long long readLen;

    // Cursor into the payload read from disk.
    long long readIndex;

protected:

    Bits();

    // Destructor; releases the buffer allocated by init().
    ~Bits();

    // Loads a previously saved payload from disk (used when decoding).
    void init(const std::string& filename, const int& size = 2);

    // Appends the low `len` bits of `info`, most significant bit first.
    void write(const int& info, const int& len);

    // Appends a single 1 or 0 bit.
    void add(const std::bitset<1>& bit);

    // Writes the accumulated payload to `filename`.
    void save(const std::string& filename);

    // Reads `len` bits and returns them as an integer, most significant bit first.
    int read(const int& len);

};

