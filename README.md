# OpenCV Image Processing and Compression Tool

*[中文](README_CN.md)*

A Windows desktop application for compressing and decompressing images,
built with **Qt** and **OpenCV**. It implements a custom lossy block codec
and exposes the compression algorithm both as a Qt GUI and as a
**standalone static library** that has no Qt dependency, so the codec can be
reused in other projects.

The compressed format (`.nam`) and the algorithm itself are specified in
detail in **[ALGORITHM.md](ALGORITHM.md)**, together with a reference test
vector that an independent implementation can use to verify itself.

---

## Contents

- [Features](#features)
- [Project layout](#project-layout)
- [Building](#building)
- [Running](#running)
- [Using the application](#using-the-application)
- [The compression algorithm](#the-compression-algorithm)
- [Using the codec as a library](#using-the-codec-as-a-library)
- [Tests and reference vectors](#tests-and-reference-vectors)
- [Repository layout and assets](#repository-layout-and-assets)
- [Known limitations](#known-limitations)
- [License](#license)

---

## Features

- **Main navigation window** with entries to every functional area
- **Compression** of greyscale images, with an adjustable error tolerance
- **Decompression** of previously written `.nam` files
- **Live quality readout** — the compression ratio, the elapsed time and the
  PSNR between the original and the reconstruction are shown after every run
- **Introduction and Help windows** documenting the project and its usage
- **Reusable codec library** — `opencv_codec` is a Qt-free static library
- **Cross-platform** — the codec builds on Linux and macOS as well as Windows

---

## Project layout

```
.
├── CMakeLists.txt        # CMake build: GUI app + codec library + tests
├── openCV_test.pro       # Qt Creator build (kept for Qt Creator users)
├── ALGORITHM.md          # Format specification and algorithm reference
│
├── Compress_gray.h/.cpp  # Core codec: compress, decompress, PSNR
├── Bits.h/.cpp           # Bitstream sink and source
├── Block.h/.cpp          # Base class for an image block
├── RBlock.h/.cpp         # Reconstructable block, block validity model
│
├── mainwindow.*          # Main navigation window
├── compresswindow.*      # Compression window
├── uncompresswindow.*    # Decompression window
├── introducewindow.*     # Project introduction window
├── helpwindow.*          # Help window
├── save.*                # Save dialog scaffold
├── operatorsignals.*     # QDataStream operators for std::bitset
│
├── tests/
│   ├── codec_selftest.cpp   # Round-trip and reference-vector checks
│   ├── codec_vector_gen.cpp # Regenerates the reference vector
│   ├── bench_sweep.cpp      # Sweeps sigma and prints ratio/PSNR
│   └── vectors/             # Versioned reference vector
│
├── images/               # Sample images used by the tests
├── Resources/            # Application icon
└── resourses.qrc         # Qt resource collection
```

---

## Building

### Requirements

| Dependency | Version | Notes |
|------------|---------|-------|
| Qt | 5.x or 6.x | Only the `Core`, `Gui` and `Widgets` modules are needed |
| OpenCV | 4.x | Tested against 4.10.0 |
| CMake | 3.16 or newer | Only needed for the CMake build |
| Compiler | C++17 | MSVC 2019+, GCC 8+, Clang 8+ |

### Option A — CMake (recommended)

CMake builds three things:

- `opencv_codec` — the static codec library, with **no Qt dependency**
- `openCV_test` — the GUI application
- `codec_selftest` — the test binary

```bash
cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="/path/to/Qt/6.6.2/msvc2019_64" \
      -DOpenCV_DIR="/path/to/opencv/build"
cmake --build build --config Release
```

On Windows with Visual Studio, configure with the generator and architecture
explicitly:

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_PREFIX_PATH="C:/Qt/6.6.2/msvc2019_64" \
      -DOpenCV_DIR="C:/Program Files/opencv/build"
cmake --build build --config Release
```

The OpenCV and Qt `bin` directories must be on `PATH` to run the resulting
executables, because both are dynamically linked:

```
C:\Program Files\opencv\build\x64\vc16\bin
C:\Qt\6.6.2\msvc2019_64\bin
```

### Option B — Qt Creator (`.pro` file)

The original `openCV_test.pro` is kept so the project still opens directly in
Qt Creator:

1. Open Qt Creator and choose **File → Open File or Project**.
2. Select `openCV_test.pro`.
3. Select the kit matching your Qt installation and compiler, for example
   *Desktop Qt 6.6.2 MSVC2019 64-bit*.
4. The `.pro` file points at `C:/Program Files/opencv/build`. If your OpenCV
   lives elsewhere, adjust `INCLUDEPATH`, `DEPENDPATH` and `LIBS` at the
   bottom of the file, or run `qmake` with the right flags:

   ```bash
   qmake openCV_test.pro "INCLUDEPATH+=C:/path/to/opencv/build/include" ^
                         "LIBS+=-L:C:/path/to/opencv/build/x64/vc16/lib -lopencv_world4100"
   ```

Both build systems produce the same binaries from the same sources.

---

## Running

```bash
./build/Release/openCV_test.exe        # Windows
./build/Release/openCV_test            # Linux / macOS
```

---

## Using the application

### Compressing an image

1. Click **Compress** on the main window.
2. Click **Choose** and select an image (`*.png`, `*.jpg`, `*.bmp`).
3. Choose greyscale or colour mode.
4. Adjust the **error tolerance** (`sigma`) with the spin box. Lower values
   give a better image but a larger file; see the table below.
5. Click **Run**. The window reports the original size, the compression
   ratio, the time taken and the compressed size.
6. Click **Decompress** to preview the reconstruction and its PSNR.
7. Click **Save** to write the compressed stream to a `.nam` file.

### Decompressing a file

1. Click **Decompress** on the main window.
2. Click **Choose** and select a `.nam` file.
3. Click **Run** to decode it, then **Save** to write the result as an image.

### Introduction and Help

The main window also opens an **Introduction** window describing the project
and a **Help** window with usage guidance.

---

## The compression algorithm

The codec is a **lossy, block-based codec for 8-bit greyscale images**.

The encoder scans the image and partitions it into rectangular blocks. A
block is described by the colours at its four corners, and its interior is
reconstructed by bilinear interpolation. Pixels that no block can cover
become isolated pixels and are stored verbatim.

The single parameter `sigma` is the maximum tolerated deviation, in grey
levels, between a pixel and the block that covers it. It is therefore both
the quality dial and the size dial at once.

Measured on the bundled 512x512 samples, with the numbers produced by
`bench_sweep`:

| `sigma` | lena512 bits | lena512 file vs raw | lena512 PSNR | baboon bits | baboon file vs raw | baboon PSNR |
|---------|--------------|---------------------|--------------|-------------|--------------------|-------------|
| 1 | 3 282 134 | 12.52x | 59.23 dB | 3 512 520 | 13.40x | 67.26 dB |
| 5 | 1 699 187 | 6.48x | 42.29 dB | 3 161 627 | 12.06x | 46.81 dB |
| 10 | 799 631 | 3.05x | 36.39 dB | 2 543 844 | 9.70x | 38.48 dB |
| 20 | 368 150 | 1.40x | 30.38 dB | 1 689 296 | 6.44x | 31.26 dB |
| 30 | 226 626 | 0.86x | 26.69 dB | 1 194 116 | 4.56x | 27.30 dB |
| 50 | 103 842 | 0.40x | 22.36 dB | 652 517 | 2.49x | 22.56 dB |

"file vs raw" is the size of the `.nam` file divided by the size of the raw
pixel data, so a value **below 1.0 means the file is smaller than the
original**. The PSNR is measured between the original and the
reconstruction.

Three results are worth calling out:

- **The text container costs a factor of eight.** The `.nam` format stores one
  ASCII character per bit, so a 3 282 134-bit stream occupies 3 282 135
  bytes on disk. The *bitstream* is what the codec produces; the *file* is
  eight times larger because of that encoding.
- **A small `sigma` makes the file much bigger.** At `sigma = 1` the file is
  12.5x the size of the original pixels, because every block costs far more
  to describe than the pixels it replaces. Only from about `sigma = 30`
  does the format become smaller than the raw image, and only at a
  substantial quality cost.
- **The useful `sigma` depends on the image.** The smooth `lena512`
  compresses far better than the textured `baboonzyp512` at the same
  tolerance, because a textured image offers few large uniform rectangles.

> **Note on `getRate()`.** The `getRate()` method inherited from the
> original code returns `bytes per pixel`, not a compression ratio, and it
> reports the bitstream length rather than the size of the file on disk.
> The table above uses the on-disk size, which is the number that matters to
> a user. See [ALGORITHM.md §9.8](ALGORITHM.md#98-getrate-reports-bytes-per-pixel-not-a-compression-ratio).

The exact bitstream layout is specified in **[ALGORITHM.md](ALGORITHM.md)**.

---

## Using the codec as a library

The CMake target `opencv_codec` contains only `Compress_gray`, `Bits`,
`Block` and `RBlock`. It depends on OpenCV but **not on Qt**, so it can be
dropped into a non-Qt project.

```cmake
add_subdirectory(path/to/OpenCV-Image-Processing-and-Compression-Tool-Qt)
target_link_libraries(my_app PRIVATE opencv_codec)
```

Compressing and decompressing from your own code:

```cpp
#include "Compress_gray.h"

// Compress
Compress_gray encoder;
encoder.init_compress(image, /*sigma=*/10);   // throws std::runtime_error
encoder.compress();                            // on an empty image
encoder.save("out.nam");
double ratio = encoder.getRate();             // compressed size / raw size

// Decompress
Compress_gray decoder;
decoder.init_uncompress("out.nam");
cv::Mat reconstructed = decoder.uncompress();
double psnr = Compress_gray::getPSNR(image, reconstructed);
```

The public API is small:

| Member | Purpose |
|--------|---------|
| `init_compress(mat, sigma)` | Prepare the encoder |
| `compress()` | Run the encoder |
| `init_uncompress(path)` | Prepare the decoder |
| `uncompress()` | Run the decoder, returns the reconstructed image |
| `save(path)` | Write the compressed stream |
| `getRate()` | Compressed size divided by raw size |
| `getTime()` | Seconds taken by the last operation |
| `getPSNR(a, b)` | Peak signal-to-noise ratio in dB |

---

## Tests and reference vectors

```bash
./build/Release/codec_selftest.exe
```

The self-test performs three kinds of check:

1. **Round trip** — compresses and decompresses the bundled sample images and
   asserts the reconstruction keeps its dimensions, reaches a PSNR floor and
   actually got smaller.
2. **Reference vector** — decodes `tests/vectors/gradient_16x16.nam` and
   asserts the result is *pixel-identical* to
   `gradient_16x16_reference.bmp`. This is the check an independent
   implementation of the format should aim to pass; see
   [ALGORITHM.md §8](ALGORITHM.md#8-verifying-a-port).
3. **Error handling** — asserts that a missing file and an empty source
   image are reported as errors rather than crashing.

To regenerate the reference vector (it is versioned, so this is only needed
when the codec changes):

```bash
./build/Release/codec_vector_gen.exe tests/vectors
```

To reproduce the ratio/PSNR table above:

```bash
./build/Release/bench_sweep.exe images/lena512.bmp
```

---

## Repository layout and assets

The `images/` directory holds the sample images used by the tests and the
benchmark:

| File | Description |
|------|-------------|
| `lena512.bmp`, `lenazyp512.bmp` | The classic Lena test image, greyscale |
| `lenaColorful.bmp` | The same image in colour |
| `baboonzyp512.bmp`, `barbarazyp512.bmp` | High-frequency texture, the hard case |
| `100gray.bmp`, `197gray.bmp`, `f16.bmp` | Small 16x16 samples |
| `100.bmp`, `100stc.bmp`, `197.bmp`, `197stc.bmp` | Small colour and structured samples |
| `broke256.bmp`, `decho256.bmp`, `flightzyp512.bmp` | Additional test inputs |
| `PrintScreen.png` | A screenshot used as a test input |

`Resources/head.ico` is the application icon, referenced through
`resourses.qrc`.

---

## Known limitations

- **Greyscale only.** The block model assumes a spatial neighbourhood, so it
  is not meaningful across colour channels. The GUI exposes a colour option,
  but colour images are treated as interleaved samples and the result is not
  meaningful. See [ALGORITHM.md §9.1](ALGORITHM.md#91-colour-images-are-not-really-supported).
- **The compressed format is text.** A `.nam` file stores one ASCII `'0'` or
  `'1'` character per bit, so it is about eight times larger than a packed
  binary stream would be. The format is specified this way deliberately, to
  keep it easy to inspect and to port.
- **No version marker or checksum.** A truncated file decodes to a partial
  image without an error. See
  [ALGORITHM.md §9.3](ALGORITHM.md#93-the-payload-has-no-end-marker).
- **Per-block overhead dominates small images.** For a 16x16 sample the file is
  larger than the raw pixels. The codec only becomes worthwhile on images of
  a few hundred pixels across, and only at a tolerance that visibly degrades
  the image.
- **`getRate()` reports bytes per pixel, not a compression ratio**, and it is
  based on the bitstream length rather than the size of the file on disk. The
  tables above use the on-disk size. See
  [ALGORITHM.md §9.8](ALGORITHM.md#98-getrate-reports-bytes-per-pixel-not-a-compression-ratio).
- **`isColorful` is inferred from the image dimensions**, not stored in the
  file. See [ALGORITHM.md §9.2](ALGORITHM.md#92-iscolorful-is-inferred-not-stored).

---

## License

Released under the [MIT License](LICENSE).
