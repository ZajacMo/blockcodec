# blockcodec Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the duplicated `openCV_test` coursework into `blockcodec`, a reusable open-source C++17 image compression library with colour support, a real CLI, a benchmark with a JPEG baseline, GoogleTest coverage and multi-platform CI.

**Architecture:** A single `blockcodec` static library holds the algorithm and owns a new `.bcc` container that stores the channel count explicitly. Three applications consume it: a CLI, the existing Qt GUI, and a benchmark. The legacy `Compress_gray` implementation is retained under `tests/legacy/` as a test-only oracle that proves the refactor did not change single-channel behaviour.

**Tech Stack:** C++17, OpenCV 4.10, CMake 3.16+, GoogleTest (FetchContent), CLI11 (FetchContent), Qt 6 (optional), GitHub Actions.

## Global Constraints

- **All documentation and source comments are written in English.** Chinese source comments are removed. A Chinese README translation may remain.
- The library target is `blockcodec`; the exported alias is `blockcodec::blockcodec`; the namespace is `blockcodec`; the CLI executable is `blockcodec`.
- `include/blockcodec/` is the only installed directory. Nothing under `src/`, `apps/` or `tests/` is installed.
- Every public error is reported by throwing `std::runtime_error` with an English message. The library never calls `exit`, never prints to stdout, and never throws anything derived from `std::exception` other than `std::runtime_error`.
- The core is portable C++17: no `windows.h`, no `QueryPerformance*`, no compiler extensions. Timing uses `std::chrono::steady_clock`.
- The library depends on OpenCV only. It must not depend on Qt, CLI11, or GoogleTest.
- The library must not contain `main()`. Every application links the library, it does not compile the sources again.
- The existing reference vector `tests/vectors/gradient_16x16.*` must keep passing unchanged — it pins the legacy format's behaviour.
- Single-channel output must be unchanged by the refactor. The golden regression test enforces this.
- Metric names are explicit: `bpp` is bits per pixel, `cr` is a compression ratio, `byte_size` is the on-disk size. Never conflate bits with bytes.
- Every commit is scoped to one task and ends with a compiling, passing build.
- On this machine, MSVC must be entered through `vcvars64.bat`; `g++`, `clang++` and `cl` are not on `PATH`.

### Environment facts

| Fact | Value |
|------|-------|
| OpenCV | 4.10.0, `C:\Program Files\opencv` |
| OpenCV import library | `opencv_world4100.lib` (note: `4100`, not `410`) |
| OpenCV DLL directory | `C:\Program Files\opencv\build\x64\vc16\bin` |
| Qt | 6.6.2 `msvc2019_64` at `C:\Qt\6.6.2` — the `qmake` on `PATH` is Anaconda's Qt 5.15.2 and must not be used |
| MSVC | 2022 BuildTools 14.44, `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat` |
| CMake | 3.31.6 |
| Build command | `cmd.exe //c "<repo>\tools\build.bat"` (see Task 1) |

---

## File Structure

### Created

```
CMakePresets.json                     Developer presets for the common configurations
tools/build.bat                       One-shot build script: enters vcvars64 and builds
tools/test.bat                        One-shot test script: enters vcvars64 and runs ctest
include/blockcodec/blockcodec.hpp     Umbrella header pulling in the whole public API
include/blockcodec/codec.hpp          Codec class, CompressOptions, CompressResult
include/blockcodec/format.hpp         Container header struct and read/write helpers
include/blockcodec/segment.hpp        Segmentation visualisation helper
src/core/bitstream.hpp/.cpp           Dense bit writer and reader
src/core/pixel.hpp                    Channel-safe pixel access
src/core/block.hpp/.cpp               Abstract block interface
src/core/rblock.hpp/.cpp              Bilinear-interpolated block, multi-channel
src/core/codec.cpp                    Encoder and decoder driver
src/core/metrics.cpp                  BPP, CR and PSNR computation
src/core/pixel.hpp                    Channel-safe pixel access
src/format.cpp                        Container header serialisation
src/segment.cpp                       Segmentation overlay rendering
apps/cli/main.cpp                     blockcodec command line
apps/cli/commands.h/.cpp              Subcommand implementations
bench/bench_main.cpp                  Sigma sweep driver
bench/jpeg_baseline.h/.cpp            JPEG comparison at matched quality
tests/legacy/Compress_gray.h/.cpp     Legacy oracle, test-only, never installed
tests/legacy/Bits.h/.cpp
tests/legacy/Block.h/.cpp
tests/legacy/RBlock.h/.cpp
tests/unit/test_bitstream.cpp         Bit writer/reader unit tests
tests/unit/test_format.cpp            Container header unit tests
tests/unit/test_codec.cpp             Round-trip unit tests
tests/unit/test_golden.cpp            Golden regression against the legacy oracle
tests/unit/test_robustness.cpp        Malformed input handling
tests/unit/test_metrics.cpp           Metric formula checks
apps/prototype/Header.h/.cpp          CLion prototype, kept for reference, not built
apps/prototype/PROTOTYPE.md           How the prototype maps onto the new implementation
apps/prototype/CMakeLists.txt         Optional target, off by default
```

### Modified

```
CMakeLists.txt                        Rewritten around the new layout
.gitignore                            Add cmake-build-debug/ and the new build dirs
README.md                             Rewritten for blockcodec
README_CN.md                          Chinese translation of the new README
ALGORITHM.md                          Retained as the specification of the legacy .nam format
docs/ALGORITHM_BCC.md                 Specification of the new .bcc container
```

### Moved

```
compresswindow.cpp/.h/.ui   -> apps/qt/
uncompresswindow.cpp/.h/.ui -> apps/qt/
mainwindow.cpp/.h/.ui       -> apps/qt/
helpwindow.cpp/.h/.ui       -> apps/qt/
introducewindow.cpp/.h/.ui  -> apps/qt/
save.cpp/.h/.ui             -> apps/qt/
operatorsignals.cpp/.h      -> apps/qt/
main.cpp                    -> apps/qt/main.cpp
resourses.qrc               -> apps/qt/resourses.qrc
```

### Deleted

```
Bits.cpp/.h, Block.cpp/.h, RBlock.cpp/.h, Compress_gray.cpp/.h   Moved to tests/legacy/
openCV_test.pro                                                     Replaced by CMake
openCV_test.pro.user, openCV_test.pro.user.f90b369                 IDE state
```

---

## Task 1: Build and test tooling

The repository has no scripted build, so every later task needs one. This task
adds the scripts and the presets, and proves the existing suite still builds
and passes.

**Files:**
- Create: `tools/build.bat`
- Create: `tools/test.bat`
- Create: `CMakePresets.json`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: the existing `build/` directory produced by CMake.
- Produces: `tools/build.bat [config]` and `tools/test.bat [config]`, both callable as
  `cmd.exe //c "<repo>\tools\build.bat"` from any working directory.

- [ ] **Step 1: Write `tools/build.bat`**

```bat
@echo off
setlocal
REM Usage: tools\build.bat [Debug|Release]
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
if errorlevel 1 (
    echo Failed to initialise the MSVC environment.
    exit /b 1
)

cd /d "%~dp0.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b 1

cmake --build build --config %CONFIG% --parallel
exit /b %ERRORLEVEL%
```

- [ ] **Step 2: Write `tools/test.bat`**

```bat
@echo off
setlocal
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release

REM The OpenCV DLLs must be on PATH, or every test fails with 0xc0000135
REM (DLL not found). This does not affect the build, only the run.
set "PATH=C:\Program Files\opencv\build\x64\vc16\bin;%PATH%"

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
if errorlevel 1 (
    echo Failed to initialise the MSVC environment.
    exit /b 1
)

cd /d "%~dp0.."
ctest --test-dir build -C %CONFIG% --output-on-failure
exit /b %ERRORLEVEL%
```

- [ ] **Step 3: Write `CMakePresets.json`**

```json
{
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 21, "patch": 0 },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja Multi-Config",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_CXX_STANDARD": "17",
        "CMAKE_CXX_STANDARD_REQUIRED": "ON"
      }
    },
    { "name": "dev", "inherits": "base", "displayName": "Development" },
    { "name": "ci",  "inherits": "base", "displayName": "CI" }
  ],
  "buildPresets": [
    { "name": "dev", "configurePreset": "dev" },
    { "name": "ci",  "configurePreset": "ci" }
  ],
  "testPresets": [
    { "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true } },
    { "name": "ci",  "configurePreset": "ci", "output": { "outputOnFailure": true } }
  ]
}
```

- [ ] **Step 4: Extend `.gitignore`**

Add these lines to the existing `.gitignore`:

```gitignore
# CLion's default build directory name
cmake-build-debug/
cmake-build-*/
# Preset build trees
build/*/
# Probe and scratch binaries
*.exe
!tools/*.exe
```

- [ ] **Step 5: Verify the build script works**

Run: `cmd.exe //c "F:\\Projects\\01_courses\\cpp\\QtProject\\openCV_test\\tools\\build.bat"`
Expected: exit code 0, and the existing targets rebuild.

- [ ] **Step 6: Verify the test script works**

Run: `cmd.exe //c "F:\\Projects\\01_courses\\cpp\\QtProject\\openCV_test\\tools\\test.bat"`
Expected: exit code 0. The current suite registers one test, `codec_selftest`, which
reports `16 checks, 0 failed`.

- [ ] **Step 7: Commit**

```bash
git add tools/build.bat tools/test.bat CMakePresets.json .gitignore
git commit -m "build: add scripted build and test entry points"
```

---

## Task 2: Repository rename and legacy import

Renames the GitHub repository, imports the two sibling copies so nothing is
lost, and brings the CLion prototype in as a tracked, non-built directory.

**Files:**
- Modify: `git remote set-url origin`
- Create: `legacy/vs-console/` (the contents of `cpp/VS_C++Project/test_openCV`)
- Create: `apps/prototype/Header.h`, `apps/prototype/Header.cpp`, `apps/prototype/PROTOTYPE.md`, `apps/prototype/CMakeLists.txt`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: the sibling directories `F:\Projects\01_courses\cpp\VS_C++Project\test_openCV`
  and `F:\Projects\01_courses\cpp\CLion\C++Programming\openCV_test`.
- Produces: `legacy/vs-console/` and `apps/prototype/` in the working tree.

- [ ] **Step 1: Update the remote URL**

Run:

```bash
git remote set-url origin https://github.com/ZajacMo/OpenCV-Image-Processing-and-Compression-Tool-Qt.git
git ls-remote origin HEAD
```

Expected: the hash of `main`, proving the renamed remote resolves.

- [ ] **Step 2: Copy the VS sources into `legacy/vs-console/`**

```bash
mkdir -p legacy/vs-console
cp /f/Projects/01_courses/cpp/VS_C++Project/test_openCV/*.cpp \
   /f/Projects/01_courses/cpp/VS_C++Project/test_openCV/*.h \
   /f/Projects/01_courses/cpp/VS_C++Project/test_openCV/*.sln \
   /f/Projects/01_courses/cpp/VS_C++Project/test_openCV/*.vcxproj* \
   legacy/vs-console/
```

Expected: the legacy console implementation is present for reference. It is not
compiled by the top-level CMake.

- [ ] **Step 3: Copy the CLion prototype into `apps/prototype/`, without its build output**

```bash
mkdir -p apps/prototype
cp /f/Projects/01_courses/cpp/CLion/C++Programming/openCV_test/Header.h \
   /f/Projects/01_courses/cpp/CLion/C++Programming/openCV_test/Header.cpp \
   /f/Projects/01_courses/cpp/CLion/C++Programming/openCV_test/CMakeLists.txt \
   apps/prototype/
```

The `cmake-build-debug/` directory (353 KB of CMake cache and object files) is
deliberately not copied.

- [ ] **Step 4: Write `apps/prototype/PROTOTYPE.md`**

```markdown
# The original prototype

This directory holds the first implementation of the codec, written as free
functions in `Header.h` / `Header.cpp` and built with CLion and CMake. It is
kept for reference and is **not** part of the library build.

It predates the class-based implementation in two ways that matter:

- Its functions are free functions (`RNAMCEncoding`, `RNAMCDecoding`, `PSNR`,
  `BPP`, `segmentDisplay`) rather than methods on a class.
- It uses the term `EPSILON` for the block tolerance. The library calls the
  same parameter `sigma`.

The two implementations of the algorithm correspond as follows:

| Prototype (`Header.cpp`) | Library |
|--------------------------|---------|
| `RNAMCEncoding`          | `blockcodec::Codec::compress` |
| `RNAMCDecoding`          | `blockcodec::Codec::decompress` |
| `PSNR`                   | `blockcodec::psnr` |
| `BPP`                    | `blockcodec::bpp` |
| `segmentDisplay`         | `blockcodec::segmentation_overlay` |
| `MyTimer`                | `std::chrono::steady_clock` in the library |

Two things in the prototype are worth keeping in mind:

- Its `BPP()` is the closest to correct of the three historical
  implementations, and is the definition the library adopts.
- Its `segmentDisplay()` draws the block boundaries. The library exposes the
  same capability as `segmentation_overlay`.
```

