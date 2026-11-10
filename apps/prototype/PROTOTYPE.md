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

## File encoding

`Header.h` and `Header.cpp` are kept byte for byte as they were written and
contain Chinese comments. They are **UTF-8 without a BOM** — verified, not
assumed: the bytes of a comment such as `//使用` are `e4 bd bf e7 94 a8`,
which decodes as UTF-8 and fails to decode as GBK.

They are not GBK. The repository's own `Compress_gray` was GBK at first, but
it was converted to UTF-8 in an earlier task, and so were these prototype
sources.

The one thing to watch for is the *display*, not the file: a UTF-8 file with no
BOM is read as the system code page by some Windows tools, so the comments
can show up as mojibake in a console or an editor configured for the local
code page. The bytes on disk are correct UTF-8. Configure the editor to UTF-8
to read them; do not re-encode them.
