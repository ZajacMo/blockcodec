
#pragma once
#include <bitset>
#include <vector>

#define PERLEN 1024// Number of bits stored per std::bitset chunk

class Bits {

    friend class Compress_gray;

private:

    // Binary payload, stored as a list of fixed-size bitsets
    std::vector<std::bitset<PERLEN>> bits;

    // Raw buffer holding the bits loaded from disk (ASCII '0'/'1')
    char* loadedInfo;

protected:

    // Index of the current bitset chunk and the bit position inside it
    int vecLen, index;

    // Total length (in characters) of the loaded binary text
    long long readLen;

    long long readIndex;   // Cursor into the loaded text

protected:

    Bits();

    // Destructor, releases the loaded buffer
    ~Bits();

    // Load a file and initialise the Bits object (used when decompressing)
    void init(const std::string& filename, const int& size = 2);

    // Append the low `len` bits of `info`, most significant bit first
    void write(const int& info, const int& len);

    // Append a single bit to the current chunk
    void add(const std::bitset<1>& bit);

    // Write the bitstream to `filename`, one ASCII '0'/'1' character per bit
    void save(const std::string& filename);

    // Read `len` bits and decode them as an unsigned integer
    int read(  const int& len);

};

