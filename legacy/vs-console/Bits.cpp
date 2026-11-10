#include "Bits.h"
#include <iostream>

Bits::Bits() {
    bits.emplace_back(0);
    vecLen = 0;
    index = -1;
    readLen = 0;
    // Must be null: the destructor frees this, and the encoder never calls
    // init() (it only writes to the stream), so it stays unallocated there.
    loadedInfo = nullptr;
}

// Loads a previously saved payload from disk (used when decoding).
void Bits::init(const std::string& filename, const int& size) {
    std::ifstream in(filename, std::ios::binary | std::ios::in);
    if (!in) {
        throw std::runtime_error("cannot open compressed stream: " + filename);
    }

    const std::size_t capacity = static_cast<std::size_t>(std::pow(1024, size)) * 10;
    loadedInfo = new char[capacity];
    std::memset(loadedInfo, 0, capacity);

    // The payload is a run of '0'/'1' characters. The stream width bounds the
    // read so a malformed or oversized file cannot overrun the buffer.
    in.width(static_cast<std::streamsize>(capacity));
    in >> loadedInfo;
    in.close();

    readLen = std::strlen(loadedInfo);
    vecLen = index =  0;
    readIndex = -1;
}

// Destructor; releases the buffer allocated by init().
Bits::~Bits() {
    if (loadedInfo) {
		delete[] loadedInfo;
        loadedInfo = nullptr;
    }
}

// Appends the low `len` bits of `info`, most significant bit first.
void Bits::write(const int& info, const int& len) {
    for (int i = len - 1; i >= 0; i--) {
        if (info & (1 << i))
            add(1);
        else
            add(0);
    }
}


// Appends a single 1 or 0 bit.
void Bits::add(const std::bitset<1>& bit) {
    if (++index == PERLEN) {
        bits.emplace_back(0);
        vecLen++;
        index = 0;
    }
    bits[vecLen][index] = bit[0];
}

// Writes the accumulated payload to `filename`.
void Bits::save(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary | std::ios::trunc | std::ios::out);
    if (!out) {
        throw std::runtime_error("cannot write compressed stream: " + filename);
    }
    for (int i = 0; i < vecLen ; i++) {
        for (int j = 0; j < PERLEN; j++)
            out << bits[i][j];
    }
    for (int i = 0; i <= index; i++) {
        out << bits[vecLen][i];
    }
    out.close();

    if (!out) {
        throw std::runtime_error("failed while writing compressed stream: " + filename);
    }
}

// Reads `len` bits and returns them as an integer, most significant bit first.
int Bits::read(const int& len) {
    int decNum = 0;
    for (int i = 0; i < len; i++) {
        if (loadedInfo[++readIndex] == '1')
            decNum += 1 << (len - i - 1);
    }
    return decNum;
}