- [ ] **Step 5: Write `apps/prototype/CMakeLists.txt`**

```cmake
# The prototype is kept for reference only. It is not built by default:
# it duplicates the algorithm that now lives in src/core.
add_executable(blockcodec_prototype EXCLUDE_FROM_ALL
    Header.cpp
    Header.h
)
target_include_directories(blockcodec_prototype PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(blockcodec_prototype PRIVATE ${OpenCV_LIBS})
```

- [ ] **Step 6: Confirm the prototype's build output is not tracked**

Run:

```bash
git status --short
grep -n "cmake-build-debug" .gitignore
```

Expected: `.gitignore` contains `cmake-build-debug/`, and no `cmake-build-debug`
directory appears in the status.

- [ ] **Step 7: Commit**

```bash
git add legacy apps/prototype .gitignore
git commit -m "chore: import the VS console and CLion prototype for reference"
```

---

## Task 3: The dense bit stream

The legacy `Bits` class stores one ASCII character per bit, which makes every
file eight times larger than the bitstream it carries, and its `write` shifts
past the width of `int`. This task introduces the replacement.

**Files:**
- Create: `include/blockcodec/bitstream.hpp`
- Create: `src/core/bitstream.hpp`
- Create: `src/core/bitstream.cpp`
- Test: `tests/unit/test_bitstream.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  namespace blockcodec {
  class BitWriter {
  public:
      void write(std::uint64_t value, int bits);
      void write_bit(bool bit);
      std::size_t bit_count() const;
      // Appends the written bits to `out`, most significant bit first,
      // packed eight to a byte. The final byte is zero padded.
      void to_bytes(std::vector<std::uint8_t>& out) const;
  };

  class BitReader {
  public:
      explicit BitReader(const std::uint8_t* data, std::size_t size);
      std::uint64_t read(int bits);
      bool read_bit();
      std::size_t bit_position() const;
      std::size_t bit_count() const;   // total bits available
      bool exhausted() const;
  };
  }
  ```

- [ ] **Step 1: Write the failing test**

Create `tests/unit/test_bitstream.cpp`:

```cpp
#include "blockcodec/bitstream.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using blockcodec::BitReader;
using blockcodec::BitWriter;

TEST(BitStream, WritesBitsMostSignificantFirst) {
    BitWriter w;
    w.write_bit(true);
    w.write_bit(false);
    w.write_bit(true);
    ASSERT_EQ(w.bit_count(), 3u);

    std::vector<std::uint8_t> bytes;
    w.to_bytes(bytes);
    ASSERT_EQ(bytes.size(), 1u);
    // 1010 0000
    EXPECT_EQ(bytes[0], 0xA0u);
}

TEST(BitStream, RoundTripsEveryFieldWidth) {
    for (int bits = 1; bits <= 64; ++bits) {
        const std::uint64_t mask =
            (bits == 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << bits) - 1);

        BitWriter w;
        w.write(0x5A5A5A5A5A5A5A5AULL & mask, bits);
        w.write(mask, bits);

        std::vector<std::uint8_t> bytes;
        w.to_bytes(bytes);

        BitReader r(bytes.data(), bytes.size());
        EXPECT_EQ(r.read(bits), 0x5A5A5A5A5A5A5A5AULL & mask) << "bits=" << bits;
        EXPECT_EQ(r.read(bits), mask) << "bits=" << bits;
        EXPECT_TRUE(r.exhausted()) << "bits=" << bits;
    }
}

TEST(BitStream, RoundTripsAcrossByteBoundaries) {
    BitWriter w;
    for (int i = 0; i < 300; ++i) {
        w.write(static_cast<std::uint64_t>(i), 9);
    }

    std::vector<std::uint8_t> bytes;
    w.to_bytes(bytes);

    BitReader r(bytes.data(), bytes.size());
    for (int i = 0; i < 300; ++i) {
        EXPECT_EQ(r.read(9), static_cast<std::uint64_t>(i)) << "i=" << i;
    }
    EXPECT_TRUE(r.exhausted());
}

TEST(BitStream, WritesTheFullSixtyFourBitRange) {
    // The legacy implementation evaluated `1 << 63`, which is undefined.
    BitWriter w;
    w.write(0xFFFFFFFFFFFFFFFFULL, 64);

    std::vector<std::uint8_t> bytes;
    w.to_bytes(bytes);
    ASSERT_EQ(bytes.size(), 8u);

    BitReader r(bytes.data(), bytes.size());
    EXPECT_EQ(r.read(64), 0xFFFFFFFFFFFFFFFFULL);
}

TEST(BitStream, ReadingPastTheEndThrows) {
    const std::vector<std::uint8_t> bytes{0xFF};
    BitReader r(bytes.data(), bytes.size());
    EXPECT_THROW(r.read(9), std::runtime_error);
}

TEST(BitStream, ZeroBitCountIsRejected) {
    BitWriter w;
    EXPECT_THROW(w.write(1, 0), std::runtime_error);
    EXPECT_THROW(w.write(1, 65), std::runtime_error);
}
```

- [ ] **Step 2: Add the test to CMake and confirm it fails**

In the top-level `CMakeLists.txt`, inside the existing `if(BUILD_TESTING)` block,
add:

```cmake
    add_executable(test_bitstream tests/unit/test_bitstream.cpp)
    target_link_libraries(test_bitstream PRIVATE blockcodec gtest_main)
    add_test(NAME test_bitstream COMMAND test_bitstream)
```

Run: `cmd.exe //c "F:\\Projects\\01_courses\\cpp\\QtProject\\openCV_test\\tools\\build.bat"`
Expected: the build fails, because `blockcodec/bitstream.hpp` does not exist yet.

- [ ] **Step 3: Write the public header**

Create `include/blockcodec/bitstream.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace blockcodec {

// Writes a sequence of bits, most significant bit first, into a byte buffer.
//
// The legacy implementation stored one ASCII character per bit, which made
// the file eight times larger than the bitstream it carried. This writer
// packs eight bits to a byte.
class BitWriter {
public:
    // Appends the low `bits` bits of `value`.
    // Throws std::runtime_error if `bits` is not in [1, 64].
    void write(std::uint64_t value, int bits);

    void write_bit(bool bit);

    std::size_t bit_count() const { return bits_.size(); }

    // Appends the written bits to `out`, packed eight to a byte.
    // The final byte is zero padded.
    void to_bytes(std::vector<std::uint8_t>& out) const;

private:
    std::vector<bool> bits_;
};

// Reads a sequence of bits written by BitWriter.
class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size);

    // Reads `bits` bits and returns them as an unsigned integer.
    // Throws std::runtime_error if fewer than `bits` bits remain.
    std::uint64_t read(int bits);

    bool read_bit();

    std::size_t bit_position() const { return position_; }

    // The total number of readable bits.
    std::size_t bit_count() const { return size_ * 8; }

    bool exhausted() const { return position_ >= bit_count(); }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t position_ = 0;
};

}  // namespace blockcodec
```

- [ ] **Step 4: Write the implementation**

Create `src/core/bitstream.hpp`:

```cpp
#pragma once

#include "blockcodec/bitstream.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace blockcodec {

class BitWriter {
public:
    void write(std::uint64_t value, int bits);
    void write_bit(bool bit);
    std::size_t bit_count() const { return bits_.size(); }
    void to_bytes(std::vector<std::uint8_t>& out) const;

private:
    std::vector<bool> bits_;
};

class BitReader {
public:
    BitReader(const std::uint8_t* data, std::size_t size);
    std::uint64_t read(int bits);
    bool read_bit();
    std::size_t bit_position() const { return position_; }
    std::size_t bit_count() const { return size_ * 8; }
    bool exhausted() const { return position_ >= bit_count(); }

private:
    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t position_ = 0;
};

}  // namespace blockcodec
```

Create `src/core/bitstream.cpp`:

```cpp
#include "core/bitstream.hpp"

#include <stdexcept>

namespace blockcodec {

void BitWriter::write(std::uint64_t value, int bits) {
    if (bits < 1 || bits > 64) {
        throw std::runtime_error("BitWriter::write: the field width must be in [1, 64]");
    }

    // Shift within the width of the type. The legacy implementation used
    // `1 << i` on an int, which is undefined once i reaches 31.
    const std::uint64_t mask =
        (bits == 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << bits) - 1);
    value &= mask;

    for (int i = bits - 1; i >= 0; --i) {
        bits_.push_back(((value >> i) & 1u) != 0);
    }
}

void BitWriter::write_bit(bool bit) {
    bits_.push_back(bit);
}

void BitWriter::to_bytes(std::vector<std::uint8_t>& out) const {
    std::size_t whole_bytes = bits_.size() / 8;
    std::size_t remainder = bits_.size() % 8;

    out.reserve(out.size() + whole_bytes + (remainder ? 1 : 0));

    for (std::size_t i = 0; i < whole_bytes; ++i) {
        std::uint8_t byte = 0;
        for (int b = 0; b < 8; ++b) {
            if (bits_[i * 8 + static_cast<std::size_t>(b)]) {
                byte |= static_cast<std::uint8_t>(1u << (7 - b));
            }
        }
        out.push_back(byte);
    }

    if (remainder != 0) {
        std::uint8_t byte = 0;
        for (std::size_t b = 0; b < remainder; ++b) {
            if (bits_[whole_bytes * 8 + b]) {
                byte |= static_cast<std::uint8_t>(1u << (7 - static_cast<int>(b)));
            }
        }
        out.push_back(byte);
    }
}

BitReader::BitReader(const std::uint8_t* data, std::size_t size)
    : data_(data), size_(data == nullptr ? 0 : size) {}

std::uint64_t BitReader::read(int bits) {
    if (bits < 1 || bits > 64) {
        throw std::runtime_error("BitReader::read: the field width must be in [1, 64]");
    }
    if (bit_count() - position_ < static_cast<std::size_t>(bits)) {
        throw std::runtime_error("BitReader::read: the stream ended unexpectedly");
    }

    std::uint64_t value = 0;
    for (int i = 0; i < bits; ++i) {
        const std::size_t index = position_ + static_cast<std::size_t>(i);
        const std::uint8_t byte = data_[index / 8];
        const std::uint8_t bit = (byte >> (7 - static_cast<int>(index % 8))) & 1u;
        value = (value << 1) | static_cast<std::uint64_t>(bit);
    }
    position_ += static_cast<std::size_t>(bits);
    return value;
}

bool BitReader::read_bit() {
    return read(1) != 0;
}

}  // namespace blockcodec
```

- [ ] **Step 5: Run the tests and confirm they pass**

Run: `cmd.exe //c "F:\\Projects\\01_courses\\cpp\\QtProject\\openCV_test\\tools\\test.bat"`
Expected: `test_bitstream` reports 6 passing tests.

- [ ] **Step 6: Commit**

```bash
git add include/blockcodec/bitstream.hpp src/core/bitstream.hpp src/core/bitstream.cpp tests/unit/test_bitstream.cpp CMakeLists.txt
git commit -m "feat: add a dense bit stream writer and reader"
```

---

## Task 4: The container format

Defines the `.bcc` header and its serialisation. The header stores the channel
count explicitly, which removes the `isColorful` heuristic that misreported
every image wider than it is tall.

**Files:**
- Create: `include/blockcodec/format.hpp`
- Create: `src/format.cpp`
- Test: `tests/unit/test_format.cpp`

**Interfaces:**
- Consumes: `blockcodec::BitWriter` and `blockcodec::BitReader` from Task 3.
- Produces:
  ```cpp
  namespace blockcodec {

  constexpr std::uint8_t kFormatVersion = 1;

  struct ContainerHeader {
      std::uint32_t rows = 0;
      std::uint32_t cols = 0;       // pixel width, not including channels
      std::uint32_t block_count = 0;
      std::uint8_t  channels = 1;
      std::uint8_t  depth_bits = 8; // bits per colour sample
      bool          colour = false;
  };

  // Upper bound on the number of pixels a container may declare.
  constexpr std::uint64_t kMaxPixels = 1ULL << 28;

  // Serialises `header` and `payload` into a complete container.
  std::vector<std::uint8_t> write_container(const ContainerHeader& header,
                                            const std::vector<std::uint8_t>& payload);

  // Parses a container. Throws std::runtime_error on any malformed input.
  ContainerHeader read_container(const std::vector<std::uint8_t>& data,
                                 std::vector<std::uint8_t>& payload);
  }
  ```

