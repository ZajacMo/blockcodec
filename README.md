# blockcodec

*[中文](README_CN.md)*

`blockcodec` is a small, self-contained C++17 library for a **lossy,
block-based, interpolating image codec**. It encodes an image as a set of
rectangular blocks, each described by its corner samples and reconstructed by
bilinear interpolation. The container it writes, the `.bcc` format, is
specified exactly enough that an independent implementation in another
language can produce byte-identical output.

The library depends on OpenCV for the image containers and nothing else. It
has no dependency on Qt, and no dependency on the GUI that ships with it.

**What this is not.** `blockcodec` is *not* a general purpose image codec.
It is not a competitor to JPEG, WebP or AVIF, and it does not try to be one.
On natural photographs it produces a **larger file than JPEG at the same
reconstruction quality** — the measured numbers below show it losing on every
512×512 and 645×1274 sample, usually by a wide margin. Its value is that it is
a *well specified, reproducible, testable* block codec: an exact format
specification, a reference vector, a golden regression test against the
original implementation, and a benchmark that measures it against a baseline
rather than against itself.

---

## Contents

- [The algorithm](#the-algorithm)
- [Benchmarks](#benchmarks)
- [Building](#building)
- [Installing](#installing)
- [Using the library](#using-the-library)
- [Command line](#command-line)
- [Colour support](#colour-support)
- [The container format](#the-container-format)
- [Tests](#tests)
- [Repository layout](#repository-layout)
- [Limitations](#limitations)
- [License](#license)

---

## The algorithm

The encoder scans the image from top to bottom and, within a row, from left to
right. At each uncovered pixel it grows a rectangular block: first along the
diagonal, then along the columns, then along the rows, stopping as soon as a
candidate fails the block test. A candidate is accepted when **every** pixel
inside it differs from the bilinear interpolation of its four corner samples by
no more than `sigma`. The four corner samples are what get written to the
file; the interior is reconstructed by interpolation on the way back. Pixels
that no block can cover are stored as isolated single-pixel blocks.

`sigma` is therefore both the quality dial and the size dial at once. A small
`sigma` accepts only near-uniform blocks, which produces large files at a high
PSNR. A large `sigma` accepts coarse blocks, which produces small files at a
lower PSNR. For a colour image the comparison uses the **largest** deviation
across the channels, so a single channel image reduces to the greyscale rule
exactly.

The exact bit-level layout, the interpolation arithmetic and every check a
decoder must perform are specified in
**[docs/ALGORITHM_BCC.md](docs/ALGORITHM_BCC.md)**.

---

## Benchmarks

The numbers below come from a single run of `blockcodec_bench` over the
bundled `images/` directory:

```
blockcodec_bench --images images --sigmas 1,5,10,20,30,50 --baseline jpeg
```

### The comparison that means something

A table with two independent lists side by side tells the reader nothing: a
JPEG file and a blockcodec file only compare when they hold *the same
reconstruction*. So the benchmark matches the JPEG quality to the blockcodec
quality first. For each `sigma` it finds the **lowest JPEG quality that still
reaches the PSNR the blockcodec file reached**, and reports the size of the
JPEG file at that quality. That is where the two curves cross.

`bc_bytes` is the size of the `.bcc` file on disk, `jpeg_bytes` the size of
the encoded JPEG. `smaller` names the codec with the smaller file at that
matched quality.

**`lena512.bmp` — 512×512, greyscale**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | smaller |
|--------:|---------:|--------:|-------:|-----------:|----------:|---------|
| 1 | 525 380 | 59.23 dB | 100 | 162 310 | 58.47 dB | *no winner* |
| 5 | 269 912 | 42.29 dB | 94 | 81 621 | 42.93 dB | jpeg |
| 10 | 126 194 | 36.39 dB | 60 | 24 107 | 36.45 dB | jpeg |
| 20 | 57 803 | 30.38 dB | 10 | 7 998 | 30.41 dB | jpeg |
| 30 | 35 474 | 26.69 dB | 5 | 5 661 | 27.33 dB | jpeg |
| 50 | 16 196 | 22.36 dB | 1 | 4 376 | 24.22 dB | jpeg |

**`baboonzyp512.bmp` — 512×512, high-frequency texture**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | smaller |
|--------:|---------:|--------:|-------:|-----------:|----------:|---------|
| 1 | 563 128 | 67.26 dB | 100 | 246 705 | 58.40 dB | *no winner* |
| 5 | 507 249 | 46.81 dB | 98 | 201 524 | 49.76 dB | jpeg |
| 10 | 408 411 | 38.48 dB | 92 | 124 485 | 38.72 dB | jpeg |
| 20 | 270 822 | 31.26 dB | 75 | 68 913 | 31.34 dB | jpeg |
| 30 | 191 211 | 27.30 dB | 39 | 39 108 | 27.31 dB | jpeg |
| 50 | 104 307 | 22.56 dB | 8 | 12 508 | 22.83 dB | jpeg |

**`decho256.bmp` — 256×256, greyscale**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | smaller |
|--------:|---------:|--------:|-------:|-----------:|----------:|---------|
| 1 | 79 364 | 54.76 dB | 99 | 31 251 | 55.36 dB | jpeg |
| 5 | 43 676 | 43.06 dB | 89 | 12 755 | 43.19 dB | jpeg |
| 10 | 29 788 | 37.12 dB | 59 | 6 645 | 37.13 dB | jpeg |
| 20 | 18 052 | 31.44 dB | 16 | 3 268 | 31.45 dB | jpeg |
| 30 | 12 228 | 28.54 dB | 9 | 2 416 | 28.93 dB | jpeg |
| 50 | 6 820 | 24.58 dB | 4 | 1 632 | 25.10 dB | jpeg |

**`lenaColorful.bmp` — 512×512, three channels**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | smaller |
|--------:|---------:|--------:|-------:|-----------:|----------:|---------|
| 1 | 1 076 975 | 76.85 dB | 100 | 229 542 | 37.49 dB | *no winner* |
| 5 | 923 162 | 46.39 dB | 100 | 229 542 | 37.49 dB | *no winner* |
| 10 | 517 081 | 38.29 dB | 100 | 229 542 | 37.49 dB | *no winner* |
| 20 | 184 177 | 32.27 dB | 57 | 26 822 | 32.29 dB | jpeg |
| 30 | 102 287 | 28.61 dB | 14 | 11 389 | 28.76 dB | jpeg |
| 50 | 46 484 | 24.13 dB | 5 | 7 055 | 24.29 dB | jpeg |

**`PrintScreen.png` — 645×1274, four channels**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | smaller |
|--------:|---------:|--------:|-------:|-----------:|----------:|---------|
| 1 | 2 029 003 | 58.09 dB | 100 | 523 861 | 53.03 dB | *no winner* |
| 5 | 1 241 516 | 43.72 dB | 95 | 319 351 | 44.22 dB | jpeg |
| 10 | 762 974 | 38.00 dB | 85 | 187 368 | 38.05 dB | jpeg |
| 20 | 499 308 | 32.32 dB | 53 | 104 395 | 32.39 dB | jpeg |
| 30 | 389 394 | 28.46 dB | 22 | 64 965 | 28.62 dB | jpeg |
| 50 | 282 816 | 23.35 dB | 5 | 31 052 | 23.40 dB | jpeg |

**`100.bmp` — 16×16, greyscale**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | smaller |
|--------:|---------:|--------:|-------:|-----------:|----------:|---------|
| 1 | 404 | 65.22 dB | 100 | 547 | 59.20 dB | *no winner* |
| 5 | 332 | 44.91 dB | 97 | 499 | 46.71 dB | **blockcodec** |
| 10 | 266 | 37.12 dB | 87 | 414 | 37.24 dB | **blockcodec** |
| 20 | 194 | 30.02 dB | 23 | 358 | 30.16 dB | **blockcodec** |
| 30 | 104 | 25.37 dB | 10 | 345 | 25.43 dB | **blockcodec** |
| 50 | 56 | 21.40 dB | 4 | 338 | 22.44 dB | **blockcodec** |

### What the table says

- **On every natural image, at every matched quality, JPEG is smaller.** The
  gap is large — across the natural samples above, the blockcodec file is
  between 2.5× and 9.1× the size of the JPEG file at the same quality, and
  usually 3× to 7×. A block that costs four corner samples to describe is
  expensive, and JPEG's transform, quantisation and entropy coding are simply
  a better way to spend the same bits on a photograph.
- **On the 16×16 samples, blockcodec wins.** The reason is not that it
  compresses better; it is that its container has a fixed 20-byte header,
  while a JPEG of the same image costs 338–547 bytes almost entirely in
  headers and tables. At `sigma = 50` blockcodec writes 56 bytes against
  JPEG's 338. This is a header-size effect, not a compression effect, and it
  does not survive the smallest images that people actually store.
- **The `*no winner*` rows are not ties.** They are rows where even the
  highest JPEG quality does not reach the blockcodec PSNR, so the two files
  are not at the same quality and their sizes say nothing about the trade
  off. The benchmark prints these as `short` and names no winner for them.
  The clearest case is `lenaColorful.bmp` at `sigma = 1`: blockcodec reaches
  76.85 dB and JPEG at quality 100 only reaches 37.49 dB, because a
  three-channel bilinear block model can reproduce a flat colour field almost
  exactly where a JPEG's 8×8 DCT blocks cannot.
- **The useful `sigma` depends on the image.** The smooth `lena512` reaches
  36.4 dB in 126 kB, while the textured `baboonzyp512` needs 408 kB for
  38.5 dB, because a textured image offers few large uniform rectangles.

### Reproducing the table

```bash
export PATH="/path/to/opencv/build/x64/vc16/bin:$PATH"   # Windows
./build/Release/blockcodec_bench.exe \
    --images images --sigmas 1,5,10,20,30,50 --baseline jpeg
```

Add `--csv bench.csv` to write the measurements to three CSV files.

---

## Building

### Requirements

| Dependency | Version | Notes |
|------------|---------|-------|
| C++ compiler | C++17 | MSVC 2019+, GCC 8+, Clang 8+ |
| CMake | 3.16 or newer | |
| OpenCV | 4.x | Tested against 4.10.0. `core` and `imgproc` are enough for the library; `imgcodecs` is needed by the CLI and the benchmark |
| Qt | 5.x or 6.x | **Optional.** Only the GUI example needs it |

GoogleTest and CLI11 are fetched by CMake at configure time, so they do not
have to be installed.

### The library alone

```bash
cmake -S . -B build -DBLOCKCODEC_BUILD_GUI=OFF -DBUILD_TESTING=OFF
cmake --build build --config Release
```

### Everything, including the GUI example

```bash
cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="/path/to/Qt/6.6.2/msvc2019_64" \
      -DOpenCV_DIR="/path/to/opencv/build"
cmake --build build --config Release
```

On Windows with Visual Studio, configure with the generator and the
architecture explicitly:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_PREFIX_PATH="C:/Qt/6.6.2/msvc2019_64" \
      -DOpenCV_DIR="C:/Program Files/opencv/build"
cmake --build build --config Release
```

OpenCV and Qt are dynamically linked, so their `bin` directories have to be on
`PATH` to run the resulting executables:

```
C:\Program Files\opencv\build\x64\vc16\bin
C:\Qt\6.6.2\msvc2019_64\bin
```

### Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `BLOCKCODEC_BUILD_GUI` | `ON` | Build the Qt GUI example (`blockcodec_gui`) |
| `BUILD_TESTING` | `ON` | Build the unit tests and register them with CTest |

---

## Installing

```bash
cmake --install build --config Release --prefix /usr/local
```

That installs:

```
<prefix>/include/blockcodec/*.hpp          the public headers
<prefix>/lib/blockcodec.lib                 the static library
<prefix>/lib/cmake/blockcodec/blockcodecConfig.cmake
<prefix>/lib/cmake/blockcodec/blockcodecConfigVersion.cmake
<prefix>/lib/cmake/blockcodec/blockcodecTargets.cmake
```

### Using the installed package

```cmake
find_package(blockcodec REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE blockcodec::blockcodec)
```

`find_package` pulls in OpenCV itself, because the public headers of
`blockcodec` include `<opencv2/core.hpp>`. If OpenCV is not on the default
search path, point `CMAKE_PREFIX_PATH` at it:

```bash
cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="/usr/local;/path/to/opencv/build"
```

### Using the source tree directly

If you would rather not install anything:

```cmake
add_subdirectory(path/to/blockcodec)
target_link_libraries(my_app PRIVATE blockcodec::blockcodec)
```

---

## Using the library

The public API is small and stateless. Every call is independent, and the
measurements are returned rather than held.

```cpp
#include <blockcodec/blockcodec.hpp>

#include <opencv2/imgcodecs.hpp>
#include <fstream>

// Encode.
cv::Mat source = cv::imread("input.png", cv::IMREAD_UNCHANGED);

blockcodec::Codec codec({/*sigma=*/10});
blockcodec::CompressResult result;
std::vector<std::uint8_t> container = codec.compress(source, &result);

std::printf("%zu bytes, %.4f bpp, %.2f dB\n",
            result.byte_size, result.bpp, result.psnr);

std::ofstream out("output.bcc", std::ios::binary);
out.write(reinterpret_cast<const char*>(container.data()),
          static_cast<std::streamsize>(container.size()));

// Decode.
blockcodec::Codec decoder;
cv::Mat reconstructed = decoder.decompress("output.bcc");
```

### The API

| Symbol | Purpose |
|--------|---------|
| `blockcodec::Codec` | The encoder and decoder. Constructed with a `CompressOptions`. |
| `Codec::compress(mat, result)` | Encodes an image, returns the `.bcc` container. Throws `std::runtime_error` on an empty or non-8-bit image. |
| `Codec::decompress(container, result)` | Decodes a container, returns the image. Throws `std::runtime_error` on malformed input. |
| `Codec::decompress(path, result)` | The same, reading the container from a file. |
| `blockcodec::CompressOptions` | Holds `sigma`, the tolerance of the block test. |
| `blockcodec::CompressResult` | The measurements: `bit_size`, `byte_size`, `bpp`, `cr`, `psnr`, `compress_ms`, `decompress_ms`, `block_count`, `blocks`. |
| `blockcodec::psnr(a, b)` | The peak signal-to-noise ratio in dB. Infinity for an identical pair. |
| `blockcodec::bpp(bits, rows, cols)` | The bits per pixel of a bitstream. |
| `blockcodec::compression_ratio(...)` | The raw size in bits divided by the encoded size. |
| `blockcodec::read_container(data, payload)` | Parses a container header and splits off the payload. Throws on a malformed container. |
| `blockcodec::write_container(header, payload)` | Builds a container from a header and a payload. |
| `blockcodec::ContainerHeader` | The header fields: `rows`, `cols`, `block_count`, `channels`, `depth_bits`, `colour`. |
| `blockcodec::BitWriter` / `BitReader` | The dense bit stream. Most significant bit first, eight bits to a byte. |
| `blockcodec::segmentation_overlay(image, blocks)` | Draws the block boundaries on a copy of an image. |

Two notes on the metrics, because they are easy to get wrong:

- `CompressResult::bpp` and `CompressResult::cr` are measured on the **bit
  stream**, not on the container file. The container adds the 20-byte header
  and the final byte's padding. The benchmark reports the file size
  separately, because that is the number a user cares about.
- `CompressResult::psnr` is measured against the source, so the encoder
  decodes what it just wrote in order to compute it. That verification decode
  is included in `compress_ms`.

---

## Command line

The `blockcodec` executable has three subcommands.

```
blockcodec, a block based image compression codec
Usage: blockcodec.exe [OPTIONS] SUBCOMMAND

Options:
  -h,--help                   Print this help message and exit

Subcommands:
  compress                    Compress an image
  decompress                  Decompress a container
  info                        Print the header of a container
```

The exit code is 0 when the command succeeded, 1 when it failed at run time
(a file that cannot be read, a container that is malformed) and 2 when the
command line itself was wrong. `--help` exits with 0, because asking for the
help is a successful completion.

The `info` command prints the dimensions as **width by height**, the order
the image formats use.

### `compress`

```
$ blockcodec compress images/lena512.bmp -o lena.bcc -s 10
wrote lena.bcc
  bits        1009392
  bytes       126194
  bpp         3.850525
  cr          2.077639
  psnr        36.3887 dB
  blocks      14844
  encode      44.81 ms
```

`-s` is the tolerance, and it defaults to `10`. `-q` suppresses the report.

### `info`

```
$ blockcodec info lena.bcc
file          lena.bcc
  version     1
  dimensions  512 x 512
  channels    1
  depth       8 bits
  blocks      14844
  payload     126174 bytes
```

### `decompress`

```
$ blockcodec decompress lena.bcc -o lena.png --compare images/lena512.bmp
wrote lena.png (512x512, 1 channels)
  blocks      14844
  decode      3.78 ms
  psnr        36.3887 dB
```

`--compare` is optional; it reports the PSNR against a reference image and
fails if the shapes do not match.

---

## Colour support

**The legacy version of this project claimed colour support and did not
deliver it.** The old format inferred the channel count from the width of the
image, so every image wider than it is tall was misreported, and the GUI's
colour option produced a result that was not meaningful.

The current library does support colour properly:

- The channel count is stored **explicitly** in the container header, so the
  image is decoded with the right number of channels.
- Greyscale (1 channel), the common colour layout (3 channels) and images
  with an alpha channel (4 channels) all round trip.
- The block test compares the **largest** deviation across the channels, so
  one channel reduces to the greyscale rule exactly and the codec cannot
  accept a block that is good in one channel and bad in another.

The case that catches the old inference is an image that is **wider than it
is tall** and has one channel. `images/landscape_grey.bmp` is 256 pixels wide
and 128 pixels tall, and the golden regression test runs it against the
legacy implementation, so the two are proved to agree on it.

```
$ blockcodec compress images/landscape_grey.bmp -o landscape.bcc -s 10
wrote landscape.bcc
  bits        85428
  bytes       10699
  bpp         2.607056
  cr          3.068596
  psnr        36.8419 dB
  blocks      1356
  encode      4.24 ms

$ blockcodec info landscape.bcc
file          landscape.bcc
  version     1
  dimensions  256 x 128
  channels    1
  depth       8 bits
  blocks      1356
  payload     10679 bytes
```

```
$ blockcodec compress images/lenaColorful.bmp -o color.bcc -s 20
wrote color.bcc
  bits        1473252
  bytes       184177
  bpp         5.620010
  cr          4.270455
  psnr        32.2666 dB
  blocks      11161
  encode      104.27 ms

$ blockcodec info color.bcc
file          color.bcc
  version     1
  dimensions  512 x 512
  channels    3
  depth       8 bits
  blocks      11161
  payload     184157 bytes
```

A 4-channel image round trips the same way:

```
$ blockcodec info ps.bcc
file          ps.bcc
  version     1
  dimensions  1274 x 645
  channels    4
  depth       8 bits
  blocks      28736
  payload     614232 bytes
```

Only 8-bit images are supported. The header has a `depth_bits` field, but
16 and 32 bit samples are reserved and rejected.

---

## The container format

A `.bcc` container is a fixed 20-byte header followed by a packed payload:

```
offset  size  field
0       4     magic        "BCC\0"
4       1     version      0x01
5       1     flags        bit 0: colour (multi-channel)
6       4     rows         uint32, little-endian
10      4     cols         uint32, little-endian
14      1     channels     uint8
15      1     depth_bits   uint8
16      4     block_count  uint32, little-endian
20      n     payload      the packed bitstream
```

Each block in the payload is written as its top-left position, its width minus
one, its height minus one and the corner samples — four per channel, in the
order top-left, top-right, bottom-left, bottom-right. The field widths are
derived from the header rather than from the writer's own position, so the
payload is self-delimiting and a decoder never reads the padding.

The full specification, including the interpolation arithmetic, the byte and
bit order, and the list of conditions a decoder must reject, is in
**[docs/ALGORITHM_BCC.md](docs/ALGORITHM_BCC.md)**.

The project used a `.nam` format before this one. It is specified separately
in **[ALGORITHM.md](ALGORITHM.md)**, which describes the **legacy** format.
**This library does not read the legacy format**; a `.nam` file is rejected
with an error rather than guessed at. The legacy implementation is kept in
the repository as the oracle of the golden regression test.

---

## Tests

```bash
ctest --test-dir build --output-on-failure        # Linux / macOS
ctest --test-dir build -C Release --output-on-failure   # Windows
```

| Test | What it covers |
|------|----------------|
| `codec_selftest` | The legacy self test: round trip, reference vector, error handling |
| `test_bitstream` | The dense bit writer and reader |
| `test_format` | The container header: writing, parsing, and every rejection rule |
| `test_vector` | The reference vectors decode to exactly the reference images |
| `test_cli` | Drives the `blockcodec` executable |
| `test_codec` | The codec: round trip, colour, the block model |
| `test_metrics` | `psnr`, `bpp` and the compression ratio |
| `test_bench` | Drives `blockcodec_bench` |
| `test_robustness` | Malformed input is rejected rather than trusted |
| `test_golden` | The new codec against the legacy implementation, as an oracle |

The reference vectors in `tests/vectors/` are the acceptance criterion for an
independent implementation: decode `gradient_16x16.bcc` and the result must
be **pixel for pixel** identical to `gradient_16x16_reference.bmp`. The same
holds for the colour pair, `gradient_16x16_colour.bcc`. Metrics are not
acceptance criteria — a verification tool and the implementation it checks
could compute them the same way and agree while both are wrong.

---

## Repository layout

```
.
├── CMakeLists.txt          the build
├── cmake/
│   └── blockcodecConfig.cmake.in   the installed package description
├── include/blockcodec/      the public headers
│   ├── blockcodec.hpp       the umbrella header
│   ├── bitstream.hpp        the dense bit stream
│   ├── codec.hpp            the encoder and the decoder
│   ├── format.hpp           the .bcc container
│   ├── metrics.hpp          psnr, bpp, the compression ratio
│   └── segment.hpp          the segmentation overlay
├── src/
│   ├── core/                the bit stream, the block model, the codec
│   ├── format.cpp           the container
│   └── segment.cpp          the overlay
├── apps/
│   ├── cli/                 the blockcodec executable
│   ├── qt/                  the GUI example (blockcodec_gui)
│   └── prototype/           the original prototype, kept for reference
├── bench/                   blockcodec_bench and its JPEG baseline
├── tests/
│   ├── unit/ cli/ bench/    the test suites
│   ├── vector/              the reference vector generator
│   ├── vectors/             the versioned reference vector
│   └── legacy/              the legacy implementation, kept as an oracle
├── legacy/vs-console/       the previous Visual Studio implementation
├── docs/
│   └── ALGORITHM_BCC.md     the .bcc format specification
├── ALGORITHM.md             the legacy .nam format specification
├── images/                  the sample images
└── CHANGELOG.md             the change history
```

---

## Limitations

- **It loses to JPEG on natural images.** This is the main one, and it is
  measured rather than assumed. See [the benchmarks](#benchmarks).
- **Only 8-bit images.** The header reserves the fields for 16 and 32 bits
  per sample, but the decoder rejects them.
- **The encoder is not fast.** It is a straightforward search, not an optimal
  partition. A 512×512 image takes tens of milliseconds; a 645×1274 screenshot
  takes over a second. It is not parallelised.
- **The block search is greedy and order-dependent.** The scan order decides
  the shape of the blocks, and therefore the size of the file. A different
  order would give a different, equally valid, stream.
- **The container has no checksum.** A corrupted file is caught by the header
  checks and by the padding check, but a payload that is corrupted in a way
  that still decodes is not detected.
- **The quality of the codec is not certified by the metrics.** As the format
  specification says, the bits per pixel and the compression ratio are
  measurements, not acceptance criteria.

---

## License

Released under the [MIT License](LICENSE).
