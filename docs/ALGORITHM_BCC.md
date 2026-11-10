# The `.bcc` container format

This document specifies the `.bcc` container written and read by the
`blockcodec` library. It is written so that an independent implementation, in
any language, can produce byte-identical output and decode the reference
vectors in `tests/vectors/`.

Everything here was derived from the implementation and verified against it.
Where the format is unusual, or where an implementation could plausibly get
it wrong, it is called out explicitly.

---

## 1. What the codec does

The codec is a **lossy, block-based image codec**.

The encoder partitions an image into rectangular *blocks*. Each block is
described by the colour samples at its four corners; the interior is
reconstructed by bilinear interpolation. Pixels that no block can represent
become *isolated pixels* and are stored verbatim.

The quality and the size are two dials of the same parameter, `sigma`:

| `sigma` | Behaviour | Effect |
|---------|-----------|--------|
| small (e.g. 1) | only near-uniform blocks are accepted | large files, high PSNR |
| large (e.g. 50) | coarse blocks are accepted | small files, lower PSNR |

The useful `sigma` depends on the image. A smooth image compresses far
better than a textured one at the same tolerance, because a textured image
offers few large uniform rectangles.

---

## 2. The container

A container is a fixed-size header followed by a packed payload.

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

### 2.1 The fields

**`magic`** is the four bytes `B`, `C`, `C`, `0x00`. The last byte is a null
rather than a printable character, so that a container is not mistaken for
text.

**`version`** is `0x01`. A decoder that does not know the version must
reject the container rather than guess.

**`flags`** is a bit field. Only bit 0 is defined, and it is set when the
image has more than one channel. The remaining bits are reserved and must be
written as zero. A decoder must ignore the reserved bits.

**`rows`** and **`cols`** are the height and the width of the image in
pixels. `cols` is the **pixel** width: it does not include the channels.
This is the single most important difference from the legacy format, whose
`cols` was the width times the channel count.

**`channels`** is the number of channels: 1 for greyscale, 3 for the common
colour layout, 4 when the image carries an alpha channel.

**`depth_bits`** is the number of bits per colour sample. This version
defines **8 only**, and a container that declares anything else is
rejected. The payload always carries 8 bit samples, so a container that
declared a deeper sample would be read at the wrong width.

**`block_count`** is the number of blocks in the payload. The payload is
self-delimiting, so this is a fact about the format rather than a hint: see
section 4.

### 2.2 The header holds no derived quantities

The header carries **no** compression ratio, no bits-per-pixel figure and no
quality score.

The reason is that a derived quantity in the header cannot serve as a check.
An independent decoder reads `bpp` from the header and computes its own
bits-per-pixel from the payload; the two agreeing proves nothing, because
they may well be two exits of the same wrong implementation. Worse, if the
specification leaves it open whether the reader should verify the stored
value, every implementation has to guess.

So the format stores what the data *is*, and the measurements belong to the
tools that read the format. The bits-per-pixel of a container is computed
from the length of the payload, which is exact because every block has the
same size.

---

## 3. The payload

The payload is a sequence of blocks. Each block is written as:

```
start_row       row_bits
start_col       column_bits
width  - 1      extent_bits
height - 1      extent_bits
corners         8 bits each
```

The widths are not stored in the stream. They are derived from the header,
so that the encoder and the decoder compute them the same way without either
of them depending on its own position.

- `row_bits` is the number of bits needed to represent every value in
  `[0, rows - 1]`.
- `column_bits` is the number of bits needed to represent every value in
  `[0, cols - 1]`.
- `extent_bits` is the number of bits needed to represent every value in
  `[0, max(rows, cols) - 1]`.

"The number of bits needed to represent every value in `[0, n]`" is the
smallest `k` such that `2^k > n`. For `n = 0` it is 1, not 0: every field is
at least one bit wide.

`width - 1` and `height - 1` are written rather than `width` and `height`,
so that a one pixel wide block writes 0.

### 3.1 The corner samples

`corners` holds four samples per channel, in the order **top-left,
top-right, bottom-left, bottom-right**. The samples are stored **per
channel**: for each channel, its four corner samples in that order.

That is, for a three channel image the layout is:

```
ch0.tl  ch0.tr  ch0.bl  ch0.br
ch1.tl  ch1.tr  ch1.bl  ch1.br
ch2.tl  ch2.tr  ch2.bl  ch2.br
```

A block that is a single pixel stores all four corners as well. They are the
same pixel, and storing the four samples uniformly means every block has the
same size, which keeps the decoder's bookkeeping trivial.

Each sample is 8 bits. This version of the format is for 8 bit images only.

### 3.2 The block order

The blocks are written in the order the encoder found them: rows from top to
bottom, and within a row from left to right. A block that covers several
rows therefore appears before the blocks that continue on the next row.

---

## 4. Where the payload ends

The payload is **self-delimiting**.

`block_count` says how many blocks follow, and every block has a size that is
fully determined by the header: the field widths follow from `rows` and
`cols`, and the corner samples are four values per channel. A decoder
therefore computes the exact bit position at which the payload ends, and
never reads the padding.

The final byte is zero padded to a byte boundary. That padding is not part
of the data and is never read, because the decoder stops after `block_count`
blocks.

---

## 5. Byte and bit order

- Every multi-byte integer in the header is **little-endian**.
- Within the payload, the **most significant bit comes first** in each byte.
- The payload is **not** aligned to any boundary. A field that follows
  another starts at the very next bit.
- The final byte is **zero padded** to a byte boundary.

These four rules are what an independent implementation needs to reproduce
the stream exactly.

---

## 6. The block model

The block search decides a block purely from its corner samples and the
tolerance `sigma`, so the interpolation is part of the format.

### 6.1 The interpolation

The value at (column, row) is the bilinear interpolation of the four corner
samples:

```
ratio_i = (i - a) / (b - a)          for a != b, and 0 for a == b

left    = truncate_u8(c_tl + (c_bl - c_tl) * ratio_row)
right   = truncate_u8(c_tr + (c_br - c_tr) * ratio_row)
value   = truncate_u8(left  + (right - left)  * ratio_column)
```

where `truncate_u8` truncates towards zero and keeps the result in
`0..255`.

Two properties of this expression matter:

- **The intermediate results are truncated to an unsigned char.** The second
  interpolation therefore works on integers, not on the unrounded doubles.
- **The multiplication must not be contracted into a fused multiply-add.**
  GCC permits that by default and MSVC does not, and a fused operation
  rounds once instead of twice, which can change the value after the
  truncation.

The order of the two truncations is part of the format, and it is not the
same as interpolating all four corners first and truncating once at the end.
An implementation that computes the four corner interpolations in full
precision, combines them, and truncates only the final result does **not**
produce the same value, and therefore does not choose the same blocks. The
truncation happens after the vertical interpolation and again after the
horizontal one, in that order.

The second point needs checking rather than assuming. It is invisible in
most expressions and would be invisible here too, if the truncation did not
make it visible.

The check is exhaustive, and it has to use the rates the codec actually
produces. Those are `n/d` for an arbitrary extent `d`, not for a fixed
denominator. A sweep over `n/64` proves nothing: every such rate is exactly
representable as a double, so the product is exact and the two evaluations
can never differ. Sweeping the real rates instead, over all pairs of corner
samples in `0..255` and all the fractions `n/d` for `d` in `1..64`, the
contracted and the uncontracted evaluation differ in 79763 of 140509184
cases. The first is `a = 5`, `b = 0`, `n = 4`, `d = 5`, where the
contracted form gives 0 and the uncontracted form gives 1.

So the rule is not optional, and an implementation that contracts the
multiply and the addition will choose different blocks on some images. The
rule for an implementation is therefore stated as a **value**, not as a
prohibition, because a language may not be able to express the prohibition:

1. `rate` is the double nearest to `n/d`.
2. `product` is the double product of `(b - a)` and `rate`.
3. `total` is `a + product`, again rounded to the nearest double.
4. The result is `total` truncated towards zero into `0..255`.

Step 2 and step 3 are **two separate roundings**. The product must be
materialised before the sum is computed, and the compiler must not fuse the
two into a single instruction. A fused multiply-add computes `a + (b-a) *
rate` with one rounding instead of two, and that changes the value.

