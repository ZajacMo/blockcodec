# blockcodec

*[English](README.md)*

`blockcodec` 是一个小巧、自包含的 C++17 库，实现了一种 **有损的、基于分块插值的图像编解码器**。
它把一幅图像编码为一组矩形块，每块由四个角点采样描述，解码时通过双线性插值重建。
它写出的容器格式 `.bcc` 有着足够精确的规范，独立实现者用另一种语言也能写出逐字节相同的输出。

该库只依赖 OpenCV 提供的图像容器，不依赖其他任何东西。它不依赖 Qt，也不依赖随附的图形界面程序。

**它不是什么。** `blockcodec` **不是**通用图像编解码器。它不是 JPEG、WebP 或 AVIF 的竞争对手，
也不打算成为它们的替代品。在自然照片上，它在**相同重建质量下写出的文件比 JPEG 更大**
——下面的实测数据表明，在每一张 512×512 与 645×1274 的样例图上它都落败，而且差距通常相当可观。
它的价值在于它是一个 **规范精确、可复现、可测试** 的分块编解码器：完整的格式规范、参考向量、
与原始实现对拍的黄金回归测试，以及一个与基线（而不是与自己）对比的基准测试。

---

## 目录

- [算法](#算法)
- [基准测试](#基准测试)
- [构建](#构建)
- [安装](#安装)
- [使用该库](#使用该库)
- [命令行](#命令行)
- [彩色支持](#彩色支持)
- [容器格式](#容器格式)
- [测试](#测试)
- [仓库结构](#仓库结构)
- [限制](#限制)
- [许可证](#许可证)

---

## 算法

编码器自上而下逐行扫描图像，行内自左向右扫描。在每个尚未被覆盖的像素处，它开始生长一个矩形块：
先沿对角线，再沿列，最后沿行；一旦候选块通不过块检验就停止。
一个候选块被接受的条件是：其内部 **每一个** 像素与四角采样的双线性插值之差都不超过 `sigma`。
写进文件的只有那四个角点采样；内部区域在解码时由插值重建。
无法被任何块覆盖的像素会作为孤立的单像素块存储。

因此 `sigma` 同时就是画质旋钮和体积旋钮。`sigma` 越小，只接受近乎均匀的块，
产出的文件大而 PSNR 高；`sigma` 越大，可以接受更粗的块，产出的文件小而 PSNR 低。
对彩色图像，检验取各通道偏差的 **最大值**，因此单通道图像恰好退化为灰度规则。

位级布局、插值算术，以及解码器必须执行的全部检查，都写在
**[docs/ALGORITHM_BCC.md](docs/ALGORITHM_BCC.md)** 中。

---

## 基准测试

下面的数字来自 `blockcodec_bench` 在随附的 `images/` 目录上的 **一次实际运行**：

```
blockcodec_bench --images images --sigmas 1,5,10,20,30,50 --baseline jpeg
```

### 有意义的对比方式

把两份互不相干的列表并排放在一起，对读者毫无意义：只有当 JPEG 文件与 blockcodec 文件
承载 **相同重建结果** 时，二者才可比。因此基准测试会先把 JPEG 质量对齐到 blockcodec 质量。
对每一个 `sigma`，它找出 **仍能达到 blockcodec 所达 PSNR 的最低 JPEG 质量**，
并给出该质量下 JPEG 文件的大小。这正是两条曲线相交的地方。

`bc_bytes` 是 `.bcc` 文件在磁盘上的大小，`jpeg_bytes` 是编码后 JPEG 的大小。
`smaller` 一列给出在同等质量下文件更小的那个编解码器。

**`lena512.bmp` —— 512×512，灰度**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | 更小者 |
|--------:|---------:|--------:|-------:|-----------:|----------:|--------|
| 1 | 525 380 | 59.23 dB | 100 | 162 310 | 58.47 dB | *无胜者* |
| 5 | 269 912 | 42.29 dB | 94 | 81 621 | 42.93 dB | jpeg |
| 10 | 126 194 | 36.39 dB | 60 | 24 107 | 36.45 dB | jpeg |
| 20 | 57 803 | 30.38 dB | 10 | 7 998 | 30.41 dB | jpeg |
| 30 | 35 474 | 26.69 dB | 5 | 5 661 | 27.33 dB | jpeg |
| 50 | 16 196 | 22.36 dB | 1 | 4 376 | 24.22 dB | jpeg |

**`baboonzyp512.bmp` —— 512×512，高频纹理**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | 更小者 |
|--------:|---------:|--------:|-------:|-----------:|----------:|--------|
| 1 | 563 128 | 67.26 dB | 100 | 246 705 | 58.40 dB | *无胜者* |
| 5 | 507 249 | 46.81 dB | 98 | 201 524 | 49.76 dB | jpeg |
| 10 | 408 411 | 38.48 dB | 92 | 124 485 | 38.72 dB | jpeg |
| 20 | 270 822 | 31.26 dB | 75 | 68 913 | 31.34 dB | jpeg |
| 30 | 191 211 | 27.30 dB | 39 | 39 108 | 27.31 dB | jpeg |
| 50 | 104 307 | 22.56 dB | 8 | 12 508 | 22.83 dB | jpeg |

**`decho256.bmp` —— 256×256，灰度**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | 更小者 |
|--------:|---------:|--------:|-------:|-----------:|----------:|--------|
| 1 | 79 364 | 54.76 dB | 99 | 31 251 | 55.36 dB | jpeg |
| 5 | 43 676 | 43.06 dB | 89 | 12 755 | 43.19 dB | jpeg |
| 10 | 29 788 | 37.12 dB | 59 | 6 645 | 37.13 dB | jpeg |
| 20 | 18 052 | 31.44 dB | 16 | 3 268 | 31.45 dB | jpeg |
| 30 | 12 228 | 28.54 dB | 9 | 2 416 | 28.93 dB | jpeg |
| 50 | 6 820 | 24.58 dB | 4 | 1 632 | 25.10 dB | jpeg |

**`lenaColorful.bmp` —— 512×512，三通道**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | 更小者 |
|--------:|---------:|--------:|-------:|-----------:|----------:|--------|
| 1 | 1 076 975 | 76.85 dB | 100 | 229 542 | 37.49 dB | *无胜者* |
| 5 | 923 162 | 46.39 dB | 100 | 229 542 | 37.49 dB | *无胜者* |
| 10 | 517 081 | 38.29 dB | 100 | 229 542 | 37.49 dB | *无胜者* |
| 20 | 184 177 | 32.27 dB | 57 | 26 822 | 32.29 dB | jpeg |
| 30 | 102 287 | 28.61 dB | 14 | 11 389 | 28.76 dB | jpeg |
| 50 | 46 484 | 24.13 dB | 5 | 7 055 | 24.29 dB | jpeg |

**`PrintScreen.png` —— 645×1274，四通道**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | 更小者 |
|--------:|---------:|--------:|-------:|-----------:|----------:|--------|
| 1 | 2 029 003 | 58.09 dB | 100 | 523 861 | 53.03 dB | *无胜者* |
| 5 | 1 241 516 | 43.72 dB | 95 | 319 351 | 44.22 dB | jpeg |
| 10 | 762 974 | 38.00 dB | 85 | 187 368 | 38.05 dB | jpeg |
| 20 | 499 308 | 32.32 dB | 53 | 104 395 | 32.39 dB | jpeg |
| 30 | 389 394 | 28.46 dB | 22 | 64 965 | 28.62 dB | jpeg |
| 50 | 282 816 | 23.35 dB | 5 | 31 052 | 23.40 dB | jpeg |

**`100.bmp` —— 16×16，灰度**

| `sigma` | bc_bytes | bc_psnr | jpeg_q | jpeg_bytes | jpeg_psnr | 更小者 |
|--------:|---------:|--------:|-------:|-----------:|----------:|--------|
| 1 | 404 | 65.22 dB | 100 | 547 | 59.20 dB | *无胜者* |
| 5 | 332 | 44.91 dB | 97 | 499 | 46.71 dB | **blockcodec** |
| 10 | 266 | 37.12 dB | 87 | 414 | 37.24 dB | **blockcodec** |
| 20 | 194 | 30.02 dB | 23 | 358 | 30.16 dB | **blockcodec** |
| 30 | 104 | 25.37 dB | 10 | 345 | 25.43 dB | **blockcodec** |
| 50 | 56 | 21.40 dB | 4 | 338 | 22.44 dB | **blockcodec** |

### 这张表说明了什么

- **在每一张自然图像上、在每一个对齐质量上，JPEG 的文件都更小。**
  差距很大 —— 在上面的自然图像样例中，blockcodec 的文件是同等质量下 JPEG 文件的
  2.5× 到 9.1×，通常在 3× 到 7× 之间。一个块需要四个角点采样来描述，代价高昂；
  而 JPEG 的变换、量化和熵编码在照片上确实是用同样比特数的更优方式。
- **在 16×16 样例上，blockcodec 胜出。** 原因并不是它压缩得更好，
  而是它的容器有一个固定的 20 字节头部，而同一张图的 JPEG 光是头部和表格就要 338–547 字节。
  在 `sigma = 50` 时 blockcodec 写出 56 字节，JPEG 是 338 字节。
  这是头部体积带来的效应，而不是压缩率上的优势，并且随着图像尺寸达到人们真正会保存的尺度就消失了。
- **标注 `*无胜者*` 的行不是平局。** 这些行意味着即便最高的 JPEG 质量也达不到 blockcodec 的 PSNR，
  于是两个文件并不处在同一质量上，它们的大小也就无法说明取舍。
  基准测试把这类行打印为 `short`，并且不为它们指定胜者。
  最明显的例子是 `lenaColorful.bmp` 在 `sigma = 1` 时：blockcodec 达到 76.85 dB，
  而质量 100 的 JPEG 只有 37.49 dB —— 因为三通道双线性块模型几乎能精确复现平坦色块，
  而 JPEG 的 8×8 DCT 块做不到。
- **合适的 `sigma` 取决于图像内容。** 平滑的 `lena512` 用 126 kB 达到 36.4 dB，
  而纹理复杂的 `baboonzyp512` 需要 408 kB 才能达到 38.5 dB，
  因为纹理图像几乎找不到大块的均匀矩形。

### 复现这张表

```bash
export PATH="/path/to/opencv/build/x64/vc16/bin:$PATH"   # Windows
./build/Release/blockcodec_bench.exe \
    --images images --sigmas 1,5,10,20,30,50 --baseline jpeg
```

加上 `--csv bench.csv` 可以把测量结果写入三个 CSV 文件。

---

## 构建

### 环境要求

| 依赖 | 版本 | 说明 |
|------|------|------|
| C++ 编译器 | C++17 | MSVC 2019+、GCC 8+、Clang 8+ |
| CMake | 3.16 及以上 | |
| OpenCV | 4.x | 已针对 4.10.0 验证。库本身只需 `core` 与 `imgproc`；命令行工具和基准测试还需要 `imgcodecs` |
| Qt | 5.x 或 6.x | **可选。** 只有 GUI 示例程序需要它 |

GoogleTest 与 CLI11 由 CMake 在配置阶段自动拉取，无需自行安装。

### 只构建库

```bash
cmake -S . -B build -DBLOCKCODEC_BUILD_GUI=OFF -DBUILD_TESTING=OFF
cmake --build build --config Release
```

### 构建全部目标（含 GUI 示例）

```bash
cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="/path/to/Qt/6.6.2/msvc2019_64" \
      -DOpenCV_DIR="/path/to/opencv/build"
cmake --build build --config Release
```

Windows + Visual Studio：

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 \
      -DCMAKE_PREFIX_PATH="C:/Qt/6.6.2/msvc2019_64" \
      -DOpenCV_DIR="C:/Program Files/opencv/build"
cmake --build build --config Release
```

OpenCV 与 Qt 都是动态链接的，运行产物前需把它们的 `bin` 目录加入 `PATH`：

```
C:\Program Files\opencv\build\x64\vc16\bin
C:\Qt\6.6.2\msvc2019_64\bin
```

### 构建选项

| 选项 | 默认值 | 含义 |
|------|--------|------|
| `BLOCKCODEC_BUILD_GUI` | `ON` | 构建 Qt GUI 示例（`blockcodec_gui`） |
| `BUILD_TESTING` | `ON` | 构建单元测试并注册到 CTest |

---

## 安装

```bash
cmake --install build --config Release --prefix /usr/local
```

安装内容：

```
<prefix>/include/blockcodec/*.hpp          公共头文件
<prefix>/lib/blockcodec.lib                 静态库
<prefix>/lib/cmake/blockcodec/blockcodecConfig.cmake
<prefix>/lib/cmake/blockcodec/blockcodecConfigVersion.cmake
<prefix>/lib/cmake/blockcodec/blockcodecTargets.cmake
```

### 在下游项目中使用已安装的包

```cmake
find_package(blockcodec REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE blockcodec::blockcodec)
```

`find_package` 会自行引入 OpenCV，因为 `blockcodec` 的公共头文件包含了 `<opencv2/core.hpp>`。
如果 OpenCV 不在默认搜索路径上，请用 `CMAKE_PREFIX_PATH` 指向它：

```bash
cmake -S . -B build \
      -DCMAKE_PREFIX_PATH="/usr/local;/path/to/opencv/build"
```

### 直接使用源码树

如果不想安装任何东西：

```cmake
add_subdirectory(path/to/blockcodec)
target_link_libraries(my_app PRIVATE blockcodec::blockcodec)
```

---

## 使用该库

公共 API 很小，而且是无状态的：每次调用彼此独立，测量结果由返回值给出，而非保存在对象里。

```cpp
#include <blockcodec/blockcodec.hpp>

#include <opencv2/imgcodecs.hpp>
#include <fstream>

// 编码
cv::Mat source = cv::imread("input.png", cv::IMREAD_UNCHANGED);

blockcodec::Codec codec({/*sigma=*/10});
blockcodec::CompressResult result;
std::vector<std::uint8_t> container = codec.compress(source, &result);

std::printf("%zu bytes, %.4f bpp, %.2f dB\n",
            result.byte_size, result.bpp, result.psnr);

std::ofstream out("output.bcc", std::ios::binary);
out.write(reinterpret_cast<const char*>(container.data()),
          static_cast<std::streamsize>(container.size()));

// 解码
blockcodec::Codec decoder;
cv::Mat reconstructed = decoder.decompress("output.bcc");
```

### API 一览

| 符号 | 作用 |
|------|------|
| `blockcodec::Codec` | 编码器与解码器。用 `CompressOptions` 构造。 |
| `Codec::compress(mat, result)` | 编码图像，返回 `.bcc` 容器。空图或非 8 位图会抛出 `std::runtime_error`。 |
| `Codec::decompress(container, result)` | 解码容器，返回图像。输入不合法时抛出 `std::runtime_error`。 |
| `Codec::decompress(path, result)` | 同上，但直接从文件读取容器。 |
| `blockcodec::CompressOptions` | 保存 `sigma`，即块检验的容限。 |
| `blockcodec::CompressResult` | 测量结果：`bit_size`、`byte_size`、`bpp`、`cr`、`psnr`、`compress_ms`、`decompress_ms`、`block_count`、`blocks`。 |
| `blockcodec::psnr(a, b)` | 峰值信噪比（dB）。完全相同的两幅图返回无穷大。 |
| `blockcodec::bpp(bits, rows, cols)` | 比特流的每像素比特数。 |
| `blockcodec::compression_ratio(...)` | 原始比特数除以编码后比特数。 |
| `blockcodec::read_container(data, payload)` | 解析容器头并分离出负载。容器不合法时抛出异常。 |
| `blockcodec::write_container(header, payload)` | 由头部与负载构造容器。 |
| `blockcodec::ContainerHeader` | 头部字段：`rows`、`cols`、`block_count`、`channels`、`depth_bits`、`colour`。 |
| `blockcodec::BitWriter` / `BitReader` | 稠密位流。每字节高位在前，每字节存八位。 |
| `blockcodec::segmentation_overlay(image, blocks)` | 在图像副本上绘制块的边界。 |

关于两个度量，有两点容易搞错：

- `CompressResult::bpp` 与 `CompressResult::cr` 依据的是 **比特流** 而不是容器文件。
  容器额外包含了 20 字节的头部和最后一个字节的填充位。基准测试会单独报告文件大小，
  因为那才是用户关心的数字。
- `CompressResult::psnr` 是与原图相比测得的，因此编码器会把刚写出的内容再解码一次来计算它。
  这次校验解码的时间已计入 `compress_ms`。

---

## 命令行

`blockcodec` 可执行文件有三个子命令。

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

退出码在命令成功时为 0，在运行时失败时为 1（文件读不出来、容器格式不合法），
在命令行本身有误时为 2。`--help` 的退出码是 0，因为请求帮助是一次成功的完成。

`info` 命令按 **宽 × 高** 的顺序打印尺寸，与图像格式本身的顺序一致。

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

`-s` 是容限，默认值为 `10`。`-q` 可以抑制这段报告输出。

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

`--compare` 是可选的；它会给出与参考图之间的 PSNR，并在形状不匹配时报错。

---

## 彩色支持

**这个项目的旧版本声称支持彩色，实际上并没有做到。** 旧格式从图像宽度推断通道数，
于是每一张宽大于高的图像都被误判，图形界面里的彩色选项产生的结果也没有实际意义。

当前的库是真正支持彩色的：

- 通道数 **显式** 存放在容器头中，因此解码出的图像通道数是正确的。
- 灰度（1 通道）、常见彩色布局（3 通道）以及带 alpha 通道的图像（4 通道）都能正确往返。
- 块检验比较各通道偏差的 **最大值**，因此单通道恰好退化为灰度规则，
  编解码器不会接受某个通道很好而另一个通道很差的块。

能暴露旧有推断错误的情形，是一张 **宽大于高** 且只有单通道的图像。
`images/landscape_grey.bmp` 宽 256 像素、高 128 像素，黄金回归测试把它与旧实现
放在一起比较，因此证明了两者在它上面结论一致。

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

四通道图像同样能正确往返：

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

只支持 8 位图像。头部虽然预留了 `depth_bits` 字段，但 16 位和 32 位采样是保留值，会被拒绝。

---

## 容器格式

一个 `.bcc` 容器由固定的 20 字节头部和一个打包后的负载组成：

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

负载中的每个块依次写出其左上角位置、宽度减一、高度减一，以及角点采样 ——
每通道四个，顺序为左上、右上、左下、右下。
字段宽度由头部推导而来，而不是依赖写入方的当前位置，
因此负载是自定界的，解码器永远不会读到填充位。

完整规范（包括插值算术、字节与位序，以及解码器必须拒绝的全部情形）见
**[docs/ALGORITHM_BCC.md](docs/ALGORITHM_BCC.md)**。

这个项目此前使用过 `.nam` 格式，它的规范单独写在 **[ALGORITHM.md](ALGORITHM.md)** 中，
描述的是 **旧版** 格式。**本库不读取旧版格式**；遇到 `.nam` 文件会直接报错，
而不是去猜测它。两个格式不可互换；两者的差异见
[`.bcc` 规范第 9 节](docs/ALGORITHM_BCC.md#9-the-legacy-format)。

---

## 测试

```bash
ctest --test-dir build --output-on-failure              # Linux / macOS
ctest --test-dir build -C Release --output-on-failure    # Windows
```

| 测试 | 覆盖内容 |
|------|----------|
| `codec_selftest` | 旧版自检：往返测试、参考向量、错误处理 |
| `test_bitstream` | 稠密位流的写入与读取 |
| `test_format` | 容器头：写入、解析，以及全部拒绝规则 |
| `test_vector` | 参考向量解码结果与参考图像完全一致 |
| `test_cli` | 驱动 `blockcodec` 可执行文件 |
| `test_codec` | 编解码器：往返测试、彩色、块模型 |
| `test_metrics` | `psnr`、`bpp` 与压缩比 |
| `test_bench` | 驱动 `blockcodec_bench` |
| `test_robustness` | 非法输入被拒绝而不是被信任 |
| `test_golden` | 以旧版实现为基准，与新编解码器对拍 |

`tests/vectors/` 中的参考向量是独立实现的验收标准：
解码 `gradient_16x16.bcc` 的结果必须与 `gradient_16x16_reference.bmp`
**逐像素完全一致**。彩色的一对 `gradient_16x16_colour.bcc` 同理。
度量指标不是验收标准 —— 验证工具与被验证的实现可能用同样的方式计算它们，
于是在双方都错的情况下也彼此一致。

---

## 仓库结构

```
.
├── CMakeLists.txt          构建脚本
├── cmake/
│   └── blockcodecConfig.cmake.in   安装后的包描述文件
├── include/blockcodec/      公共头文件
│   ├── blockcodec.hpp       总头文件
│   ├── bitstream.hpp        稠密位流
│   ├── codec.hpp            编码器与解码器
│   ├── format.hpp           .bcc 容器
│   ├── metrics.hpp          psnr、bpp、压缩比
│   └── segment.hpp          分块叠加显示
├── src/
│   ├── core/                位流、块模型、编解码器
│   ├── format.cpp           容器
│   └── segment.cpp          叠加显示
├── apps/
│   ├── cli/                 blockcodec 可执行文件
│   ├── qt/                  GUI 示例（blockcodec_gui）
│   └── prototype/           原始原型，保留供参考
├── bench/                   blockcodec_bench 及其 JPEG 基线
├── tests/
│   ├── unit/ cli/ bench/    各测试套件
│   ├── vector/              参考向量生成器
│   ├── vectors/             版本化的参考向量
│   └── legacy/              旧版实现，作为对拍基准
├── legacy/vs-console/       之前的 Visual Studio 实现
├── docs/
│   └── ALGORITHM_BCC.md     .bcc 格式规范
├── ALGORITHM.md             旧版 .nam 格式规范
├── images/                  示例图像
└── CHANGELOG.md             变更历史
```

---

## 限制

- **在自然图像上输给 JPEG。** 这是最主要的一条，而且是实测出来的而非假设的。
  参见[基准测试](#基准测试)。
- **只支持 8 位图像。** 头部为 16 位和 32 位每采样预留了字段，但解码器会拒绝它们。
- **编码器不快。** 它是一个直接了当的搜索，不是最优划分。
  一张 512×512 图像要几十毫秒，一张 645×1274 的截图要一秒多。它没有并行化。
- **块搜索是贪心的，且依赖顺序。** 扫描顺序决定了块的形状，进而决定了文件大小。
  换一个顺序会得到另一个同样合法的流。
- **容器没有校验和。** 被破坏的文件会被头部检查和填充位检查发现，
  但那些仍能被解码的负载损坏则检测不出来。
- **各项指标并不构成对编解码器的认证。** 正如格式规范所说，
  每像素比特数与压缩比是测量值，而不是验收标准。

---

## 许可证

基于 [MIT 许可证](LICENSE) 发布。