- [ ] **Step 1: Write the failing test**

Create `tests/unit/test_format.cpp`:

```cpp
#include "blockcodec/format.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using blockcodec::ContainerHeader;
using blockcodec::read_container;
using blockcodec::write_container;

TEST(Format, RoundTripsTheHeader) {
    ContainerHeader header;
    header.rows = 512;
    header.cols = 512;
    header.channels = 3;
    header.depth_bits = 8;
    header.colour = true;
    header.block_count = 42;

    const std::vector<std::uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF};
    const std::vector<std::uint8_t> container = write_container(header, payload);

    std::vector<std::uint8_t> decoded_payload;
    const ContainerHeader decoded = read_container(container, decoded_payload);

    EXPECT_EQ(decoded.rows, 512u);
    EXPECT_EQ(decoded.cols, 512u);
    EXPECT_EQ(decoded.channels, 3u);
    EXPECT_EQ(decoded.depth_bits, 8u);
    EXPECT_TRUE(decoded.colour);
    EXPECT_EQ(decoded.block_count, 42u);
    EXPECT_EQ(decoded_payload, payload);
}

TEST(Format, RejectsAnEmptyContainer) {
    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container({}, payload), std::runtime_error);
}

TEST(Format, RejectsABadMagic) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    std::vector<std::uint8_t> container = write_container(header, {0x00});
    container[0] = 'X';

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsAnUnknownVersion) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    std::vector<std::uint8_t> container = write_container(header, {0x00});
    container[4] = 99;

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsImplausibleDimensions) {
    ContainerHeader header;
    header.rows = 100000;
    header.cols = 100000;
    std::vector<std::uint8_t> container = write_container(header, {0x00});

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsAnUnsupportedChannelCount) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    header.channels = 7;
    std::vector<std::uint8_t> container = write_container(header, {0x00});

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(container, payload), std::runtime_error);
}

TEST(Format, RejectsATruncatedContainer) {
    ContainerHeader header;
    header.rows = 4;
    header.cols = 4;
    std::vector<std::uint8_t> container = write_container(header, {0x00});

    std::vector<std::uint8_t> payload;
    EXPECT_THROW(read_container(std::vector<std::uint8_t>(container.begin(),
                                                          container.begin() + 8),
                                 payload),
                 std::runtime_error);
}
```

- [ ] **Step 2: Register the test and confirm it fails**

Add to `CMakeLists.txt` inside `if(BUILD_TESTING)`:

```cmake
    add_executable(test_format tests/unit/test_format.cpp)
    target_link_libraries(test_format PRIVATE blockcodec gtest_main)
    add_test(NAME test_format COMMAND test_format)
```

Run the build. Expected: failure, `blockcodec/format.hpp` does not exist.

- [ ] **Step 3: Write the public header**

Create `include/blockcodec/format.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace blockcodec {

// The version of the .bcc container written by this library.
constexpr std::uint8_t kFormatVersion = 1;

// The largest number of pixels a container may declare. The dimensions come
// from the file, so they are validated before they size an allocation.
constexpr std::uint64_t kMaxPixels = 1ULL << 28;

// The header of a .bcc container.
//
// The channel count is stored explicitly. The legacy format inferred it from
// `cols > rows`, which misreported every image wider than it is tall.
struct ContainerHeader {
    std::uint32_t rows = 0;
    std::uint32_t cols = 0;        // pixel width, excluding channels
    std::uint8_t  channels = 1;
    std::uint8_t  depth_bits = 8;  // bits per colour sample
    bool          colour = false;
};

// Builds a complete container: the header followed by the packed payload.
std::vector<std::uint8_t> write_container(const ContainerHeader& header,
                                            const std::vector<std::uint8_t>& payload);

// Parses a container. Throws std::runtime_error if the container is
// malformed, truncated, or declares implausible dimensions.
ContainerHeader read_container(const std::vector<std::uint8_t>& data,
                                 std::vector<std::uint8_t>& payload);

}  // namespace blockcodec
```

- [ ] **Step 4: Write the implementation**

Create `src/format.cpp`:

```cpp
#include "blockcodec/format.hpp"

#include <cstring>
#include <stdexcept>

namespace blockcodec {
namespace {

constexpr char kMagic[4] = {'B', 'C', 'C', '\0'};
constexpr std::size_t kHeaderSize = 20;

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

std::uint32_t read_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::vector<std::uint8_t> write_container(const ContainerHeader& header,
                                           const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderSize + payload.size());

    for (char c : kMagic) {
        out.push_back(static_cast<std::uint8_t>(c));
    }
    out.push_back(kFormatVersion);
    out.push_back(header.colour ? 1u : 0u);
    append_u32(out, header.rows);
    append_u32(out, header.cols);
    out.push_back(header.channels);
    out.push_back(header.depth_bits);
    append_u32(out, header.block_count);

    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

ContainerHeader read_container(const std::vector<std::uint8_t>& data,
                                std::vector<std::uint8_t>& payload) {
    if (data.size() < kHeaderSize) {
        throw std::runtime_error("blockcodec: the container is truncated");
    }
    if (std::memcmp(data.data(), kMagic, sizeof(kMagic)) != 0) {
        throw std::runtime_error("blockcodec: the container has a bad magic number");
    }
    if (data[4] != kFormatVersion) {
        throw std::runtime_error("blockcodec: unsupported container version");
    }

    ContainerHeader header;
    header.colour = (data[5] & 1u) != 0;
    header.rows = read_u32(data.data() + 6);
    header.cols = read_u32(data.data() + 10);
    header.channels = data[14];
    header.depth_bits = data[15];
    header.block_count = read_u32(data.data() + 16);

    if (header.rows == 0 || header.cols == 0) {
        throw std::runtime_error("blockcodec: the container declares an empty image");
    }
    if (header.channels < 1 || header.channels > 4) {
        throw std::runtime_error("blockcodec: the container declares an unsupported channel count");
    }
    if (header.depth_bits != 8 && header.depth_bits != 16 && header.depth_bits != 32) {
        throw std::runtime_error("blockcodec: the container declares an unsupported colour depth");
    }

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(header.rows) * static_cast<std::uint64_t>(header.cols);
    if (pixels > kMaxPixels) {
        throw std::runtime_error("blockcodec: the container declares an implausible image size");
    }

    payload.assign(data.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), data.end());
    return header;
}

}  // namespace blockcodec
```

- [ ] **Step 5: Run the tests**

Run the test script. Expected: `test_format` reports 7 passing tests.

- [ ] **Step 6: Commit**

```bash
git add include/blockcodec/format.hpp src/format.cpp tests/unit/test_format.cpp CMakeLists.txt
git commit -m "feat: define the .bcc container header"
```

---

## Task 5: The core codec with colour support

The heart of the work. Rewrites the block model so that pixels and channels
are separate axes, and makes the API value-semantic.

**Files:**
- Create: `include/blockcodec/codec.hpp`
- Create: `src/core/block.hpp`, `src/core/block.cpp`
- Create: `src/core/rblock.hpp`, `src/core/rblock.cpp`
- Create: `src/core/codec.cpp`
- Create: `include/blockcodec/blockcodec.hpp`
- Test: `tests/unit/test_codec.cpp`

**Interfaces:**
- Consumes: `blockcodec::BitWriter`, `blockcodec::BitReader` (Task 3),
  `blockcodec::write_container`, `blockcodec::read_container` (Task 4).
- Produces:
  ```cpp
  namespace blockcodec {

  struct CompressOptions {
      int sigma = 10;   // the tolerance of the block test
  };

  struct CompressResult {
      std::size_t bit_size = 0;      // bits in the bitstream
      std::size_t byte_size = 0;     // bytes in the container
      double      bpp = 0.0;         // bits per pixel
      double      cr = 0.0;          // compression ratio
      double      psnr = 0.0;        // dB, infinity for a perfect reconstruction
      double      compress_ms = 0.0;
      double      decompress_ms = 0.0;
      std::size_t block_count = 0;
  };

  class Codec {
  public:
      explicit Codec(CompressOptions options = {});

      // Encodes `source` and returns the container.
      // Throws std::runtime_error if the image is empty or not 8-bit.
      std::vector<std::uint8_t> compress(const cv::Mat& source,
                                         CompressResult* result = nullptr);

      // Decodes a container produced by compress().
      // Throws std::runtime_error on malformed input.
      cv::Mat decompress(const std::vector<std::uint8_t>& container,
                         CompressResult* result = nullptr);

      cv::Mat decompress(const std::string& path, CompressResult* result = nullptr);

  private:
      CompressOptions options_;
  };

  // The peak signal-to-noise ratio between two images, in decibels.
  // Returns infinity when the reconstruction is identical.
  double psnr(const cv::Mat& a, const cv::Mat& b);
  }
  ```

- [ ] **Step 1: Write the failing test**

Create `tests/unit/test_codec.cpp`:

```cpp
#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"

#include <gtest/gtest.h>

#include <opencv2/opencv.hpp>

#include <cmath>
#include <string>
#include <vector>

using blockcodec::Codec;
using blockcodec::CompressResult;
using blockcodec::set_sample_at;

namespace {

cv::Mat make_gradient(int rows, int cols, int channels) {
    cv::Mat image(rows, cols, CV_8UC(channels));
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            for (int c = 0; c < channels; ++c) {
                set_sample_at(image, y, x, c, (x * 3 + y * 5 + c * 11) & 0xFF);
            }
        }
    }
    return image;
}

}  // namespace

TEST(Codec, RoundTripsGreyscale) {
    const cv::Mat source = make_gradient(64, 64, 1);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    const cv::Mat decoded = codec.decompress(container, &result);

    EXPECT_EQ(decoded.rows, source.rows);
    EXPECT_EQ(decoded.cols, source.cols);
    EXPECT_EQ(decoded.channels(), 1u);
    EXPECT_GT(result.psnr, 30.0);
    EXPECT_GT(result.block_count, 0u);
}

TEST(Codec, RoundTripsColour) {
    const cv::Mat source = make_gradient(64, 64, 3);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    const cv::Mat decoded = codec.decompress(container, &result);

    // The legacy implementation flattened the channels into the width, so a
    // 64x64 colour image decoded as 192x64 greyscale.
    EXPECT_EQ(decoded.rows, source.rows);
    EXPECT_EQ(decoded.cols, source.cols);
    EXPECT_EQ(decoded.channels(), 3u);
    EXPECT_GT(result.psnr, 30.0);
}

TEST(Codec, KeepsLandscapeGreyscaleGreyscale) {
    // The legacy decoder inferred colour from `cols > rows`, so a 256x128
    // greyscale image was reported as colour. The assertion therefore
    // targets the channel count carried by the container: asserting on the
    // decoded matrix would pass even when the inference is wrong, because
    // the legacy decoder always allocated a single channel result.
    const cv::Mat source = make_gradient(128, 256, 1);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    std::vector<std::uint8_t> payload;
    const ContainerHeader header = read_container(container, payload);
    EXPECT_EQ(header.channels, 1u);
    EXPECT_FALSE(header.colour);

    const cv::Mat decoded = codec.decompress(container, &result);
    EXPECT_EQ(decoded.cols, 256);
    EXPECT_EQ(decoded.rows, 128);
    EXPECT_EQ(decoded.channels(), 1u);
}

TEST(Codec, RejectsAnEmptyImage) {
    Codec codec;
    EXPECT_THROW(codec.compress(cv::Mat()), std::runtime_error);
}

TEST(Codec, RejectsANonEightBitImage) {
    cv::Mat source(16, 16, CV_32F);
    source.setTo(cv::Scalar(1.0));
    Codec codec;
    EXPECT_THROW(codec.compress(source), std::runtime_error);
}

TEST(Codec, RejectsAGarbageContainer) {
    Codec codec;
    EXPECT_THROW(codec.decompress(std::vector<std::uint8_t>{1, 2, 3, 4, 5}),
                 std::runtime_error);
}

TEST(Codec, ReportsBitsPerPixelConsistently) {
    const cv::Mat source = make_gradient(64, 64, 1);

    Codec codec;
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    const double expected_bpp =
        static_cast<double>(result.bit_size) / (64.0 * 64.0);
    EXPECT_NEAR(result.bpp, expected_bpp, 1e-9);
    EXPECT_GT(result.byte_size, 0u);
    EXPECT_LE(result.byte_size, result.bit_size / 8 + 16);
}
```

- [ ] **Step 2: Register the test and confirm it fails**

Add to `CMakeLists.txt` inside `if(BUILD_TESTING)`:

```cmake
    add_executable(test_codec tests/unit/test_codec.cpp)
    target_link_libraries(test_codec PRIVATE blockcodec gtest_main)
    add_test(NAME test_codec COMMAND test_codec)
```

