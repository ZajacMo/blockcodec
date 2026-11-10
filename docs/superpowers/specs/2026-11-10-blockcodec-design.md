# blockcodec — Design Specification

**Date:** 2026-11-10
**Status:** Approved
**Supersedes:** the three separate `openCV_test` repositories described below

---

## 1. Motivation

Three near-identical copies of the same block-based image codec exist in this
workspace:

| Path | IDE / Build | Shape | Git |
|------|-------------|-------|-----|
| `cpp/CLion/C++Programming/openCV_test` | CLion / CMake | Free functions, command line | Initialised, **zero commits**, no remote |
| `cpp/VS_C++Project/test_openCV` | Visual Studio / `.sln` | Classes, batch benchmark | 1 commit, remote `ZajacMo/...-console` |
| `cpp/QtProject/openCV_test` | Qt Creator / `.pro` | Classes + Qt GUI | 5 commits, remote `ZajacMo/...-Tool` |

A file-by-file comparison after normalising source encoding shows the copies
have drifted far enough apart to be a maintenance liability:

| File | Qt vs VS (whitespace-insensitive) |
|------|-----------------------------------|
| `Block.cpp` / `Block.h` | identical |
| `RBlock.cpp` / `RBlock.h` | identical |
| `Bits.cpp` / `Bits.h` | 7 / 4 lines differ |
| `Compress_gray.cpp` / `.h` | 39 / 8 lines differ |

The `images/` directory (3.5 MB) is duplicated between the Qt and VS copies.

The project is to become a **reusable open-source library** rather than a
coursework folder with three folders.

## 2. Goals

1. One repository, one copy of the algorithm, three consumers.
2. Support for colour images, which the current code claims but does not
   deliver.
3. A real command-line interface.
4. A systematic benchmark suite with a JPEG baseline for context.
5. Unit tests and continuous integration.
6. English documentation and comments throughout.

### Starting point

Commit `a2b975e` ("make the codec reusable, portable and documented") landed
in the Qt repository before this refactor began. It already provides:

- A CMake build with a Qt-free static library `opencv_codec` beside the GUI.
- `ALGORITHM.md`, a specification of the `.nam` container and the codec,
  detailed enough to write an independent implementation.
- A versioned reference vector (`tests/vectors/gradient_16x16`) that an
  independent implementation must decode pixel for pixel.
- A self-test and a sigma sweep benchmark.
- A GitHub Actions workflow for Windows and Ubuntu.
- Fixes for the heap overflow in `Bits::init`, the uninitialised read, the
  PSNR divide-by-zero, the unvalidated dimensions and the GUI's window and
  pointer leaks.

That work is a sound foundation and is preserved. This specification builds on
it: the remaining defects, colour support, the new container format, the CLI
and the full test suite are what still need to be done.

### Non-goals

- Rewriting the compression algorithm itself. The RNAMC block model stays.
- Backwards compatibility with the legacy `.nam` container.
- Preserving the exact byte layout of the legacy container.
- A plugin architecture or an abstract `Codec` interface with multiple
  backends. One algorithm, one implementation.

## 3. Naming

The project is renamed **blockcodec**.

- Repository: `ZajacMo/blockcodec`
- CMake package: `blockcodec`
- Library target: `blockcodec::blockcodec`
- Namespace: `blockcodec`
- CLI executable: `blockcodec`

The name describes the block-based codec without binding the project to the
current algorithm, so the algorithm can be replaced later without a rename.

## 4. Repository layout

```
blockcodec/
├── CMakeLists.txt
├── CMakePresets.json
├── include/blockcodec/           # public headers (the only public surface)
│   ├── blockcodec.hpp             # umbrella header
│   ├── codec.hpp                  # Codec, CompressOptions, CompressResult
│   ├── format.hpp                 # container header definition
│   └── segment.hpp                # segmentation visualisation
├── src/
│   ├── core/                      # the single copy of the algorithm
│   │   ├── bitstream.cpp/.hpp
│   │   ├── block.cpp/.hpp
│   │   ├── rblock.cpp/.hpp
│   │   └── codec.cpp
│   ├── format.cpp
│   └── segment.cpp
├── apps/
│   ├── cli/                       # blockcodec command line
│   ├── qt/                        # Qt GUI (optional component)
│   └── prototype/                 # legacy CLion prototype, not built by default
├── tests/
│   ├── vectors/                   # reference vectors
│   └── ...
├── bench/
├── images/
├── docs/
│   ├── ALGORITHM.md
│   └── ...
└── legacy/                        # legacy sources kept for reference
```