This is a requirement on the intermediate value rather than a prohibition
on the compiler, because a language may not be able to express the
prohibition. How it is met depends on the language:

=== "C++"

    // The volatile keeps the compiler from fusing the two operations.
    // Without it, -ffp-contract=fast or -march=native fuses them.
    volatile double product = static_cast<double>(b - a) * rate;
    volatile double sum = static_cast<double>(a) + product;
    return static_cast<int>(static_cast<unsigned char>(sum));

=== "Python"

    # Python evaluates the two operations separately and rounds each to
    # the nearest double, so no extra care is needed.
    def interpolate(a, b, n, d):
        rate = n / d                   # double, rounded to nearest
        product = float(b - a) * rate  # double, rounded to nearest
        total = float(a) + product     # double, rounded to nearest
        return int(total) & 0xFF       # truncate towards zero

=== "Rust"

    // The intermediate is stored in a variable, which the compiler may
    // still fold. A black_box or an explicit rounding step keeps it.
    fn interpolate(a: u8, b: u8, n: u32, d: u32) -> u8 {
        let rate = (n as f64) / (d as f64);
        let product = (b as f64 - a as f64) * rate;
        let total = (a as f64) + product;
        // The value is always in 0..255, so the truncation is a cast.
        total as u8
    }

=== "Java"

    // Strict floating point is required: the x87 extended precision of
    // the 32 bit x86 ABI keeps intermediates in 80 bit registers, which
    // rounds differently.
    public static int interpolate(int a, int b, int n, int d) {
        final double rate = (double) n / (double) d;
        final double product = ((double) (b - a)) * rate;
        final double total = ((double) a) + product;
        return (int) (byte) total & 0xFF;
    }

This is **not** exact rational arithmetic. Computing the rate as a rational
and truncating the exact quotient gives a different answer in many cases,
because `n/d` is rounded to the nearest double before the multiplication
happens. The example is `a = 255`, `b = 0`, `n = 1`, `d = 2`, where the
double form gives 127 and the exact rational gives 128.

The value that this rule produces is stable across compilers. The
interpolation as implemented above was measured under GCC 13 with
`-ffp-contract=fast`, with `-ffp-contract=off`, with `-O0`, with
`-march=native`, and under MSVC 19.44: all five produce the same result for
every one of the 140509184 cases.

The interpolated value never leaves `0..255`. The rate is in `[0, 1]` and
the endpoints are bytes, so the result is a convex combination of them. This
is worth stating, because an implementation that does not know it has to
choose between wrapping, saturating and erroring, and each choice gives a
different implementation.

### 6.2 The block test

A candidate block is accepted when every pixel inside it differs from the
interpolated value by no more than `sigma`. For a colour image the
comparison is the **largest** deviation across the channels, so that a
single channel image reduces to the greyscale rule exactly.

A pixel that is already covered by another block disqualifies the candidate.

### 6.3 Growing a block

From a starting corner, the encoder grows the block in three steps:

1. **Along the diagonal**, while the block one step further along the
   diagonal is accepted.
2. **Along the columns**, while the block one column further is accepted.
3. **Along the rows**, while the block one row further is accepted.

The three steps are in that order, and the order matters: it decides the
shape of the blocks, and therefore the size of the file.

---

## 7. What a decoder must reject, and what it may tolerate

Every rule in this specification states one of two things: whether a decoder
that finds the condition violated **must raise an error**, or whether it
**may tolerate** it. The distinction matters, because a rule that is only
described but not classified leaves the implementer to guess, and two
implementers will guess differently.

A decoder **must raise an error** when:

- the magic number does not match;
- the version is not one it understands;
- `rows` or `cols` is zero;
- the pixel count exceeds 268435456 (2^28);
- `channels` is not between 1 and 4;
- `depth_bits` is not 8;
- the payload is shorter than `block_count` blocks require;
- a block lies outside the image;
- the padding bits after the last block are not zero.

A decoder **may tolerate**:

- trailing bytes after the payload. A file that was concatenated with
  something else is still decodable, and rejecting it would be unfriendly.