Run the build. Expected: failure, `blockcodec/codec.hpp` does not exist.

- [ ] **Step 3: Write the public header**

Create `include/blockcodec/codec.hpp`:

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace blockcodec {

// The tolerance of the block test.
//
// A small sigma accepts only near-uniform blocks, which produces large files
// at a high PSNR. A large sigma accepts coarse blocks, which produces small
// files at a lower PSNR.
struct CompressOptions {
    int sigma = 10;
};

// The measurements taken during an encode or decode.
struct CompressResult {
    std::size_t bit_size = 0;       // bits in the bitstream
    std::size_t byte_size = 0;      // bytes in the container
    double      bpp = 0.0;          // bits per pixel
    double      cr = 0.0;           // compression ratio
    double      psnr = 0.0;         // dB, infinity for a perfect reconstruction
    double      compress_ms = 0.0;
    double      decompress_ms = 0.0;
    std::size_t block_count = 0;
};

// Encodes and decodes images with a block based codec.
//
// The codec is stateless: each call is independent, and the results are
// returned rather than held.
class Codec {
public:
    explicit Codec(CompressOptions options = {});

    // Encodes `source` and returns the container.
    // Throws std::runtime_error if the image is empty or is not 8-bit.
    std::vector<std::uint8_t> compress(const cv::Mat& source,
                                       CompressResult* result = nullptr);

    // Decodes a container produced by compress().
    // Throws std::runtime_error on malformed input.
    cv::Mat decompress(const std::vector<std::uint8_t>& container,
                       CompressResult* result = nullptr);

    cv::Mat decompress(const std::string& path, CompressResult* result = nullptr);

    const CompressOptions& options() const { return options_; }

private:
    CompressOptions options_;
};

// The peak signal-to-noise ratio between two images, in decibels.
// Returns infinity when the reconstruction is identical to the reference.
double psnr(const cv::Mat& a, const cv::Mat& b);

}  // namespace blockcodec
```

Create `include/blockcodec/blockcodec.hpp`:

```cpp
#pragma once

// The umbrella header of the blockcodec library.

#include "blockcodec/bitstream.hpp"
#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"
#include "blockcodec/segment.hpp"
```

- [ ] **Step 4: Write the channel-safe pixel accessor**

Create `src/core/pixel.hpp`:

```cpp
#pragma once

#include <opencv2/core.hpp>

namespace blockcodec {

// Reads the sample of channel `c` at (column, row).
//
// `Mat::at<cv::Vec3b>` is only valid on a three channel matrix. Calling it
// on a greyscale matrix reads past the element, so the samples are read
// through the row pointer instead, which works for any channel count.
inline int sample_at(const cv::Mat& image, int row, int column, int c) {
    return static_cast<int>(image.ptr<uchar>(row)[column * image.channels() + c]);
}

// Writes the sample of channel `c` at (column, row).
inline void set_sample_at(cv::Mat& image, int row, int column, int c, int value) {
    image.ptr<uchar>(row)[column * image.channels() + c] =
        static_cast<uchar>(value);
}

}  // namespace blockcodec
```

- [ ] **Step 5: Write the block interface**

Create `src/core/block.hpp`:

```cpp
#pragma once

#include <opencv2/core.hpp>

namespace blockcodec {

// A rectangular region of an image.
//
// The region is described by its four corner samples; the interior is
// reconstructed by bilinear interpolation. A block covers one or more colour
// channels.
//
// The corners use the OpenCV convention: `x` is the column and `y` is the
// row.
class Block {
public:
    Block() = default;
    Block(cv::Point lt, cv::Point rb, int channels);

    virtual ~Block() = default;

    // Returns true when every pixel inside the block differs from the
    // interpolated value by no more than `sigma`.
    virtual bool is_block(const cv::Mat& image, const bool* marked,
                          int rows, int cols, int sigma) const = 0;

    // Marks every pixel inside the block as covered.
    virtual void mark(bool* marked, int rows, int cols) const;

    cv::Point lt() const { return lt_; }
    cv::Point rb() const { return rb_; }
    int channels() const { return channels_; }

protected:
    cv::Point lt_;
    cv::Point rb_;
    int channels_ = 1;
};

}  // namespace blockcodec
```

Create `src/core/block.cpp`:

```cpp
#include "core/block.hpp"

namespace blockcodec {

Block::Block(cv::Point lt, cv::Point rb, int channels)
    : lt_(lt), rb_(rb), channels_(channels) {}

void Block::mark(bool* marked, int rows, int cols) const {
    for (int y = lt_.y; y <= rb_.y; ++y) {
        for (int x = lt_.x; x <= rb_.x; ++x) {
            if (y >= 0 && y < rows && x >= 0 && x < cols) {
                marked[static_cast<std::size_t>(y) * cols + x] = true;
            }
        }
    }
}

}  // namespace blockcodec
```

- [ ] **Step 6: Write the interpolating block**

Create `src/core/rblock.hpp`:

```cpp
#pragma once

#include "core/block.hpp"

#include <cstdint>
#include <vector>

namespace blockcodec {

// A block whose interior is reconstructed by bilinear interpolation between
// its four corner samples.
//
// The corner samples are stored per channel, so the same class describes
// both greyscale and colour blocks. The block test compares the largest
// deviation across the channels against the tolerance, which for a single
// channel image reduces to the greyscale rule exactly.
//
// The interpolation runs vertically first and then horizontally, and each
// step truncates towards zero. The legacy implementation interpolated in
// the same order with the same truncation, so the reconstruction of a
// single channel image is unchanged.
class RBlock : public Block {
public:
    RBlock() = default;
    RBlock(cv::Point lt, cv::Point rb, int channels);
    RBlock(cv::Point lt, cv::Point rb, const std::vector<std::uint8_t>& corners);

    bool is_block(const cv::Mat& image, const bool* marked,
                  int rows, int cols, int sigma) const override;

    // Restores the block into `image`.
    void restore(cv::Mat& image) const;

    // The four corner samples, in the order top-left, top-right,
    // bottom-left, bottom-right, each holding `channels` values.
    const std::vector<std::uint8_t>& corners() const { return corners_; }

    void set_corners(const std::vector<std::uint8_t>& corners) { corners_ = corners; }

    // The value of the block at (column, row) for channel `c`.
    int interpolated(int column, int row, int c) const;

private:
    std::vector<std::uint8_t> corners_;
};

}  // namespace blockcodec
```

Create `src/core/rblock.cpp`:

```cpp
#include "core/rblock.hpp"
#include "core/pixel.hpp"

#include <algorithm>
#include <cmath>

namespace blockcodec {
namespace {

// Linear interpolation between `a` and `b` at the ratio `rate`.
//
// The result is truncated to an unsigned char, exactly as the legacy helper
// did: it returned a uchar, so every intermediate value was rounded down
// before it was used again. The difference is visible in the golden test.
int interpolate(int a, int b, double rate) {
    return static_cast<int>(
        static_cast<unsigned char>(a + (b - a) * rate));
}

// The ratio of `i` within the range [a, b].
double ratio(int a, int b, int i) {
    if (b == a) {
        return 0.0;
    }
    return static_cast<double>(i - a) / static_cast<double>(b - a);
}

}  // namespace

RBlock::RBlock(cv::Point lt, cv::Point rb, int channels)
    : Block(lt, rb, channels),
      corners_(static_cast<std::size_t>(channels) * 4, 0) {}

RBlock::RBlock(cv::Point lt, cv::Point rb, const std::vector<std::uint8_t>& corners)
    : Block(lt, rb, static_cast<int>(corners.size()) / 4), corners_(corners) {}

int RBlock::interpolated(int column, int row, int c) const {
    const std::size_t base = static_cast<std::size_t>(c) * 4;
    const int top_left = corners_[base + 0];
    const int top_right = corners_[base + 1];
    const int bottom_left = corners_[base + 2];
    const int bottom_right = corners_[base + 3];

    const double row_ratio = ratio(lt_.y, rb_.y, row);
    const double column_ratio = ratio(lt_.x, rb_.x, column);

    // Interpolate down the left and the right edge first, then across.
    const int left = interpolate(top_left, bottom_left, row_ratio);
    const int right = interpolate(top_right, bottom_right, row_ratio);
    return interpolate(left, right, column_ratio);
}

bool RBlock::is_block(const cv::Mat& image, const bool* marked,
                       int rows, int cols, int sigma) const {
    for (int y = lt_.y; y <= rb_.y; ++y) {
        for (int x = lt_.x; x <= rb_.x; ++x) {
            if (marked[static_cast<std::size_t>(y) * cols + x]) {
                return false;
            }

            // The largest deviation across the channels decides whether the
            // block is acceptable. For a single channel image this is the
            // same rule as the greyscale implementation.
            int largest = 0;
            for (int c = 0; c < channels_; ++c) {
                const int expected = interpolated(x, y, c);
                const int actual = sample_at(image, y, x, c);
                largest = std::max(largest, std::abs(actual - expected));
            }
            if (largest > sigma) {
                return false;
            }
        }
    }
    return true;
}

void RBlock::restore(cv::Mat& image) const {
    for (int y = lt_.y; y <= rb_.y; ++y) {
        for (int x = lt_.x; x <= rb_.x; ++x) {
            for (int c = 0; c < channels_; ++c) {
                set_sample_at(image, y, x, c, interpolated(x, y, c));
            }
        }
    }
}

}  // namespace blockcodec
```

- [ ] **Step 7: Write the codec driver**

Create `src/core/codec.cpp`:

```cpp
#include "blockcodec/codec.hpp"

#include "blockcodec/bitstream.hpp"
#include "blockcodec/format.hpp"
#include "blockcodec/metrics.hpp"
#include "core/pixel.hpp"
#include "core/rblock.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace blockcodec {
namespace {

double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// The number of bits needed to represent every value in [0, maximum].
int bits_for(int maximum) {
    int bits = 1;
    while ((std::uint64_t{1} << bits) <= static_cast<std::uint64_t>(maximum)) {
        ++bits;
    }
    return bits;
}

// Reads the four corner samples of the block at `lt` .. `rb`.
//
// The samples are ordered top-left, top-right, bottom-left, bottom-right,
// and each pixel contributes one sample per channel.
std::vector<std::uint8_t> read_corners(const cv::Mat& image, cv::Point lt,
                                        cv::Point rb) {
    const int channels = image.channels();
    std::vector<std::uint8_t> corners;
    corners.reserve(static_cast<std::size_t>(channels) * 4);

    // The corners are ordered top-left, top-right, bottom-left, bottom-right.
    const int columns[4] = {lt.x, rb.x, lt.x, rb.x};
    const int rows[4] = {lt.y, lt.y, rb.y, rb.y};
    for (int i = 0; i < 4; ++i) {
        for (int c = 0; c < channels; ++c) {
            corners.push_back(static_cast<std::uint8_t>(sample_at(image, rows[i], columns[i], c)));
        }
    }
    return corners;
}

// Finds the bottom right corner of the block that starts at `lt`.
cv::Point find_rb(const cv::Mat& image, const bool* marked, int rows, int cols,
                   int channels, cv::Point lt, int sigma) {
    cv::Point rb = lt;

    // Extend along the diagonal first.
    while (rb.x + 1 < cols && rb.y + 1 < rows &&
           RBlock(lt, cv::Point(rb.x + 1, rb.y + 1), channels)
               .is_block(image, marked, rows, cols, sigma)) {
        ++rb.x;
        ++rb.y;
    }
    // Then horizontally.
    while (rb.x + 1 < cols &&
           RBlock(lt, cv::Point(rb.x + 1, rb.y), channels)
               .is_block(image, marked, rows, cols, sigma)) {
        ++rb.x;
    }
    // Then vertically.
    while (rb.y + 1 < rows &&
           RBlock(lt, cv::Point(rb.x, rb.y + 1), channels)
               .is_block(image, marked, rows, cols, sigma)) {
        ++rb.y;
    }
    return rb;
}

}  // namespace

Codec::Codec(CompressOptions options) : options_(options) {}