`include/blockcodec/` is the only directory installed. Everything else is
internal.

## 5. Core API

The current `Compress_gray` is a stateful object with a
construct / initialise / compress / save sequence, and its `isNull` member is
shadowed by a same-named local in `compress()` (`Compress_gray.cpp:246`). The
new API is value-semantic and has no hidden state:

```cpp
namespace blockcodec {

struct CompressOptions {
    int sigma = 10;                 // tolerance for the block test
};

struct CompressResult {
    std::size_t bit_size   = 0;    // bits in the payload
    std::size_t byte_size  = 0;    // bytes in the container on disk
    double      bpp        = 0;    // bits per pixel
    double      cr         = 0;    // compression ratio
    double      psnr       = 0;    // dB, infinity for a perfect reconstruction
    double      compress_ms = 0;
    double      decompress_ms = 0;
    std::size_t block_count = 0;
};

class Codec {
public:
    explicit Codec(CompressOptions options = {});

    CompressResult compress(const cv::Mat& source);
    cv::Mat         decompress(const std::vector<std::uint8_t>& container);
    cv::Mat         decompress(const std::string& path);

    CompressResult  last_result() const;
};

}  // namespace blockcodec
```

Errors are reported by throwing `std::runtime_error` with an English message.
The CLI translates an exception into a non-zero exit code and a one-line
message; the GUI catches it and shows it in a message box.

## 6. Colour support

This is the largest functional change and the one with the most risk.

### 6.1 The problem with the current code

`init_compress` sets `cols = img.cols * img.channels()`, so a colour image is
treated as one wide row of interleaved samples. The block model assumes a
spatial neighbourhood, so a block can straddle a pixel boundary and the
"corner colours" are misaligned. Worse, `RBlock` indexes the image with
`img.at<uchar>(i, j)` for `j` up to the flattened width, which reads past the
end of each row, and `result` is allocated with that same flattened width, so
a 512x512 colour image decodes to a 1536x512 greyscale image.

The colour path is therefore **unsafe**, not merely imprecise: it performs an
out-of-bounds read and produces a mis-sized result. The GUI offers a colour
option that reaches it.

### 6.2 Design

Pixels and channels are decoupled. `cols` is always the image width; the
channel count is a separate axis.

- `RBlock`'s `uchar color[2][2]` becomes `uchar color[2][2][C]`
  (4 corners x C channels).
- `isBlock` interpolates each channel separately and compares the **maximum
  absolute deviation across channels** against `sigma`.
- `regain` reconstructs each channel by bilinear interpolation.
- `init_compress` sets `cols = img.cols`; the channel count no longer
  participates in the width computation.

Using the maximum deviation rather than a Euclidean distance is deliberate: for
a single-channel image the two definitions coincide, so **single-channel
behaviour is unchanged**. The golden regression test relies on this property
by comparing block structure and reconstructed pixels, not raw bytes.

### 6.3 Metric definitions

```
bpp  = total_bits / (rows * cols)
cr   = (rows * cols * channels * depth_bits) / total_bits
psnr = 10 * log10(255^2 * rows * cols * channels / sum(diff^2))
```

`bpp` counts all channels, so a colour image is not flattered by the
denominator.

Two things need care here, and they are easy to conflate:

1. **Bits versus bytes.** `getWriteLen()` returns the number of *bits* in the
   bitstream, so `getWriteLen() / (rows * cols)` is already bits per pixel.
   The legacy `getRate()` divides by `rows * cols * 8.0`, which makes it
   **eight times smaller** than the bits-per-pixel value — it is not a
   bits-per-pixel figure despite being documented as one.

   The legacy `.nam` container stores one ASCII character per bit, so the
   file on disk is *eight times larger* than the bitstream it carries. For
   `lena512` at `sigma = 10` the bitstream is 799,631 bits while the file is
   799,632 bytes. The two must never be reported under the same name.

2. **The channel factor.** The VS variant's `getRate()` multiplies by
   `img.channels()` a second time even though `cols` already includes the
   channel count, so its compression ratio is wrong by the channel factor on
   colour images. On greyscale images the two agree, which is why the bug
   went unnoticed.

The CLion prototype's `BPP()` is the reference definition. It adds the
length of the position stream to the colour samples a conforming encoder
would have to write — 16 bits for a block with four corner samples, 8 for one
with two, 4 for an isolated pixel — and divides by the pixel count. It is
therefore a genuine bits-per-pixel figure.

