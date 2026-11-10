#include "Bits.h"
#include <fstream>
#include <string.h>
#include <cmath>
#include <stdexcept>

Bits::Bits() {
    bits.emplace_back(0);
    vecLen = 0;
    index = -1;
    readLen = 0;
    loadedInfo = nullptr;
}

// Load a file and initialise the Bits object (used when decompressing)
void Bits::init(const std::string& filename, const int& size) {
    std::ifstream in(filename, std::ios::binary | std::ios::in);
    if (!in)
        throw std::runtime_error("Bits: cannot open '" + filename + "' for reading");

    // The container stores one ASCII '0'/'1' character per bit, so the file
    // is text. Size the buffer from the real file length instead of a
    // fixed guess: the previous fixed allocation could overflow on files
    // larger than the guess.
    in.seekg(0, std::ios::end);
    std::streamoff fileLen = in.tellg();
    if (fileLen < 0)
        throw std::runtime_error("Bits: cannot determine the size of '" + filename + "'");
    in.seekg(0, std::ios::beg);

    // `size` is a capacity hint in units of 1024 characters; keep at least
    // that much room so the historical call sites keep working.
    std::size_t capacity = static_cast<std::size_t>(fileLen) + 1;
    const std::size_t hint = static_cast<std::size_t>(pow(1024, size) * 10);
    if (capacity < hint)
        capacity = hint;

    if (loadedInfo)
        delete[] loadedInfo;
    loadedInfo = new char[capacity];
    std::memset(loadedInfo, 0, capacity);

    in.read(loadedInfo, fileLen);
    in.close();

    readLen = std::strlen(loadedInfo);
    vecLen = index =  0;
    readIndex = -1;
}

// Destructor, releases the loaded buffer
Bits::~Bits() {
    if (loadedInfo)
        delete[] loadedInfo;
}

// Append the low `len` bits of `info`, most significant bit first
void Bits::write(const int& info, const int& len) {
    for (int i = len - 1; i >= 0; i--) {
        if (info & (1 << i))
            add(1);
        else
            add(0);
    }
}


// Append a single bit to the current chunk
void Bits::add(const std::bitset<1>& bit) {
    if (++index == PERLEN) {
        bits.emplace_back(0);
        vecLen++;
        index = 0;
    }
    bits[vecLen][index] = bit[0];
}

// Write the bitstream to `filename`, one ASCII '0'/'1' character per bit
void Bits::save(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary | std::ios::trunc | std::ios::out);
    if (!out)
        throw std::runtime_error("Bits: cannot open '" + filename + "' for writing");
    for (int i = 0; i < vecLen ; i++) {
        for (int j = 0; j < PERLEN; j++)
            out << bits[i][j];
    }
    for (int i = 0; i <= index; i++) {
        out << bits[vecLen][i];
    }
    out.close();
}

// Read `len` bits and decode them as an unsigned integer
int Bits::read(const int& len) {
    int decNum = 0;
    for (int i = 0; i < len; i++) {
        if (loadedInfo[++readIndex] == '1')
            decNum += 1 << (len - i - 1);
    }
    return decNum;
}