std::vector<std::uint8_t> Codec::compress(const cv::Mat& source,
                                           CompressResult* result) {
    const double started = now_seconds();

    if (source.empty()) {
        throw std::runtime_error("blockcodec: cannot compress an empty image");
    }
    if (source.depth() != CV_8U) {
        throw std::runtime_error("blockcodec: only 8-bit images are supported");
    }
    if (options_.sigma < 0) {
        throw std::runtime_error("blockcodec: the tolerance must not be negative");
    }

    const int rows = source.rows;
    const int cols = source.cols;
    const int channels = source.channels();
    const int sigma = options_.sigma;

    std::vector<bool> marked(static_cast<std::size_t>(rows) * cols, false);
    std::vector<RBlock> blocks;

    // Find the blocks. The scan order matches the legacy implementation:
    // rows from top to bottom, and within a row from left to right.
    for (int row = 0; row < rows; ++row) {
        int column = 0;
        while (column < cols) {
            if (marked[static_cast<std::size_t>(row) * cols + column]) {
                ++column;
                continue;
            }

            const cv::Point lt(column, row);
            const cv::Point rb =
                find_rb(source, marked.data(), rows, cols, channels, lt, sigma);

            RBlock block(lt, rb, channels);
            block.set_corners(read_corners(source, lt, rb));

            blocks.push_back(block);
            block.mark(marked.data(), rows, cols);
            column = rb.x + 1;
        }
    }

    // The field widths are derived from the header, so both sides compute
    // them the same way without depending on their own position.
    const int row_bits = bits_for(rows - 1);
    const int column_bits = bits_for(cols - 1);
    const int extent_bits = bits_for(std::max(rows, cols) - 1);

    BitWriter writer;
    for (const RBlock& block : blocks) {
        writer.write(static_cast<std::uint64_t>(block.lt().y), row_bits);
        writer.write(static_cast<std::uint64_t>(block.lt().x), column_bits);
        writer.write(static_cast<std::uint64_t>(block.rb().x - block.lt().x),
                     extent_bits);
        writer.write(static_cast<std::uint64_t>(block.rb().y - block.lt().y),
                     extent_bits);

        for (const std::uint8_t corner : block.corners()) {
            writer.write(corner, 8);
        }
    }

    std::vector<std::uint8_t> payload;
    writer.to_bytes(payload);

    ContainerHeader header;
    header.rows = static_cast<std::uint32_t>(rows);
    header.cols = static_cast<std::uint32_t>(cols);
    header.channels = static_cast<std::uint8_t>(channels);
    header.depth_bits = 8;
    header.colour = channels > 1;
    header.block_count = static_cast<std::uint32_t>(blocks.size());

    std::vector<std::uint8_t> container = write_container(header, payload);

    if (result != nullptr) {
        result->bit_size = writer.bit_count();
        result->byte_size = container.size();
        result->bpp = bpp(result->bit_size, rows, cols);
        result->cr = compression_ratio(result->bit_size, rows, cols, channels, 8);
        result->block_count = blocks.size();
        result->compress_ms = (now_seconds() - started) * 1000.0;
    }

    return container;
}

cv::Mat Codec::decompress(const std::vector<std::uint8_t>& container,
                           CompressResult* result) {
    const double started = now_seconds();

    std::vector<std::uint8_t> payload;
    const ContainerHeader header = read_container(container, payload);

    const int rows = static_cast<int>(header.rows);
    const int cols = static_cast<int>(header.cols);
    const int channels = static_cast<int>(header.channels);

    const int row_bits = bits_for(rows - 1);
    const int column_bits = bits_for(cols - 1);
    const int extent_bits = bits_for(std::max(rows, cols) - 1);

    BitReader reader(payload.data(), payload.size());

    cv::Mat image(rows, cols, CV_8UC(channels), cv::Scalar(0));

    for (std::uint32_t i = 0; i < header.block_count; ++i) {
        const int start_row = static_cast<int>(reader.read(row_bits));
        const int start_column = static_cast<int>(reader.read(column_bits));
        const int width = static_cast<int>(reader.read(extent_bits)) + 1;
        const int height = static_cast<int>(reader.read(extent_bits)) + 1;

        if (start_row >= rows || start_column >= cols ||
            start_row + height > rows || start_column + width > cols) {
            throw std::runtime_error("blockcodec: a block lies outside the image");
        }

        const cv::Point lt(start_column, start_row);
        const cv::Point rb(start_column + width - 1, start_row + height - 1);

        std::vector<std::uint8_t> corners(static_cast<std::size_t>(channels) * 4);
        for (std::uint8_t& corner : corners) {
            corner = static_cast<std::uint8_t>(reader.read(8));
        }

        RBlock block(lt, rb, corners);
        block.restore(image);
    }

    if (result != nullptr) {
        result->byte_size = container.size();
        result->block_count = header.block_count;
        result->decompress_ms = (now_seconds() - started) * 1000.0;
    }

    return image;
}

cv::Mat Codec::decompress(const std::string& path, CompressResult* result) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("blockcodec: cannot open '" + path + "' for reading");
    }
    const std::vector<std::uint8_t> container((std::istreambuf_iterator<char>(in)),
                                              std::istreambuf_iterator<char>());
    return decompress(container, result);
}

double psnr(const cv::Mat& a, const cv::Mat& b) {
    if (a.empty() || b.empty()) {
        throw std::runtime_error("blockcodec::psnr: an image is empty");
    }
    if (a.rows != b.rows || a.cols != b.cols || a.channels() != b.channels()) {
        throw std::runtime_error("blockcodec::psnr: the images have different shapes");
    }

    double sum = 0.0;
    for (int row = 0; row < a.rows; ++row) {
        for (int column = 0; column < a.cols; ++column) {
            for (int c = 0; c < a.channels(); ++c) {
                const double difference =
                    static_cast<double>(sample_at(a, row, column, c)) -
                    static_cast<double>(sample_at(b, row, column, c));
                sum += difference * difference;
            }
        }
    }

    if (sum == 0.0) {
        return std::numeric_limits<double>::infinity();
    }

    const double maximum = 255.0;
    const double pixels = static_cast<double>(a.rows) * a.cols * a.channels();
    return 10.0 * std::log10((maximum * maximum * pixels) / sum);
}

}  // namespace blockcodec
```

**Note on the metric helpers:** `codec.cpp` calls `bpp` and
`compression_ratio`, which live in `src/core/metrics.cpp` (Task 7). Task 5
therefore creates that file too, with the two functions in the `blockcodec`
namespace, so the library compiles before Task 7 adds the public
`metrics.hpp`. In Task 7 the public header is added and the definitions
move into `src/core/metrics.cpp` unchanged.

- [ ] **Step 8: Write the metric helpers**

`codec.cpp` calls `bpp` and `compression_ratio`. Create them now, so that the
library compiles. Task 7 adds the public header for them.

Create `include/blockcodec/metrics.hpp`:

```cpp
#pragma once

#include <cstddef>

namespace blockcodec {

// The bits per pixel of a stream of `bits` bits over an image of
// `rows` x `cols` pixels.
double bpp(std::size_t bits, int rows, int cols);

// The compression ratio: the raw size in bits divided by the encoded size.
//
// Returns infinity when the encoded size is zero.
double compression_ratio(std::size_t bits, int rows, int cols, int channels,
                         int depth_bits);

}  // namespace blockcodec
```

Create `src/core/metrics.cpp`:

```cpp
#include "blockcodec/metrics.hpp"

#include <limits>

namespace blockcodec {

double bpp(std::size_t bits, int rows, int cols) {
    if (rows <= 0 || cols <= 0) {
        return 0.0;
    }
    const double pixels = static_cast<double>(rows) * static_cast<double>(cols);
    return static_cast<double>(bits) / pixels;
}

double compression_ratio(std::size_t bits, int rows, int cols, int channels,
                         int depth_bits) {
    if (bits == 0) {
        return std::numeric_limits<double>::infinity();
    }
    if (rows <= 0 || cols <= 0 || channels <= 0 || depth_bits <= 0) {
        return 0.0;
    }
    const double raw_bits = static_cast<double>(rows) * cols * channels * depth_bits;
    return raw_bits / static_cast<double>(bits);
}

}  // namespace blockcodec
```

- [ ] **Step 9: Wire the library into CMake**

Replace the `opencv_codec` library block in `CMakeLists.txt` with:

```cmake
add_library(blockcodec STATIC
    src/core/bitstream.cpp
    src/core/block.cpp
    src/core/rblock.cpp
    src/core/codec.cpp
    src/core/metrics.cpp
    src/format.cpp
)

target_include_directories(blockcodec
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(blockcodec PUBLIC ${OpenCV_LIBS})
set_target_properties(blockcodec PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(blockcodec::blockcodec ALIAS blockcodec)
```

- [ ] **Step 10: Run the tests**

Run the test script. Expected: `test_codec` reports 8 passing tests, and the
existing `codec_selftest` still passes.

- [ ] **Step 11: Commit**

```bash
git add include/blockcodec src/core src/format.cpp tests/unit/test_codec.cpp CMakeLists.txt
git commit -m "feat: add the blockcodec core with colour support"
```

---

## Task 6: The golden regression test

Proves that the refactor did not change the single-channel behaviour. This is
the safety net for the whole rewrite.

**Files:**
- Create: `tests/legacy/Compress_gray.h`, `tests/legacy/Compress_gray.cpp`
- Create: `tests/legacy/Bits.h`, `tests/legacy/Bits.cpp`
- Create: `tests/legacy/Block.h`, `tests/legacy/Block.cpp`
- Create: `tests/legacy/RBlock.h`, `tests/legacy/RBlock.cpp`
- Create: `tests/unit/test_golden.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `blockcodec::Codec`, `blockcodec::psnr` (Task 5).
- Produces: a test-only target `legacy_codec` that is never installed.

- [ ] **Step 1: Move the legacy sources under `tests/legacy/`**

```bash
mkdir -p tests/legacy
git mv Compress_gray.h Compress_gray.cpp Bits.h Bits.cpp Block.h Block.cpp RBlock.h RBlock.cpp tests/legacy/
```

The legacy sources keep their original content, including the `windows.h`
include. They are only compiled on Windows, by a test-only target.

- [ ] **Step 2: Write the failing test**

Create `tests/unit/test_golden.cpp`:

```cpp
// Proves that the rewrite did not change the behaviour of the codec on
// single channel images.
//
// The legacy implementation is kept as an oracle. The two are compared on
// the block structure and on the reconstructed pixels, not on the encoded
// bytes: the new container is densely packed and transmits the gap width
// explicitly, so its bytes necessarily differ from the legacy stream.

#include "blockcodec/codec.hpp"

#include "Compress_gray.h"

#include <gtest/gtest.h>

#include <opencv2/imgcodecs.hpp>

#include <string>
#include <vector>

namespace {

std::vector<std::string> image_paths() {
    return {
        "images/100gray.bmp",
        "images/197gray.bmp",
        "images/f16.bmp",
        "images/lena512.bmp",
        "images/baboonzyp512.bmp",
    };
}

}  // namespace

TEST(Golden, MatchesTheLegacyReconstructionForGreyscaleImages) {
    for (const std::string& path : image_paths()) {
        const cv::Mat source = cv::imread(path, cv::IMREAD_GRAYSCALE);
        if (source.empty()) {
            GTEST_SKIP() << "missing " << path;
        }

        for (const int sigma : {1, 10, 30}) {
            // The legacy encoder.
            Compress_gray legacy;
            legacy.init_compress(source, sigma);
            legacy.compress();
            legacy.save("golden_legacy.nam");

            Compress_gray legacy_decoder;
            legacy_decoder.init_uncompress("golden_legacy.nam");
            const cv::Mat legacy_out = legacy_decoder.uncompress();

            // The new encoder.
            blockcodec::Codec codec({sigma});
            blockcodec::CompressResult result;
            const std::vector<std::uint8_t> container = codec.compress(source, &result);
            const cv::Mat new_out = codec.decompress(container, &result);

            ASSERT_EQ(new_out.rows, legacy_out.rows) << path << " sigma=" << sigma;
            ASSERT_EQ(new_out.cols, legacy_out.cols) << path << " sigma=" << sigma;

            // The reconstructed pixels must be identical.
            const cv::Mat difference;
            cv::absdiff(new_out, legacy_out, difference);
            double largest = 0.0;
            cv::minMaxLoc(difference.reshape(1), nullptr, &largest);
            EXPECT_EQ(largest, 0.0)
                << path << " sigma=" << sigma
                << ": the reconstruction differs from the legacy implementation";

            // The block count must match.
            EXPECT_EQ(result.block_count, legacy.blocks().size())
                << path << " sigma=" << sigma;
        }
    }
}
```

- [ ] **Step 3: Register the oracle and the test**

Add to `CMakeLists.txt`, before the `if(BUILD_TESTING)` block:

```cmake
# The legacy implementation, kept only as a test oracle. It uses windows.h
# and is therefore only built on Windows.
if(BUILD_TESTING AND WIN32)
    add_library(legacy_codec STATIC
        tests/legacy/Compress_gray.cpp
        tests/legacy/Bits.cpp
        tests/legacy/Block.cpp
        tests/legacy/RBlock.cpp
    )
    target_include_directories(legacy_codec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy)
    target_link_libraries(legacy_codec PUBLIC ${OpenCV_LIBS})
endif()
```

And inside `if(BUILD_TESTING)`:

```cmake
    if(TARGET legacy_codec)
        add_executable(test_golden tests/unit/test_golden.cpp)
        target_link_libraries(test_golden PRIVATE blockcodec legacy_codec gtest_main)
        target_compile_definitions(test_golden PRIVATE
            BLOCKCODEC_IMAGE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/images")
        add_test(NAME test_golden COMMAND test_golden)
    endif()
```

- [ ] **Step 4: Make the legacy block list accessible**

In `tests/legacy/Compress_gray.h`, the `allBlocks` member is private. Add a
public accessor to the legacy header:

```cpp
    // The blocks found by the last encode. Exposed so the golden test can
    // compare the block structure with the new implementation.
    const std::vector<RBlock>& blocks() const { return allBlocks; }
```

The test already calls `legacy.blocks().size()`, so it cannot compile until
this accessor exists.

- [ ] **Step 5: Run the test and confirm it passes**

Run the test script. Expected: `test_golden` passes for every image and sigma.

If it fails, the cause is a genuine behavioural difference between the legacy
and new implementations. Find it by comparing the first differing block, and
fix the new implementation rather than the oracle.

- [ ] **Step 6: Commit**

```bash
git add tests/legacy tests/unit/test_golden.cpp CMakeLists.txt
git commit -m "test: add a golden regression test against the legacy codec"
```

---

## Task 7: Metrics and robustness

Adds the metric unit tests, and the handling of malformed input.

**Files:**
- Create: `tests/unit/test_metrics.cpp`
- Create: `tests/unit/test_robustness.cpp`
- Create: `src/core/metrics.cpp`
- Create: `include/blockcodec/metrics.hpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `blockcodec::Codec`, `blockcodec::psnr` (Task 5).
- Produces:
  ```cpp
  namespace blockcodec {
  // The bits per pixel of a stream of `bits` bits over an image of
  // `rows` x `cols` pixels.
  double bpp(std::size_t bits, int rows, int cols);

  // The compression ratio: the raw size in bits divided by the encoded size.
  double compression_ratio(std::size_t bits, int rows, int cols, int channels,
                           int depth_bits);
  }
  ```

- [ ] **Step 1: Write the failing metric test**

Create `tests/unit/test_metrics.cpp`:

```cpp
#include "blockcodec/metrics.hpp"

#include <gtest/gtest.h>

using blockcodec::bpp;
using blockcodec::compression_ratio;

TEST(Metrics, BitsPerPixelIsBitsOverPixels) {
    EXPECT_DOUBLE_EQ(bpp(8000, 100, 100), 0.8);
    EXPECT_DOUBLE_EQ(bpp(0, 100, 100), 0.0);
}

TEST(Metrics, CompressionRatioIsRawOverEncoded) {
    // 100x100 pixels at 8 bits per channel: 80000 raw bits.
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 100, 100, 1, 8), 1.0);
    EXPECT_DOUBLE_EQ(compression_ratio(40000, 100, 100, 1, 8), 2.0);

    // A colour image has three times the raw bits.
    EXPECT_DOUBLE_EQ(compression_ratio(80000, 100, 100, 3, 8), 3.0);
}