The two class-based implementations instead divide the bit count by eight, so
they report *bytes per pixel* while calling the result a bit count. That is
where the factor-of-eight confusion in the old documentation comes from. The new API reports `bpp` and `cr` as distinct,
explicitly named fields, so the ambiguity cannot recur.

## 7. File format

A new container, `.bcc`, replaces `.nam`. The legacy format is not read.

```
offset  size  field
0       4     magic       "BCC\0"
4       1     version     0x01
5       1     flags       bit 0: colour (multi-channel)
6       4     rows        uint32, little-endian
10      4     cols        uint32, little-endian  (pixel width, not channels)
14      1     channels    uint8
15      1     depth_bits  uint8  (8 / 16 / 32)
16      4     block_count uint32, little-endian
20      n     payload     densely packed bitstream
```

The payload is **densely packed**: the legacy format stored one ASCII
`'0'`/`'1'` character per bit, making the file eight times larger than the
bitstream it carried. The new format packs eight bits per byte.

The payload is a sequence of blocks. Each block is written as:

```
start_row     row_bits
start_col     col_bits
width - 1     width_bits
height - 1    height_bits
corners       8 bits each
```

`row_bits`, `col_bits`, `width_bits` and `height_bits` are all derived from
the header's `rows` and `cols` by the rule "the number of bits needed to
represent the largest value in range", and are therefore identical on both
sides **without either side depending on its own position**. This is what
removes the implicit coupling of the legacy format.

`corners` holds four samples per channel in the order top-left, top-right,
bottom-left, bottom-right. A block that is a single pixel stores all four
corners as well; they are the same pixel, and storing the four samples
uniformly means every block has the same size, which keeps the decoder's
bookkeeping trivial.

The block count is stored in the header, so the decoder knows exactly how
many blocks to read and never has to guess where the payload ends.

### 7.1 How the decoder knows where the payload ends

This deserves stating explicitly, because it is the part a reimplementation is
most likely to get wrong.

The payload is self-delimiting. `block_count` says how many blocks follow,
and every block has a size that is fully determined by the header: the field
widths follow from `rows` and `cols`, and the corner samples are four values
per channel, or one value per channel for a single-pixel block. A decoder
therefore computes the exact bit position at which the payload ends and never
reads the padding.

The final byte is zero padded to a byte boundary. That padding is not part of
the data and is never read, because the decoder stops after `block_count`
blocks.

A decoder must reject a container whose payload is shorter than
`block_count` blocks require, and one whose padding contains non-zero bits.
Both are corrupt files, and both must raise an error rather than decode into
a partial image.

### 7.2 The container holds raw facts, not derived quantities

The header carries only facts about the image and the stream: the
dimensions, the channel count, the colour depth, and the number of blocks.
It carries **no** compression ratio, no bits-per-pixel figure and no quality
score.

The reason is that a derived quantity in the header cannot serve as a check.
An independent decoder reads `bpp` from the header and computes its own
bits-per-pixel from the payload; the two agreeing proves nothing, because
they may well be two exits of the same wrong implementation. Worse, if the
specification leaves it open whether the reader should verify the stored
value, every implementation has to guess.

So the format stores what the data *is*, and the measurements belong to the
tools that read the format: the benchmark computes the bits-per-pixel from
the actual length of the bitstream, and the decoder can do the same. The
acceptance criterion for an independent implementation is therefore a single,
unambiguous one — the decoded pixels match the reference exactly.

This is also why the field widths are fixed. The length of the payload is
then exactly computable from the header, so the bits-per-pixel of a
container is a *measurement* rather than an estimate. The prototype's `BPP()`
had to estimate, because its colour samples were not part of the stream it
measured.

### 7.3 Byte and bit order

- Every multi-byte integer in the header is **little-endian**.
- Within the payload, the **most significant bit comes first** in each byte.
- The payload is **not** aligned to any boundary. A field that follows another
  starts at the very next bit.
- The final byte is **zero padded** to a byte boundary.

These four rules are what an independent implementation needs to reproduce
the stream exactly.

### 7.4 What a decoder must reject, and what it may tolerate

Every rule in this specification states one of two things: whether a decoder
that finds the condition violated **must raise an error**, or whether it
**may tolerate** it. The distinction matters, because a rule that is only
described but not classified leaves the implementer to guess, and two
implementers will guess differently.

A decoder **must raise an error** when:

- the magic number does not match;
- the version is not one it understands;
- `rows` or `cols` is zero;
- the pixel count exceeds `kMaxPixels`;
- `channels` is not between 1 and 4;
- `depth_bits` is not one of the supported values;
- the payload is shorter than `block_count` blocks require;
- a block lies outside the image;
- the padding bits after the last block are not zero.

A decoder **may tolerate**:

- trailing bytes after the payload. A file that was concatenated with
  something else is still decodable, and rejecting it would be
  unfriendly.

Everything else in this specification is exact: a conforming encoder and a
conforming decoder produce identical bits, and a decoder that accepts a
malformed file instead of rejecting it is not conforming.

Note that the padding check is listed as an error even though a decoder that
stops after `block_count` blocks never has to look at the padding. The check
is required, because it turns a corrupted file into a clean error rather
than a silently accepted one, and requiring it in the specification means
every implementation performs it, not only those that happened to write it.

## 8. Defects to fix

These were found by reading the code and are not optional cleanups; an
open-source library must not ship them.

**Already fixed** in commit `a2b975e` ("make the codec reusable, portable and
documented"), which landed before this refactor began:

1. ~~**Heap overflow in `Bits::init`**~~ — the buffer was allocated as
   `new char[pow(1024, size) * 10]` (a fixed 10 MB) and then filled with an
   unbounded `in >> loadedInfo`. A file larger than the guess overflowed the
   buffer. This was remotely triggerable with a crafted file. **Fixed**: the
   buffer is now sized from the real file length, zeroed, and a failed open
   is reported.
2. ~~**Read of uninitialised memory**~~ — **Fixed**: the buffer is zeroed and
   its length comes from the file size rather than from `strlen`.
3. ~~**PSNR divides by zero**~~ on a perfect reconstruction (`log10(x/0)`).
   **Fixed**: returns `infinity`, and mismatched or empty inputs are rejected.
4. ~~**Unvalidated dimensions from the file**~~ — **Fixed**: the decoder
   validates the header and rejects dimensions above `kMaxPixels`.
5. ~~**`windows.h` in a public header** — **Mostly fixed**: the timer uses
   `std::chrono::steady_clock`. The `#include <windows.h>` remains, but is
   guarded by `#ifdef _WIN32` and is no longer required by the implementation.
   It is removed entirely by the core rewrite.

**Still outstanding**, and part of this work:

6. **Colour images read out of bounds.** `init_compress` sets
   `cols = img.cols * img.channels()`, so a colour image is treated as one
   wide row of interleaved samples. `RBlock` then indexes it with
   `img.at<uchar>(i, j)` for `j` up to the flattened width, which reads past
   the end of each row. `result` is allocated as
   `cv::Mat::zeros(rows, cols, CV_8UC1)`, so a 512x512 colour image decodes
   to a 1536x512 greyscale image. The GUI exposes a colour option that
   therefore produces a mis-sized, out-of-bounds result rather than a
   merely imperfect one. **The colour path must be treated as unsafe, not
   merely imprecise.**
7. **`isColorful` is inferred, not stored** — the decoder guesses colour
   from `cols > rows`, which is wrong for any image wider than it is tall.
   A 256x128 greyscale image is reported as colour. The existing test images
   are all square, so this has never surfaced. The new container stores the
   channel count explicitly, and a landscape-greyscale regression test is
   added.
8. **No end marker in the payload** — a truncated file decoded to a partial
   image without error. The new format validates the payload length.
9. **The metrics are ambiguous** — see section 6.3. Bits, bytes and the
   channel factor are conflated.
10. **`Bits::write` shifts past the width of `int`** — `info & (1 << i)` is
    undefined behaviour for `i == 31`. This is **not reachable in the legacy
    code**: every call site uses a width of at most 16, because the gap width
    is bounded by the 16-bit column field and the depth is guarded to 8 bits
    on the encode path. It becomes reachable as soon as the new container
    supports a `depth_bits` greater than 8, so the new bit writer uses
    `std::uint64_t` and shifts within the type's width. The requirement
    belongs to the new bit writer's design, not to the legacy defect list.
11. **The GUI leaked its sub-windows** and dereferenced uninitialised
    `Compress_gray` pointers in their destructors. **Fixed** in `a2b975e`.
12. **`operatorsignals.cpp` was both compiled separately and `#include`d**,
    producing duplicate symbols. **Fixed** in `a2b975e`.

### A note on the gap field width

The width `b` of the gap field is recomputed from the decoder's current
position (`cols - col - !isNull`). This looks like a desynchronisation
hazard, but it is not: the encoder and the decoder advance their positions
by the same rule, so both compute the same width at every step, and an
enumeration over all column counts finds no mismatch. The legacy format is
therefore self-consistent for a single implementation.

The new format nevertheless transmits the width explicitly. That removes the
implicit coupling between the two sides, which is what makes the format
hard to reimplement — the reference vector exists precisely because the
format's rules are easy to get subtly wrong.

## 9. Applications

### 9.1 Command line

```
blockcodec compress   <input> -o <out.bcc> [--sigma N] [--quiet]
blockcodec decompress <in.bcc>  -o <out.png> [--compare <reference>]
blockcodec info       <in.bcc>
blockcodec benchmark  [--images DIR] [--sigmas 1,5,10,20,30,50]
                      [--baseline jpeg] [--csv out.csv]
```

Argument parsing uses **CLI11** (header-only, fetched with `FetchContent`).
Hand-written parsing is error-prone around edge cases and is not worth the
code.

`decompress --compare` additionally computes the PSNR against a reference
image.

### 9.2 Benchmark

The benchmark sweeps an image set across a sigma grid and reports, per
combination: dimensions, sigma, encoded size, BPP, CR, PSNR, encode and
decode time, and block count. Results are written as CSV and as a table.

With `--baseline jpeg`, the same images are additionally encoded by OpenCV's
JPEG encoder at several quality levels, and the report places blockcodec and
JPEG side by side at comparable PSNR. Without that comparison a compression
library has no reference point for how good its numbers are.

The existing VS-version sweep (10 images x 6 sigma values) is the starting
point for this tool.

### 9.3 Qt GUI

Kept as an example application, gated behind `BLOCKCODEC_BUILD_QT` (default
`ON` locally, exercised separately in CI).

- Calls `blockcodec::Codec` directly; no dependency on the core internals.
- The existing colour / greyscale buttons become genuinely functional.
- Metric labels state their units explicitly ("bits/pixel", "compression
  ratio") so the byte/bit ambiguity is not repeated in the UI.
- Image resources are located relative to the executable instead of a
  hard-coded path.

## 10. Tests

GoogleTest, fetched with `FetchContent`.

| Suite | Coverage |
|-------|----------|
| Golden regression | The legacy single-channel implementation is retained as an oracle. Both implementations run on the same image, and the test asserts that they produce **the same block structure and the same reconstructed pixels**. It does **not** compare bitstreams: the new container is densely packed and transmits the gap width explicitly, so its bytes necessarily differ from the legacy ASCII-per-bit stream. What must be preserved is the *algorithm*, not the encoding. See section 10.1. |
| Bitstream | write/read symmetry, every field width, crossing the 1024-bit vector boundary |
| Round trip | Greyscale and colour: dimensions and channel count preserved, PSNR above a floor, block count consistent |
| Container | header serialisation round trip, magic and version validation |
| Robustness | Truncated files, wrong magic, wrong version, absurd dimensions: must throw, never crash |
| Landscape greyscale | A 256x128 greyscale image must round-trip with `channels == 1`. The legacy decoder inferred colour from `cols > rows` and misreported every image wider than it is tall; the existing test images are all square, so this case was never covered. **The assertion must target the channel count carried by the container, not the decoded matrix.** The legacy decoder always allocates a single channel result, so `decoded.channels() == 1` holds even when the inference is wrong and the assertion would prove nothing. |
| Fuzzing | Random bytes and truncations of a valid file: must fail cleanly, no undefined behaviour |
| Metrics | BPP, CR and PSNR checked against hand-computed values for known inputs |

### 10.1 The golden regression test in detail

The oracle is the current `Compress_gray`, kept under `tests/legacy/` and
compiled into the test binary. It is never installed and never shipped.

For each image in the reference set and for each `sigma`, the test asserts:

1. The legacy and the new encoder produce the **same number of blocks**.
2. The legacy and the new encoder produce the **same block origins and
   corners**, compared as sets of `(row, col, rb_row, rb_col)`.
3. The reconstructed images are **pixel-identical**.

Because the block search decides a block purely from its corner samples and
the tolerance `sigma`, and because the multi-channel rule (section 6.2)
reduces to the single-channel rule when there is one channel, the block
structure must match exactly. A mismatch means the refactor changed the
algorithm, which the test reports as a failure listing the first differing
block.

The existing `tests/vectors/gradient_16x16` vector stays as it is: it pins
the legacy decoder's behaviour, which remains valid for the legacy format.

The test suite runs under **AddressSanitizer and UndefinedBehaviorSanitizer**
on Linux. This is how defects 1, 2 and 3 above are demonstrated fixed.

## 11. Continuous integration

GitHub Actions:

```
ci.yml
├── build-test   ubuntu-latest (gcc, clang) and windows-latest (MSVC)
│                configure -> build -> ctest
├── sanitizers   ubuntu-latest, ASan + UBSan
├── qt-build     aqtinstall for Qt 6, builds apps/qt, allowed to fail
└── format       clang-format check
```

The Windows job is required because the code was developed there, but the core
must be portable, which is why `windows.h` is removed.

## 12. Packaging and documentation

- `install` target with `export(blockcodecTargets)` and a version file, so
  downstream projects can use `find_package(blockcodec)`.
- Doxygen API documentation, generated by an optional CMake target.
- `README.md` and `README_CN.md`: algorithm overview, build instructions,
  measured benchmark tables including the JPEG comparison, and usage
  examples.
- `ALGORITHM.md`: the format and algorithm specification, so an independent
  implementation can be written against it.
- `CONTRIBUTING.md`, `CHANGELOG.md`, `.clang-format`, issue and pull request
  templates.
- The existing MIT licence is retained (Copyright (c) 2024 Zajac Mo).

**All documentation and source comments are written in English.** The Chinese
README remains as a translation; Chinese comments in the source are removed.

## 13. Git history

The Qt repository is the base: it has the best history and already carries the
licence and the README.

1. Rename the GitHub repository to `blockcodec`. GitHub redirects the old URL
   automatically, so **no history rewrite and no force-push are required**.
   The repository is currently `ZajacMo/OpenCV-Image-Processing-and-Compression-Tool-Qt`
   (renamed from `...-Tool`; the local `origin` still holds the older name).
   Both names resolve to the same commit because GitHub follows the
   redirect, and the local URL is updated to the current name.
2. Fetch the VS repository and merge with `--allow-unrelated-histories`, so
   its single commit enters the main history. Its original sources are kept
   under `legacy/vs-console/` for reference before being superseded by the
   new CLI.
3. Commit the CLion prototype (currently zero commits, no remote, and
   therefore at risk of loss) under `apps/prototype/`, with a `PROTOTYPE.md`
   explaining how its functions correspond to the new implementation. Its
   `cmake-build-debug/` directory (353 KB of CMake cache and object files)
   is deleted before the import, and `cmake-build-debug/` is added to
   `.gitignore` — the current ignore rules cover `build/` and `build-*/` but
   not CLion's default output directory name.
4. Land the refactor as a distinct, well-described commit.

The VS repository's remote is left in place and archived after the merge.

## 14. Delivery order

Each stage leaves the tree compiling and testable.

1. **Foundation** (partly done in `a2b975e`): repository rename on GitHub,
   history preservation, the VS and CLion sources imported under `legacy/`
   and `apps/prototype/`.
2. **Core refactor**: the `include/blockcodec/` + `src/core/` layout, the new
   value-semantic API, colour support, and the new `.bcc` container.
   The golden regression test lands with it. **The critical stage.**
3. **CLI and benchmark**, including the JPEG baseline.
4. **Full test suite and CI**: GoogleTest, sanitizers, the multi-platform
   workflow.
5. **Qt GUI migration** onto the new API.
6. **Documentation, README, Doxygen, release preparation.**

## 15. Risks

| Risk | Mitigation |
|------|------------|
| The colour refactor changes existing single-channel output | The golden regression test compares block structure and reconstructed pixels against the retained legacy implementation (section 10.1). |
| The gap-field width defect is hard to fix without changing the stream | Fix it in the new format, where the encoder and decoder derive the width from an explicitly transmitted value rather than from local state. The legacy stream is not read, so there is no compatibility constraint. |
| The Qt GUI depends on internals that the refactor changes | The GUI is migrated last, once the public API is stable. |
| Another session may be working in the same directory | The work committed in `a2b975e` is preserved; the refactor proceeds on top of it. |

## 16. Environment

Verified on this machine:

- OpenCV 4.10.0 at `C:\Program Files\opencv`
- Qt 6.6.2 (msvc2019_64) at `C:\Qt\6.6.2`
- MSVC 14.44 (Build Tools 2022) with `vcvars64.bat`
- CMake 3.31.6, 16 cores
- Network access confirmed for `FetchContent`

Note that the `qmake` on `PATH` belongs to an Anaconda Qt 5.15.2
installation and is **not** the Qt 6 used by the project. Builds must point at
`C:\Qt\6.6.2\msvc2019_64`.
