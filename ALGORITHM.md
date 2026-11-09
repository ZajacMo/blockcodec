# The `.nam` Block Codec — Format and Algorithm Specification

This document specifies the compression codec implemented by
`Compress_gray`, `Bits`, `Block` and `RBlock`, so that an independent
implementation in another language can produce byte-identical output and
decode the reference vectors in `tests/vectors/`.

Everything here was derived from the source in this repository and verified
against the reference vectors. Where the implementation deviates from what
you might expect, or contains a latent defect, it is called out explicitly
in [Known issues](#known-issues).

---

## 1. Overview

The codec is a **lossy, block-based image codec for 8-bit greyscale images**.

An encoder scans the image and partitions it into rectangular *blocks*. Each
block is described by the four colours at its corners; the interior is
reconstructed by bilinear interpolation. Pixels that cannot be represented by
any block become *isolated pixels* and are stored verbatim.

The compression ratio and the reconstruction quality are therefore two
dials of the same parameter, `sigma`:

| `sigma` | Behaviour | Effect |
|---------|-----------|--------|
| small (e.g. 1) | Only near-perfectly uniform blocks are accepted | Large files, high PSNR |
| large (e.g. 50) | Coarse blocks are accepted | Small files, lower PSNR |

Measured on `images/lena512.bmp` (512x512) and `images/baboonzyp512.bmp`
(512x512) with this implementation. "File vs raw" is the size of the `.nam`
file on disk divided by the size of the raw pixel data, so a value **below
1.0 means the file is smaller than the original**.

| `sigma` | lena512 bits | lena512 file vs raw | lena512 PSNR | baboon bits | baboon file vs raw | baboon PSNR |
|---------|--------------|---------------------|--------------|-------------|--------------------|-------------|
| 1 | 3 282 134 | 12.52x | 59.23 dB | 3 512 520 | 13.40x | 67.26 dB |
| 5 | 1 699 187 | 6.48x | 42.29 dB | 3 161 627 | 12.06x | 46.81 dB |
| 10 | 799 631 | 3.05x | 36.39 dB | 2 543 844 | 9.70x | 38.48 dB |
| 20 | 368 150 | 1.40x | 30.38 dB | 1 689 296 | 6.44x | 31.26 dB |
| 30 | 226 626 | 0.86x | 26.69 dB | 1 194 116 | 4.56x | 27.30 dB |
| 50 | 103 842 | 0.40x | 22.36 dB | 652 517 | 2.49x | 22.56 dB |

Three things worth noticing:

- **The container costs a factor of eight.** The bitstream is what the codec
  produces; the `.nam` file stores one ASCII character per bit, so it is
  eight times larger on disk than the bitstream it carries.
- **A small `sigma` makes the file much larger.** At `sigma = 1` every block
  costs more to describe than the pixels it replaces, and the file is 12.5x
  the size of the original. Only from roughly `sigma = 30` does the format
  become smaller than the raw image, at a noticeable quality cost.
- **The useful `sigma` depends on the image.** The smooth `lena512`
  compresses far better than the textured `baboonzyp512` at the same
  tolerance, because a textured image offers few large uniform rectangles.

(`sigma = 10` is the default in the GUI. These numbers were produced by
`bench_sweep`; see [§8](#8-verifying-a-port).)

---

## 2. Terminology

- **rows**, **cols** — the image height and width. `cols` is stored in the
  header as `width * channels`; for the greyscale codec used here,
  `channels == 1`.
- **marker matrix** — an internal `rows x cols` matrix of small integers
  describing the block layout. Values:
  - `0` — unencoded, not covered by any block
  - `1` — the **top-left corner** of a rectangular block
  - `2` — the **bottom-right corner** of a rectangular block
  - `3` — an **isolated pixel** (a pixel that no block could cover)
- **lt**, **rb** — the top-left and bottom-right corners of a block.
- **gap** — the distance from the previous encoded pixel in the same row.

---

## 3. The `.nam` container

A `.nam` file is a **text file containing one ASCII character per bit**: every
bit is written as the character `'0'` or `'1'`. The file therefore has
exactly as many bytes as the stream has bits — it is *not* byte-packed, and
*not* binary.

```
+---------------------------+---------------------------+------------------+
| rows (16 bits)            | cols (16 bits)            | payload          |
+---------------------------+---------------------------+------------------+
```

All multi-bit integers are **big-endian** (most significant bit first) and
**unsigned**.

### 3.1 Header

| Field | Bits | Description |
|-------|------|-------------|
| `rows` | 16 | Image height |
| `cols` | 16 | Image width (times the channel count) |

There is no magic number, no version field and no checksum. A decoder must
therefore validate the header itself before allocating memory; see
[§7](#7-decoder-requirements).

### 3.2 Payload

The payload is a sequence of *rows*. Each row encodes the positions and
kinds of the encoded pixels in that row, in increasing column order.

The stream is read as a sequence of **tokens**. At the start of each token
the decoder reads:

1. **A row-continuation flag** — 1 bit.
   - `0` — the current row has no more encoded pixels; advance to the next
     row and start a fresh row.
   - `1` — an encoded pixel follows; read the token body.

2. **The token body**, which begins with the *block kind*:
   - `1` → a **rectangular block** (marker `1`)
   - `01` → the block's **end corner** (marker `2`)
   - `00` → an **isolated pixel** (marker `3`)

   These two bits are read as: read one bit; if it is `1` the kind is a
   rectangular block, otherwise read a second bit which selects `01`
   (end corner) or `00` (isolated pixel).

3. **The gap** — the number of bits used to store the distance from the
   previous encoded pixel of the same row. The width is computed as

   ```
   b = ceil(log2(cols - col - (isNull ? 0 : 1)))
   ```

   where `col` is the column of the previously decoded pixel in this row and
   `isNull` is true while no pixel has yet been decoded in this row. If the
   expression yields `0`, `b` is forced to `1`.

   The value stored is

   ```
   gap = column_of_this_pixel - col - (isNull ? 0 : 1)
   ```

   written in `b` bits. After reading it, the decoder advances:

   ```
   col += gap + (isNull ? 0 : 1)
   ```

   The `+ (isNull ? 0 : 1)` term accounts for the fact that `col` holds the
   index of the previous pixel, so the next gap is measured from
   `previous + 1`.

4. **The pixel data**, which depends on the block kind:
   - **Rectangular block** (kind `1`) — four colours, 8 bits each, in the
     order

     ```
     colour[0][0]  colour[0][1]  colour[1][0]  colour[1][1]
     ```

     that is: top-left, top-right, bottom-left, bottom-right.

     These four colours describe the block. The block's extent is *not*
     stored explicitly — the decoder recovers it by scanning for the
     matching end corner, see [§5](#5-decoding).

   - **End corner** (kind `01`) — no pixel data follows.

   - **Isolated pixel** (kind `00`) — one colour, 8 bits.

### 3.3 Termination

The stream ends when the decoder runs out of bits, or when the row counter
reaches `rows`. **There is no explicit end-of-stream marker.**

---

## 4. Encoding

### 4.1 Block validity

A candidate block spanning corners `lt` and `rb` is *valid* when, for every
pixel `(i, j)` inside the rectangle, the actual pixel value differs from
the bilinearly interpolated value by no more than `sigma`.

The interpolated value is computed from the four corner colours:

```
gamma_i = (rb.x == lt.x) ? 0 : (i - lt.x) / (rb.x - lt.x)
gamma_j = (rb.y == lt.y) ? 0 : (j - lt.y) / (rb.y - lt.y)

left(i)  = colour[0][0] + (colour[1][0] - colour[0][0]) * gamma_i
right(i) = colour[0][1] + (colour[1][1] - colour[0][1]) * gamma_i

model(i, j) = left(i) + (right(i) - left(i)) * gamma_j
```

This is standard bilinear interpolation over the block's rectangle.

### 4.2 Block search

The encoder walks the image in row-major order. At the first unmarked pixel
`lt`, it searches for the largest valid block with `lt` as its top-left
corner:

1. Extend along the **diagonal**: while the block `(lt, (rb.x+1, rb.y+1))`
   is valid, grow both coordinates.
2. Extend **horizontally**: while the block `(lt, (rb.x, rb.y+1))` is valid,
   grow `rb.y`.
3. Extend **vertically**: while the block `(lt, (rb.x+1, rb.y))` is valid,
   grow `rb.x`.

If the resulting block is a single pixel (`lt == rb`), it is recorded as an
**isolated pixel**. Otherwise it is recorded as a **rectangular block**, with
marker `1` at `lt` and marker `2` at `rb`.

Every pixel covered by the block is then marked as encoded, and the scan
resumes at the end of the covered region.

> **Note.** The search order (diagonal, then horizontal, then vertical) is
> part of the format: it determines which blocks are produced for a given
> image, and therefore the exact output bytes.

### 4.3 Serialisation

The marker matrix is then serialised row by row, as described in
[§3.2](#32-payload). For each row:

- A pixel with marker `0` contributes nothing, unless it is the *last*
  column of the row, in which case a single `0` bit is written to signal the
  end of the row.
- A pixel with marker `1`, `2` or `3` is preceded by a `1` bit (the
  row-continuation flag) and then by its kind bits and data.

---

## 5. Decoding

The decoder rebuilds the marker matrix from the payload and then recovers
each block's extent.

For a block starting at `lt`:

- If the marker at `lt` is `3`, the block is an isolated pixel and has no
  extent.
- Otherwise the decoder extends **horizontally** and then **vertically**,
  accepting a candidate corner `rb` when `isPeer(lt, rb)` holds:

  ```
  isPeer(lt, rb) is true when:
    - the cell (rb.x, rb.y) is unmarked, and its marker is 2 or 0
    - no cell in the column of rb between lt.x and rb.x is marked, and
      none of them carries a marker other than the one at (lt.x, rb.y)
    - no cell in the row of rb between lt.y and rb.y is marked, and
      none of them carries a marker other than the one at (rb.x, lt.y)
  ```

  The recovered `rb` is the block's bottom-right corner. The four corner
  colours, which were stored in the payload, are then used to fill the
  rectangle by the same bilinear interpolation used during validation.

Blocks are consumed in the same order in which they were produced by the
encoder, so the decoder walks the block list with a running index.

---

## 6. PSNR

The peak signal-to-noise ratio is computed over all samples:

```
mse   = sum over all samples of (original - reconstructed)^2
psnr  = 10 * log10( (255^2 * width * height * channels) / mse )
```

A perfect reconstruction gives `mse == 0`, in which case the PSNR is
infinite. (The reference implementation now returns `infinity` for this
case; see [Known issues](#known-issues).)

---

## 7. Decoder requirements

A conforming decoder **must**:

1. Read the file as ASCII text and treat each character as one bit.
   `'0'` is a zero bit, `'1'` is a one bit. Any other character is a
   malformed file.
2. Validate `rows` and `cols` from the header before allocating. The
   reference implementation rejects non-positive dimensions and any image
   larger than 268 megapixels (`Compress_gray::kMaxPixels`).
3. Reject a file whose length is shorter than 32 bits (the header).
4. Treat the end of the stream as the end of the payload, and cope with a
   truncated final row.

---

## 8. Verifying a port

`tests/vectors/` contains a reference vector:

| File | Purpose |
|------|---------|
| `gradient_16x16_source.bmp` | The synthetic source image (16x16, greyscale) |
| `gradient_16x16.nam` | The compressed stream, produced by the reference encoder |
| `gradient_16x16_reference.bmp` | The expected decoding, produced by the reference decoder |

The source is deterministic: a diagonal gradient
(`value = x * 8 + y * 4`) with a constant `200` patch in the 4x4 top-left
corner, which forces the encoder to emit both rectangular blocks and
isolated pixels.

A conforming implementation must decode `gradient_16x16.nam` to
**exactly** the pixels in `gradient_16x16_reference.bmp`. The reference
decoder reproduces the source image exactly, so the PSNR of the reference
is infinite.

The vector can be regenerated with:

```
codec_vector_gen tests/vectors
```

The self-test that checks both the round trip and the reference vector is:

```
codec_selftest
```

---

## 9. Known issues

These are properties of the current implementation. They are documented
rather than silently changed, because changing them would alter the
compressed output and invalidate the reference vectors.

### 9.1 Colour images are not really supported

`init_compress` sets `cols = width * channels`, so a colour image is
treated as a single wide row of interleaved samples. The block model
assumes a spatial neighbourhood, so blocks may span across colour
boundaries. The GUI exposes a "colour" option, but the codec should be
treated as **greyscale only**. The reference implementation now rejects
non-`CV_8U` inputs but still accepts 3-channel input.

### 9.2 `isColorful` is inferred, not stored

`init_uncompress` sets `isColorful = (cols > rows)`, which is a heuristic
rather than information carried in the file. For images that are wider than
they are tall this produces the wrong answer.

### 9.3 The payload has no end marker

A truncated file decodes to a partial image without an error. The decoder
stops when it runs out of bits.

### 9.4 Rows are not self-delimiting

The row-continuation flag is only written for the *last* column of a row
when the row contains no encoded pixel. A row that contains encoded pixels
ends implicitly when the next token's gap computation places it beyond
`cols`. This works, but it means a decoder cannot skip rows without
decoding them.

### 9.5 The bit width of the gap depends on the decoder's position

The width `b` of the gap field is recomputed from `cols - col - 1`. An
encoder and a decoder that disagree about the position of the previous
pixel will disagree about the width of the field, and the stream will
desynchronise. This is the most likely source of incompatibility between
implementations.

### 9.6 `getPSNR` divides by zero on a perfect reconstruction

The original implementation computed `log10(x / 0)`, which is undefined
rather than infinite. This has been fixed to return `infinity`; the fix
does not affect the compressed output.

### 9.7 The decoder trusts the file's dimensions

The original implementation allocated memory sized directly from the header
without validation, so a corrupt file could request an enormous
allocation. This has been fixed; see [§7](#7-decoder-requirements).

### 9.8 `getRate()` reports bytes per pixel, not a compression ratio

`getRate()` is inherited from the original implementation and computes

```
getRate() = bitstream_bits / (rows * cols * 8)
```

which is a **bytes-per-pixel** figure, not a ratio, and it is based on the
bitstream length rather than on the size of the file on disk. Because the
container stores one byte per bit, the two differ by a factor of eight.

The number is used unchanged here, because changing it would change the
behaviour the GUI reports. Consumers that need a true compression ratio
should divide the size of the file on disk by the size of the raw pixel
data, as `bench_sweep` does.

### 9.9 The bitstream length is reported in bits, not bytes

`getWriteLen()` returns the number of *bits* produced by the encoder, which
is why the `.nam` file it produces is eight times that many bytes. The
method is named `getWriteLen` rather than `getWriteBytes` for that reason.