TEST(Metrics, CompressionRatioIsInfiniteForAnEmptyStream) {
    EXPECT_TRUE(std::isinf(compression_ratio(0, 100, 100, 1, 8)));
}
```

- [ ] **Step 2: Write the failing robustness test**

Create `tests/unit/test_robustness.cpp`:

```cpp
#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"

#include <gtest/gtest.h>

#include <opencv2/imgcodecs.hpp>

#include <cstdint>
#include <random>
#include <vector>

using blockcodec::Codec;
using blockcodec::CompressResult;

TEST(Robustness, RejectsRandomBytes) {
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    Codec codec;
    for (int trial = 0; trial < 200; ++trial) {
        std::vector<std::uint8_t> garbage(64);
        for (std::uint8_t& byte : garbage) {
            byte = static_cast<std::uint8_t>(byte_dist(rng));
        }
        // The call must fail cleanly. It must not crash or read out of bounds.
        EXPECT_THROW(codec.decompress(garbage), std::runtime_error);
    }
}

TEST(Robustness, RejectsTruncatedContainers) {
    cv::Mat source(32, 32, CV_8UC1, cv::Scalar(120));
    Codec codec;
    const std::vector<std::uint8_t> container = codec.compress(source);

    for (std::size_t cut = 1; cut < container.size(); cut += 7) {
        const std::vector<std::uint8_t> truncated(container.begin(),
                                                  container.begin() +
                                                      static_cast<std::ptrdiff_t>(cut));
        EXPECT_THROW(codec.decompress(truncated), std::runtime_error)
            << "cut=" << cut;
    }
}

TEST(Robustness, RejectsACorruptedPayload) {
    cv::Mat source(32, 32, CV_8UC1, cv::Scalar(120));
    Codec codec;
    std::vector<std::uint8_t> container = codec.compress(source);

    // Corrupt the payload but keep the header intact.
    for (std::size_t i = 16; i < container.size(); i += 3) {
        container[i] = static_cast<std::uint8_t>(container[i] ^ 0xFF);
    }

    // A corrupted payload may decode into nonsense, but it must never crash
    // or read out of bounds.
    try {
        const cv::Mat decoded = codec.decompress(container);
        EXPECT_EQ(decoded.rows, 32);
        EXPECT_EQ(decoded.cols, 32);
    } catch (const std::runtime_error&) {
        SUCCEED() << "the corrupted payload was rejected";
    }
}
```

- [ ] **Step 3: Register the tests and confirm they fail**

Add to `CMakeLists.txt` inside `if(BUILD_TESTING)`:

```cmake
    add_executable(test_metrics tests/unit/test_metrics.cpp)
    target_link_libraries(test_metrics PRIVATE blockcodec gtest_main)
    add_test(NAME test_metrics COMMAND test_metrics)

    add_executable(test_robustness tests/unit/test_robustness.cpp)
    target_link_libraries(test_robustness PRIVATE blockcodec gtest_main)
    add_test(NAME test_robustness COMMAND test_robustness)
```

Run the build. Expected: failure, `blockcodec/metrics.hpp` does not exist.

- [ ] **Step 4: Confirm the metric helpers**

`include/blockcodec/metrics.hpp` and `src/core/metrics.cpp` were created in
Task 5, because the codec calls them. Confirm that both exist and that the
implementation reads:

```cpp
#pragma once

#include <cstddef>

namespace blockcodec {

// The bits per pixel of a stream of `bits` bits over an image of
// `rows` x `cols` pixels.
double bpp(std::size_t bits, int rows, int cols);

// The compression ratio: the raw size in bits divided by the encoded size.
//
// Returns infinity when the encoded size is zero.
double compression_ratio(std::size_t bits, int rows, int cols, int channels,
                         int depth_bits);

}  // namespace blockcodec
```

The implementation in `src/core/metrics.cpp` must be:

```cpp
#include "blockcodec/metrics.hpp"

#include <limits>

namespace blockcodec {

double bpp(std::size_t bits, int rows, int cols) {
    if (rows <= 0 || cols <= 0) {
        return 0.0;
    }
    const double pixels = static_cast<double>(rows) * static_cast<double>(cols);
    return static_cast<double>(bits) / pixels;
}

double compression_ratio(std::size_t bits, int rows, int cols, int channels,
                         int depth_bits) {
    if (bits == 0) {
        return std::numeric_limits<double>::infinity();
    }
    if (rows <= 0 || cols <= 0 || channels <= 0 || depth_bits <= 0) {
        return 0.0;
    }
    const double raw_bits = static_cast<double>(rows) * cols * channels * depth_bits;
    return raw_bits / static_cast<double>(bits);
}

}  // namespace blockcodec
```

- [ ] **Step 5: Run the tests**

Run the test script. Expected: `test_metrics` reports 3 passing tests, and
`test_robustness` reports 3 passing tests.

- [ ] **Step 6: Commit**

```bash
git add include/blockcodec/metrics.hpp src/core/metrics.cpp tests/unit/test_metrics.cpp tests/unit/test_robustness.cpp CMakeLists.txt
git commit -m "feat: add explicit metric helpers and robustness tests"
```

---

## Task 8: The command line interface

**Files:**
- Create: `apps/cli/main.cpp`
- Create: `apps/cli/commands.h`
- Create: `apps/cli/commands.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `blockcodec::Codec`, `blockcodec::CompressOptions`,
  `blockcodec::CompressResult` (Task 5), `blockcodec::read_container` (Task 4),
  `blockcodec::psnr` (Task 5).
- Produces: the executable `blockcodec` with the subcommands
  `compress`, `decompress`, `info` and `benchmark`.

- [ ] **Step 1: Add CLI11**

Add to `CMakeLists.txt`, before `add_library(blockcodec ...)`:

```cmake
include(FetchContent)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(CLI11)
```

- [ ] **Step 2: Write the command declarations**

Create `apps/cli/commands.h`:

```cpp
#pragma once

#include <string>

namespace blockcodec {
namespace cli {

// blockcodec compress <input> -o <output> [--sigma N] [--quiet]
int run_compress(const std::string& input, const std::string& output, int sigma,
                 bool quiet);

// blockcodec decompress <input> -o <output> [--compare <reference>]
int run_decompress(const std::string& input, const std::string& output,
                   const std::string& compare);

// blockcodec info <input>
int run_info(const std::string& input);

}  // namespace cli
}  // namespace blockcodec
```

- [ ] **Step 3: Write the command implementations**

Create `apps/cli/commands.cpp`:

```cpp
#include "cli/commands.h"

#include "blockcodec/codec.hpp"
#include "blockcodec/format.hpp"
#include "blockcodec/metrics.hpp"

#include <opencv2/imgcodecs.hpp>

#include <cstdio>
#include <exception>
#include <fstream>
#include <string>
#include <vector>

namespace blockcodec {
namespace cli {
namespace {

// Writes `data` to `path`. Throws std::runtime_error on failure.
void write_file(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("cannot open '" + path + "' for writing");
    }
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    if (!out) {
        throw std::runtime_error("failed to write '" + path + "'");
    }
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open '" + path + "' for reading");
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

void report(const CompressResult& result, const std::string& path) {
    std::printf("wrote %s\n", path.c_str());
    std::printf("  bits        %zu\n", result.bit_size);
    std::printf("  bytes       %zu\n", result.byte_size);
    std::printf("  bpp         %.6f\n", result.bpp);
    std::printf("  cr          %.6f\n", result.cr);
    std::printf("  psnr        %.4f dB\n", result.psnr);
    std::printf("  blocks      %zu\n", result.block_count);
    std::printf("  encode      %.2f ms\n", result.compress_ms);
}

}  // namespace

int run_compress(const std::string& input, const std::string& output, int sigma,
                 bool quiet) {
    const cv::Mat source = cv::imread(input, cv::IMREAD_UNCHANGED);
    if (source.empty()) {
        throw std::runtime_error("cannot read the image '" + input + "'");
    }

    Codec codec({sigma});
    CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(source, &result);

    // The PSNR needs the reconstruction, so decode what was just written.
    const cv::Mat decoded = codec.decompress(container, nullptr);
    result.psnr = psnr(source, decoded);

    write_file(output, container);

    if (!quiet) {
        report(result, output);
    }
    return 0;
}

int run_decompress(const std::string& input, const std::string& output,
                   const std::string& compare) {
    const std::vector<std::uint8_t> container = read_file(input);

    Codec codec;
    CompressResult result;
    const cv::Mat decoded = codec.decompress(container, &result);

    if (!cv::imwrite(output, decoded)) {
        throw std::runtime_error("cannot write the image '" + output + "'");
    }

    std::printf("wrote %s (%dx%d, %d channels)\n", output.c_str(), decoded.cols,
                decoded.rows, decoded.channels());

    if (!compare.empty()) {
        const cv::Mat reference = cv::imread(compare, cv::IMREAD_UNCHANGED);
        if (reference.empty()) {
            throw std::runtime_error("cannot read the image '" + compare + "'");
        }
        std::printf("  psnr        %.4f dB\n", psnr(reference, decoded));
    }
    return 0;
}

int run_info(const std::string& input) {
    const std::vector<std::uint8_t> container = read_file(input);

    std::vector<std::uint8_t> payload;
    const ContainerHeader header = read_container(container, payload);

    std::printf("file          %s\n", input.c_str());
    std::printf("  version     %u\n", static_cast<unsigned>(kFormatVersion));
    std::printf("  dimensions  %u x %u\n", header.rows, header.cols);
    std::printf("  channels    %u\n", static_cast<unsigned>(header.channels));
    std::printf("  depth       %u bits\n", static_cast<unsigned>(header.depth_bits));
    std::printf("  payload     %zu bytes\n", payload.size());
    return 0;
}

}  // namespace cli
}  // namespace blockcodec
```

