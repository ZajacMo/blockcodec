# Changelog

All notable changes to this project are recorded here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [semantic versioning](https://semver.org/).

## [Unreleased]

## [0.1.0] — 2026-11-10
The release in which the coursework project became a reusable library.

### Added

- **The `blockcodec` library.** A self-contained C++17 static library with
  no dependency on Qt. The public headers are in `include/blockcodec/`:
  `blockcodec.hpp`, `bitstream.hpp`, `codec.hpp`, `format.hpp`, `metrics.hpp`
  and `segment.hpp`.
- **The `.bcc` container format.** A binary format with a fixed 20-byte
  header and a self-delimiting payload, specified in
  [`docs/ALGORITHM_BCC.md`](docs/ALGORITHM_BCC.md) precisely enough that an
  independent implementation in another language can produce byte-identical
  output.
- **Colour support.** The channel count is stored explicitly in the header,
  so greyscale, three-channel and four-channel images all round trip
  correctly. The block test compares the largest deviation across the
  channels.
- **The `blockcodec` command line tool** (`apps/cli/`) with three
  subcommands: `compress`, `decompress` and `info`.
- **The `blockcodec_bench` benchmark** (`bench/`), which measures the codec
  against a JPEG baseline *at the matched quality* — for each `sigma` it
  finds the lowest JPEG quality that still reaches the blockcodec PSNR, and
  reports the file size at that quality.
- **The segmentation overlay** (`blockcodec::segmentation_overlay`), which
  draws the boundaries of the blocks the codec chose.
- **The install rules.** The library installs a CMake package that a
  downstream project finds with `find_package(blockcodec)` and uses through
  the exported target `blockcodec::blockcodec`.
- **The test suites**: the bit stream, the container format, the codec, the
  metrics, the robustness, the command line, the benchmark, the reference
  vectors and the golden regression test.
- **Continuous integration.** Four jobs in `.github/workflows/build.yml`:
  Linux with GCC, Linux with Clang, Windows with MSVC, and a build under
  AddressSanitizer and UndefinedBehaviorSanitizer. The Qt GUI is built by a
  fifth job that is allowed to fail, because the library is the deliverable
  and the GUI is not. The test run is checked by `tools/check_ctest.py`,
  which fails on a suite that ran no check, reported a failing check or
  skipped its input, because the exit code of ctest alone would not notice
  any of those.
- **The reference vectors** in `tests/vectors/`: `gradient_16x16.bcc` and
  `gradient_16x16_colour.bcc`, with their reference images, and
  `truncation_3x16.bcc`, a constructed image whose blocks pin the number of
  roundings the interpolation performs.
- **`docs/ALGORITHM_BCC.md`**, the specification of the `.bcc` container.

### Changed

- The project was renamed from *OpenCV Image Processing and Compression Tool*
  to *blockcodec*, and the CMake project from
  `OpenCV-Image-Processing-and-Compression-Tool` to `blockcodec`.
- The Qt GUI was migrated onto the `blockcodec` API. The old `Compress_gray`,
  `Bits`, `Block` and `RBlock` classes are no longer used by it.
- The bit stream is now packed eight bits to a byte. The legacy format
  stored one ASCII character per bit, which made the file on disk eight times
  larger than the bitstream it carried.
- The container header stores the channel count and the pixel width, instead
  of inferring the channel count from the width. The legacy rule misreported
  every image wider than it is tall.
- The payload no longer encodes the gaps between blocks, and the width of
  every field is derived from the header rather than from the writer's own
  position. That makes the payload self-delimiting and the stream much less
  fragile to reimplement.
- The benchmark reports the sizes of the files that are actually written, and
  derives the bits per pixel and the compression ratio from those, so the two
  codecs are measured in the same units.
- The README was rewritten to describe a library rather than a coursework
  project, and to state plainly where the codec wins and where it loses.

### Fixed

- The interpolation is no longer contracted into a fused multiply-add, which
  rounded once instead of twice and could change the value after the
  truncation to `uint8`. The contracted and the uncontracted evaluation were
  compared exhaustively over 140 509 184 cases.
- The corner samples are stored in one order, not two.
- A writer no longer produces a container that the reader would reject.
- The padding bits after the last block are checked, so a corrupted file
  becomes a clean error rather than a silently accepted one.
- The block extents are replaced, not appended to, when a result is passed in
  again.
- The command line tool exits with zero for `--help`, which is a successful
  completion. It exited with the usage-error code, so a script that checked
  the exit status saw a failure where there was none.
- The command line tool prints the dimensions of a container as width by
  height, the order the image formats use and the order the `decompress`
  command already used. It printed them as height by width, so a landscape
  image was reported with its two sides the wrong way round.

### Removed

- The `.nam` container as the format of the library. **The library no longer
  reads the legacy format**; a `.nam` file is rejected with an error rather
  than guessed at. The legacy implementation is kept in `tests/legacy/` as
  the oracle of the golden regression test, and its specification is kept in
  [`ALGORITHM.md`](ALGORITHM.md).

### Notes on the results

The benchmark shows the codec losing to JPEG on every natural image at the
matched quality, by between 2.5× and 9.1× the file size. It wins on the
16×16 samples, because its fixed 20-byte header is smaller than a JPEG
header. The value of this release is the exact format specification, the
reproducible benchmark and the test suite, not a claim to beat JPEG. See
[the README](README.md#benchmarks) for the measured table.

## [Unreleased legacy releases]

Before 0.1.0 the project was a Qt coursework application named *OpenCV Image
Processing and Compression Tool*. Its history is in the git log.

[0.1.0]: https://github.com/ZajacMo/OpenCV-Image-Processing-and-Compression-Tool-Qt/releases/tag/v0.1.0