Everything else in this specification is exact: a conforming encoder and a
conforming decoder produce identical bits, and a decoder that accepts a
malformed file instead of rejecting it is not conforming.

Note that the padding check is listed as an error even though a decoder that
stops after `block_count` blocks never has to look at the padding. The check
is required, because it turns a corrupted file into a clean error rather
than a silently accepted one, and requiring it in the specification means
every implementation performs it, not only those that happened to write it.

A writer must not produce a container that a reader would reject. The same
conditions are therefore checked when writing.

---

## 8. Verifying an implementation

The reference vectors in `tests/vectors/` are what an independent
implementation is checked against. **Each vector targets one specific class
of mistake**, and a decoder that is wrong in that way produces a different
image on that vector and not on the others:

| Vector | The mistake it catches |
|--------|------------------------|
| `gradient_16x16` | the block layout and the field widths |
| `gradient_16x16_colour` | the per-channel order of the corner samples |
| `truncation_3x16` | the number of roundings in the interpolation |

Each vector is three files:

| File | Purpose |
|------|---------|
| `gradient_16x16_source.bmp` | the synthetic source image, 16x16, one channel |
| `gradient_16x16.bcc` | the container the encoder produces from it |
| `gradient_16x16_reference.bmp` | the image the container decodes to |

A second, colour one, which catches a decoder that reads the corner
samples in the wrong order:

| File | Purpose |
|------|---------|
| `gradient_16x16_colour_source.bmp` | the same picture in three channels, 16x16 |
| `gradient_16x16_colour.bcc` | its container |
| `gradient_16x16_colour_reference.bmp` | the image it decodes to |

And a third, which catches a decoder that truncates only at the end
rather than after each interpolation:

| File | Purpose |
|------|---------|
| `truncation_3x16_source.bmp` | a constructed image, 16x3, one channel |
| `truncation_3x16.bcc` | its container |
| `truncation_3x16_reference.bmp` | the image it decodes to |

The third vector exists because the other two do not catch a decoder that
truncates only at the end. Its blocks have corner values for which the two
rules disagree, so a decoder that applies the wrong one produces a different
image there and not on the others.

The source of the first two is deterministic: a diagonal gradient
(`value = x * 8 + y * 4`) with a constant `200` patch in the 4x4 top-left
corner, which forces the encoder to emit both rectangular blocks and
isolated pixels. The colour vector holds the same picture with a different
value in each channel, so that a decoder which reads the corner samples in
the wrong order produces a different image.

The source of the third is a three by three tile placed at the left of a
three by sixteen image whose remainder is a constant 255, so that the tiles
cannot merge. The tile's corners are 0, 0, 29 and 31, and its interior
pixel holds 14. The two truncation rules predict 14 and 15 for that pixel.

All three vectors use `sigma = 10`.

A conforming implementation must decode the container to **exactly** the
pixels of the reference image. Not approximately: pixel for pixel, with no
tolerance. The reference is produced by the encoder, so a decoder that
reproduces it has reproduced the block search and the interpolation exactly.

The acceptance criterion is a single one. Metrics such as the bits-per-pixel
or the compression ratio are **not** acceptance criteria, because a
verification tool and the implementation it checks could compute them the
same way and agree with each other while both being wrong.

The vectors are regenerated with:

```
blockcodec_vector_gen tests/vectors
```

They are versioned, and regenerating them is only done deliberately: a
change in the encoder changes the expected output, and that has to be
noticed rather than absorbed.

---

## 9. The legacy format

The project used a `.nam` format before this one. It is specified
separately, in `ALGORITHM.md`, and the legacy implementation is kept in the
repository as the oracle of the golden regression test.

**This library does not read the legacy format.** A `.nam` file is not a
`.bcc` file, and the library rejects it with an error rather than guessing.

The legacy format differs from this one in ways that matter:

- It stored one ASCII character per bit, so the file on disk was eight times
  larger than the bitstream it carried.
- It encoded the gaps between blocks, and derived the width of the gap
  field from the decoder's own position, which made the stream fragile to
  reimplement.
- It had no channel count in the header, and inferred the channel count
  from the width, which misreported every image wider than it is tall.

The new format exists to remove those problems. The two formats are not
interchangeable.