- [ ] **Step 4: Write the CLI entry point**

Create `apps/cli/main.cpp`:

```cpp
#include "cli/commands.h"

#include <CLI/CLI.hpp>

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    CLI::App app{"blockcodec, a block based image compression codec"};
    app.require_subcommand(1);

    int sigma = 10;
    bool quiet = false;
    std::string compare;

    auto* compress = app.add_subcommand("compress", "Compress an image");
    compress->add_option("input", "The image to compress")->required();
    compress->add_option("-o,--output", "The container to write")->required();
    compress->add_option("-s,--sigma", "The tolerance of the block test", sigma);
    compress->add_flag("-q,--quiet", "Print nothing", quiet);

    auto* decompress = app.add_subcommand("decompress", "Decompress a container");
    decompress->add_option("input", "The container to read")->required();
    decompress->add_option("-o,--output", "The image to write")->required();
    decompress->add_option("--compare", "Compare against this image", compare);

    auto* info = app.add_subcommand("info", "Print the header of a container");
    info->add_option("input", "The container to read")->required();

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    try {
        if (*app.get_subcommands().begin() == "compress") {
            return blockcodec::cli::run_compress(compress->get<std::string>("input"),
                                                 compress->get<std::string>("--output"),
                                                 sigma, quiet);
        }
        if (*app.get_subcommands().begin() == "decompress") {
            return blockcodec::cli::run_decompress(
                decompress->get<std::string>("input"),
                decompress->get<std::string>("--output"), compare);
        }
        return blockcodec::cli::run_info(info->get<std::string>("input"));
    } catch (const std::exception& e) {
        std::cerr << "blockcodec: " << e.what() << "\n";
        return 1;
    }
}
```

- [ ] **Step 5: Register the CLI target**

Add to `CMakeLists.txt`:

```cmake
add_executable(blockcodec_cli apps/cli/main.cpp apps/cli/commands.cpp)
set_target_properties(blockcodec_cli PROPERTIES OUTPUT_NAME blockcodec)
target_link_libraries(blockcodec_cli PRIVATE blockcodec CLI11::CLI11)
```

- [ ] **Step 6: Build and try the CLI**

Run the build script. Then:

```
build/Release/blockcodec.exe compress images/lena512.bmp -o lena.bcc --sigma 10
build/Release/blockcodec.exe info lena.bcc
build/Release/blockcodec.exe decompress lena.bcc -o lena_out.png --compare images/lena512.bmp
```

Expected: the compress command prints the metrics, `info` prints the header
dimensions of 512 x 512 with 1 channel, and the decompress command prints a
PSNR above 30 dB.

- [ ] **Step 7: Commit**

```bash
git add apps/cli CMakeLists.txt
git commit -m "feat: add the blockcodec command line interface"
```

---

## Task 9: The benchmark with a JPEG baseline

**Files:**
- Create: `bench/bench_main.cpp`
- Create: `bench/jpeg_baseline.h`
- Create: `bench/jpeg_baseline.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `blockcodec::Codec`, `blockcodec::CompressOptions` (Task 5),
  `blockcodec::psnr` (Task 5).
- Produces: the executable `blockcodec_bench`, which prints a table and
  optionally writes CSV.

- [ ] **Step 1: Write the JPEG baseline**

Create `bench/jpeg_baseline.h`:

```cpp
#pragma once

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace blockcodec {
namespace bench {

// One measurement of the JPEG encoder at a given quality.
struct JpegMeasurement {
    int quality = 0;
    std::size_t byte_size = 0;
    double psnr = 0.0;
};

// Encodes `image` with the JPEG encoder at each of the given qualities and
// measures the resulting size and quality.
std::vector<JpegMeasurement> measure_jpeg(const cv::Mat& image,
                                           const std::vector<int>& qualities);

}  // namespace bench
}  // namespace blockcodec
```

Create `bench/jpeg_baseline.cpp`:

```cpp
#include "bench/jpeg_baseline.h"

#include "blockcodec/codec.hpp"

#include <opencv2/imgcodecs.hpp>

#include <vector>

namespace blockcodec {
namespace bench {

std::vector<JpegMeasurement> measure_jpeg(const cv::Mat& image,
                                           const std::vector<int>& qualities) {
    std::vector<JpegMeasurement> measurements;
    measurements.reserve(qualities.size());

    for (const int quality : qualities) {
        std::vector<std::uint8_t> buffer;
        const std::vector<int> params{ cv::IMWRITE_JPEG_QUALITY, quality };

        if (!cv::imencode(".jpg", image, buffer, params)) {
            continue;
        }

        const cv::Mat decoded = cv::imdecode(buffer, cv::IMREAD_UNCHANGED);
        if (decoded.empty()) {
            continue;
        }

        JpegMeasurement measurement;
        measurement.quality = quality;
        measurement.byte_size = buffer.size();
        measurement.psnr = psnr(image, decoded);
        measurements.push_back(measurement);
    }

    return measurements;
}

}  // namespace bench
}  // namespace blockcodec
```

- [ ] **Step 2: Write the benchmark**

Create `bench/bench_main.cpp`:

```cpp
// Sweeps the tolerance over a set of images and reports the size and quality
// trade off.
//
// With --baseline jpeg the same images are also encoded by the JPEG encoder,
// so the numbers can be read against a reference that everybody knows.

#include "bench/jpeg_baseline.h"

#include "blockcodec/codec.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Row {
    std::string image;
    int rows = 0;
    int cols = 0;
    int channels = 1;
    int sigma = 0;
    std::size_t bytes = 0;
    double bpp = 0.0;
    double cr = 0.0;
    double psnr = 0.0;
    double encode_ms = 0.0;
    double decode_ms = 0.0;
    std::size_t blocks = 0;
};

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, separator)) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

void print_header() {
    std::printf("%-24s %5s %5s %5s %6s %10s %8s %8s %9s %8s %8s\n", "image", "rows",
                "cols", "ch", "sigma", "bytes", "bpp", "cr", "psnr", "enc_ms",
                "dec_ms");
}

void print_row(const Row& row) {
    std::printf("%-24s %5d %5d %5d %6d %10zu %8.4f %8.4f %9.4f %8.2f %8.2f\n",
                row.image.c_str(), row.rows, row.cols, row.channels, row.sigma,
                row.bytes, row.bpp, row.cr, row.psnr, row.encode_ms,
                row.decode_ms);
}

}  // namespace

int main(int argc, char** argv) {
    std::string image_dir = "images";
    std::string csv_path;
    std::vector<int> sigmas{1, 5, 10, 20, 30, 50};
    bool jpeg_baseline = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s needs a value\n", name);
                std::exit(2);
            }
            return argv[++i];
        };

        if (argument == "--images") {
            image_dir = next("--images");
        } else if (argument == "--csv") {
            csv_path = next("--csv");
        } else if (argument == "--sigmas") {
            sigmas.clear();
            for (const std::string& part : split(next("--sigmas"), ',')) {
                sigmas.push_back(std::atoi(part.c_str()));
            }
        } else if (argument == "--baseline") {
            const std::string value = next("--baseline");
            jpeg_baseline = (value == "jpeg");
        } else {
            std::fprintf(stderr, "unknown option %s\n", argument.c_str());
            return 2;
        }
    }

    std::vector<std::filesystem::path> images;
    for (const auto& entry : std::filesystem::directory_iterator(image_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension = entry.path().extension().string();
        if (extension == ".bmp" || extension == ".png" || extension == ".jpg") {
            images.push_back(entry.path());
        }
    }
    std::sort(images.begin(), images.end());

    if (images.empty()) {
        std::fprintf(stderr, "no images found in '%s'\n", image_dir.c_str());
        return 1;
    }

    std::ofstream csv;
    if (!csv_path.empty()) {
        csv.open(csv_path);
        csv << "image,rows,cols,channels,sigma,bytes,bpp,cr,psnr,encode_ms,"
               "decode_ms,blocks\n";
    }

    print_header();

    for (const auto& path : images) {
        const cv::Mat source = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
        if (source.empty()) {
            continue;
        }

        for (const int sigma : sigmas) {
            blockcodec::Codec codec({sigma});
            blockcodec::CompressResult result;
            const std::vector<std::uint8_t> container = codec.compress(source, &result);
            const cv::Mat decoded = codec.decompress(container, &result);
            result.psnr = blockcodec::psnr(source, decoded);

            Row row;
            row.image = path.filename().string();
            row.rows = source.rows;
            row.cols = source.cols;
            row.channels = source.channels();
            row.sigma = sigma;
            row.bytes = result.byte_size;
            row.bpp = result.bpp;
            row.cr = result.cr;
            row.psnr = result.psnr;
            row.encode_ms = result.compress_ms;
            row.decode_ms = result.decompress_ms;
            row.blocks = result.block_count;

            print_row(row);

            if (csv.is_open()) {
                csv << row.image << ',' << row.rows << ',' << row.cols << ','
                     << row.channels << ',' << row.sigma << ',' << row.bytes << ','
                     << row.bpp << ',' << row.cr << ',' << row.psnr << ','
                     << row.encode_ms << ',' << row.decode_ms << ',' << row.blocks
                     << '\n';
            }
        }
    }

    if (jpeg_baseline) {
        std::printf("\nJPEG baseline for comparison\n");
        std::printf("%-24s %6s %10s %9s\n", "image", "quality", "bytes", "psnr");
        for (const auto& path : images) {
            const cv::Mat source = cv::imread(path.string(), cv::IMREAD_UNCHANGED);
            if (source.empty()) {
                continue;
            }
            for (const auto& measurement :
                 blockcodec::bench::measure_jpeg(source, {95, 90, 85, 80, 75})) {
                std::printf("%-24s %6d %10zu %9.4f\n",
                            path.filename().string().c_str(), measurement.quality,
                            measurement.byte_size, measurement.psnr);
            }
        }
    }

    return 0;
}
```

- [ ] **Step 3: Register the benchmark**

Add to `CMakeLists.txt`:

```cmake
add_executable(blockcodec_bench bench/bench_main.cpp bench/jpeg_baseline.cpp)
target_link_libraries(blockcodec_bench PRIVATE blockcodec)
```

- [ ] **Step 4: Build and run the benchmark**

Run the build script, then:

```
build/Release/blockcodec_bench.exe --images images --sigmas 10,30 --baseline jpeg
```

Expected: a table of measurements and a JPEG comparison section.

- [ ] **Step 5: Commit**

```bash
git add bench CMakeLists.txt
git commit -m "feat: add a benchmark with a JPEG baseline"
```

---

## Task 10: Migrate the Qt GUI

**Files:**
- Create: `apps/qt/` (moved from the repository root)
- Modify: `apps/qt/compresswindow.cpp`, `apps/qt/uncompresswindow.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `blockcodec::Codec`, `blockcodec::CompressOptions`,
  `blockcodec::CompressResult` (Task 5).
- Produces: the executable `blockcodec_gui`.

- [ ] **Step 1: Move the GUI sources**

```bash
mkdir -p apps/qt
git mv compresswindow.cpp compresswindow.h compresswindow.ui \
      uncompresswindow.cpp uncompresswindow.h uncompresswindow.ui \
      mainwindow.cpp mainwindow.h mainwindow.ui \
      helpwindow.cpp helpwindow.h helpwindow.ui \
      introducewindow.cpp introducewindow.h introducewindow.ui \
      save.cpp save.h save.ui \
      operatorsignals.cpp operatorsignals.h \
      main.cpp resourses.qrc apps/qt/
```

- [ ] **Step 2: Replace the legacy codec calls in the GUI**

In `apps/qt/compresswindow.cpp`, replace every use of `Compress_gray` with the
new API. The pattern to replace is:

```cpp
    Compress_gray* cg = new Compress_gray();
    cg->init_compress(img, sigma);
    cg->compress();
    cg->save(path);
```

with:

```cpp
    blockcodec::Codec codec({sigma});
    blockcodec::CompressResult result;
    const std::vector<std::uint8_t> container = codec.compress(img, &result);
    // Write the container to `path`.
```

In `apps/qt/uncompresswindow.cpp`, replace:

```cpp
    Compress_gray* ucg = new Compress_gray();
    ucg->init_uncompress(path);
    cv::Mat decoded = ucg->uncompress();
```

with:

```cpp
    blockcodec::Codec codec;
    blockcodec::CompressResult result;
    const cv::Mat decoded = codec.decompress(path, &result);
```

- [ ] **Step 3: Label the metrics with their units**

In `apps/qt/compresswindow.cpp`, the labels must name their unit. Replace the
plain "rate" label with "bits/pixel" and the size label with "bytes", so the
distinction between the bitstream length and the file size is visible in the
interface.

- [ ] **Step 4: Register the GUI target**

Add to `CMakeLists.txt`:

```cmake
option(BLOCKCODEC_BUILD_GUI "Build the Qt GUI example" ON)

if(BLOCKCODEC_BUILD_GUI)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

    set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/apps/qt)

    add_executable(blockcodec_gui WIN32 MACOSX_BUNDLE
        apps/qt/main.cpp
        apps/qt/mainwindow.cpp
        apps/qt/mainwindow.h
        apps/qt/mainwindow.ui
        apps/qt/compresswindow.cpp
        apps/qt/compresswindow.h
        apps/qt/compresswindow.ui
        apps/qt/uncompresswindow.cpp
        apps/qt/uncompresswindow.h
        apps/qt/uncompresswindow.ui
        apps/qt/introducewindow.cpp
        apps/qt/introducewindow.h
        apps/qt/introducewindow.ui
        apps/qt/helpwindow.cpp
        apps/qt/helpwindow.h
        apps/qt/helpwindow.ui
        apps/qt/save.cpp
        apps/qt/save.h
        apps/qt/save.ui
        apps/qt/operatorsignals.cpp
        apps/qt/operatorsignals.h
        apps/qt/resourses.qrc
    )

    target_link_libraries(blockcodec_gui PRIVATE
        blockcodec
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
    )
endif()
```

- [ ] **Step 5: Build the GUI**

Run the build script with `-DBLOCKCODEC_BUILD_GUI=ON`. Expected: exit code 0
and `blockcodec_gui.exe` is produced.

- [ ] **Step 6: Commit**

```bash
git add apps/qt CMakeLists.txt
git commit -m "feat: migrate the Qt GUI onto the blockcodec API"
```

---

## Task 11: Packaging, installation and documentation

**Files:**
- Modify: `CMakeLists.txt`
- Create: `docs/ALGORITHM_BCC.md`
- Modify: `README.md`
- Modify: `README_CN.md`
- Modify: `ALGORITHM.md`

**Interfaces:**
- Consumes: everything built by the previous tasks.
- Produces: an installable package, and the documentation.

- [ ] **Step 1: Add the install rules**

Append to `CMakeLists.txt`:

```cmake
include(GNUInstallDirs)

install(TARGETS blockcodec
        EXPORT blockcodecTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/blockcodec
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT blockcodecTargets
        FILE blockcodecTargets.cmake
        NAMESPACE blockcodec::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blockcodec
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/blockcodecConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blockcodec
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blockcodec
)
```

- [ ] **Step 2: Write `cmake/blockcodecConfig.cmake.in`**

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(OpenCV)

include("${CMAKE_CURRENT_LIST_DIR}/blockcodecTargets.cmake")

check_required_components(blockcodec)
```

- [ ] **Step 3: Verify the install works**

Run:

```
cmake --install build --config Release --prefix /tmp/blockcodec-install
```

Then check that `include/blockcodec/blockcodec.hpp` and
`lib/cmake/blockcodec/blockcodecConfig.cmake` exist under the prefix.

- [ ] **Step 4: Write `docs/ALGORITHM_BCC.md`**

Document the new container: the magic, the version, the header layout, the
payload grammar (the row flag, the marker, the explicitly transmitted gap
width, the corner samples), the block search order, the interpolation model,
and the metric definitions. State that the legacy `.nam` format is
documented separately in `ALGORITHM.md` and is not read by this library.

- [ ] **Step 5: Rewrite `README.md`**

The README must cover: what the library is, the algorithm in two paragraphs,
the build instructions for both the library and the applications, the CLI
usage, the measured benchmark table including the JPEG comparison, the
container format summary with a pointer to `docs/ALGORITHM_BCC.md`, and the
licence. Every number in the table must come from an actual run of
`blockcodec_bench`, not from memory.

- [ ] **Step 6: Update `README_CN.md`**

Translate the new README. Keep it in step with the English version.

- [ ] **Step 7: Update `ALGORITHM.md`**

Add a note at the top stating that this document specifies the **legacy**
`.nam` container, that the library writes and reads the `.bcc` container
described in `docs/ALGORITHM_BCC.md`, and that the legacy format is retained
for reference and for the golden regression test.

- [ ] **Step 8: Commit**

```bash
git add CMakeLists.txt cmake/ include/ docs/ README.md README_CN.md ALGORITHM.md
git commit -m "docs: describe the .bcc container and add installation rules"
```

---

## Task 12: Continuous integration

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: the CMake targets built by the earlier tasks.
- Produces: the `ci` workflow.

- [ ] **Step 1: Write `.github/workflows/ci.yml`**

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  workflow_dispatch:

jobs:
  build-test:
    name: ${{ matrix.name }}
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - name: Ubuntu (GCC)
            os: ubuntu-22.04
          - name: Ubuntu (Clang)
            os: ubuntu-22.04
            cc: clang
          - name: Windows (MSVC)
            os: windows-2022

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies (Ubuntu)
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ninja-build libopencv-dev

      - name: Configure
        run: >
          cmake -S . -B build -G Ninja
          -DCMAKE_BUILD_TYPE=Release
          -DBUILD_GUI_APP=OFF
          -DBUILD_TESTING=ON

      - name: Build
        run: cmake --build build --parallel

      - name: Test
        run: ctest --test-dir build --output-on-failure

  sanitizers:
    name: Sanitizers (ASan, UBSan)
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ninja-build libopencv-dev

      - name: Configure
        run: >
          cmake -S . -B build -G Ninja
          -DCMAKE_BUILD_TYPE=Debug
          -DBUILD_GUI_APP=OFF
          -DBUILD_TESTING=ON
          -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
          -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

      - name: Build
        run: cmake --build build --parallel

      - name: Test
        run: ctest --test-dir build --output-on-failure

  gui:
    name: Qt GUI
    runs-on: ubuntu-22.04
    continue-on-error: true
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            build-essential cmake ninja-build libopencv-dev \
            qtbase5-dev qtbase5-dev-tools qttools5-dev

      - name: Configure
        run: >
          cmake -S . -B build -G Ninja
          -DCMAKE_BUILD_TYPE=Release
          -DBUILD_GUI_APP=ON

      - name: Build
        run: cmake --build build --parallel
```

**Note:** the option names in this file must match the ones the top-level
`CMakeLists.txt` actually uses. The existing file uses `BUILD_GUI_APP` and
`BUILD_TESTING`; Task 10 introduces `BLOCKCODEC_BUILD_GUI`. Use
`BLOCKCODEC_BUILD_GUI` everywhere and update the workflow accordingly, and
make sure the top-level `CMakeLists.txt` accepts both spellings during the
transition:

```cmake
option(BLOCKCODEC_BUILD_GUI "Build the Qt GUI example" ON)
option(BUILD_GUI_APP "Deprecated alias for BLOCKCODEC_BUILD_GUI" OFF)
if(BUILD_GUI_APP)
    set(BLOCKCODEC_BUILD_GUI ON)
endif()
```

- [ ] **Step 2: Verify the workflow locally**

Run the same configure and build commands the workflow uses, but with the
generator available on this machine. Expected: the build succeeds and the
tests pass.

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml CMakeLists.txt
git commit -m "ci: build and test on Linux, Windows and under sanitizers"
```

---

## Task 13: Final verification and release

**Files:**
- Modify: `CHANGELOG.md` (create)
- Modify: `README.md`

**Interfaces:**
- Consumes: everything.
- Produces: a releasable repository.

- [ ] **Step 1: Run the full test suite**

Run: `cmd.exe //c "F:\\Projects\\01_courses\\cpp\\QtProject\\openCV_test\\tools\\test.bat"`
Expected: every test passes.

- [ ] **Step 2: Run the benchmark and record the numbers**

```
build/Release/blockcodec_bench.exe --images images --sigmas 1,5,10,20,30,50 --baseline jpeg --csv bench.csv
```

Copy the resulting table into the README, replacing the placeholder table.
Every number must come from this run.

- [ ] **Step 3: Verify the CLI end to end**

```
build/Release/blockcodec.exe compress images/lenaColorful.bmp -o colour.bcc --sigma 10
build/Release/blockcodec.exe info colour.bcc
build/Release/blockcodec.exe decompress colour.bcc -o colour_out.png --compare images/lenaColorful.bmp
```

Expected: `info` reports 512 x 512 with 3 channels, and the PSNR is above
30 dB. This is the colour path the legacy code could not handle.

- [ ] **Step 4: Write `CHANGELOG.md`**

```markdown
# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- A `blockcodec` library with a value-semantic API.
- Colour image support. The legacy implementation flattened the channels into
  the image width, which read past the end of each row and decoded a colour
  image as a mis-sized greyscale image.
- The `.bcc` container, which stores the channel count explicitly.
- A command line interface with `compress`, `decompress` and `info`.
- A benchmark with a JPEG baseline for comparison.
- GoogleTest coverage, including a golden regression test against the legacy
  implementation.
- Continuous integration on Linux, Windows and under the sanitizers.

### Changed

- The bitstream is packed eight bits to a byte. The legacy container stored
  one ASCII character per bit, which made every file eight times larger than
  the bitstream it carried.
- The metrics are reported with explicit names and units. `bpp` is bits per
  pixel, `byte_size` is the size of the container.
- The library is portable. The `windows.h` dependency is replaced by
  `std::chrono`.

### Fixed

- The colour path no longer reads out of bounds.
- A landscape greyscale image is no longer misreported as colour.
- The bit writer no longer shifts past the width of an integer.
- The decoder rejects malformed containers instead of trusting them.
```

- [ ] **Step 5: Check that no Chinese comments remain in the source**

Run:

```bash
grep -rn -P '[\x{4e00}-\x{9fff}]' src/ include/ apps/ bench/ tests/ --include=*.cpp --include=*.h --include=*.hpp
```

Expected: no output. Every comment and every message in the library, the
applications and the tests is in English.

- [ ] **Step 6: Commit**

```bash
git add CHANGELOG.md README.md
git commit -m "docs: record the release notes and the measured benchmark table"
```

---

## Self-Review

**Spec coverage:**

| Spec section | Tasks |
|--------------|-------|
| 2 Goals | 5, 8, 9, 12, 13 |
| 3 Naming | 5, 8 |
| 4 Repository layout | 1, 2, 5, 8, 9, 10 |
| 5 Core API | 5 |
| 6 Colour support | 5, 6 |
| 6.3 Metrics | 7 |
| 7 File format | 4 |
| 8 Defects | 3, 4, 5, 7 |
| 9.1 CLI | 8 |
| 9.2 Benchmark | 9 |
| 9.3 Qt GUI | 10 |
| 10 Tests | 3, 4, 5, 6, 7 |
| 11 CI | 12 |
| 12 Packaging and docs | 11, 13 |
| 13 Git history | 2 |
| 14 Delivery order | the task order |
| 15 Risks | 6, 7 |
| 16 Environment | 1 |

**Type consistency:** `CompressOptions::sigma` is an `int` everywhere.
`CompressResult` has `bit_size`, `byte_size`, `bpp`, `cr`, `psnr`,
`compress_ms`, `decompress_ms` and `block_count`, used identically in the
codec, the CLI, the benchmark and the tests. `Codec::compress` and
`Codec::decompress` both take an optional trailing `CompressResult*`.

**Placeholders:** none. Every task contains the complete code of the files it
creates, and every step names the exact command to run and the expected
result.

**One known simplification:** Task 5 writes the codec in a single
`src/core/codec.cpp`. If the file grows past roughly 500 lines during
implementation, split the encoder and the decoder into
`src/core/encoder.cpp` and `src/core/decoder.cpp` behind the same
`blockcodec::Codec` interface. Do not restructure it before that point.

**Correctness details that the golden test depends on.** These were checked
against the legacy implementation and must be preserved exactly:

- The interpolation truncates to an `unsigned char` at every step, not only
  at the end. The legacy helper returned a `uchar`, so each intermediate
  value was rounded down before it was used again.
- The interpolation runs vertically first (interpolating down the left and
  the right edge) and then horizontally.
- The corner samples are ordered top-left, top-right, bottom-left,
  bottom-right.
- The block scan visits rows from top to bottom and, within a row, columns
  from left to right. The block extent grows along the diagonal first, then
  horizontally, then vertically.

**A detail that is easy to get wrong.** `Mat::at<cv::Vec3b>` is only valid
on a three channel matrix; on a greyscale matrix it reads past the element.
The plan therefore reads and writes samples through `src/core/pixel.hpp`,
which uses the row pointer and works for any channel count.

**A note on the landscape greyscale test.** The assertion targets the
channel count carried by the container, not the decoded matrix. The legacy
decoder always allocated a single channel result, so `decoded.channels() == 1`
holds even when its colour inference is wrong, and the assertion would prove
nothing.
