# 图像处理与压缩工具 (OpenCV Image Processing and Compression Tool)

*[English](README.md)*

一个基于 **Qt** 和 **OpenCV** 的 Windows 桌面图像压缩与解压工具。它实现了一套自定义的有损分块压缩算法，
并且将压缩算法同时封装为一个 **Qt 无关的静态库**，便于在其他项目中复用。

压缩格式（`.nam`）与算法本身的详细规范见 **[ALGORITHM.md](ALGORITHM.md)**，
其中还包含一份参考测试向量，独立实现可以用它来验证自己的解码器。

> **说明：** 本文档为英文 README 的中文摘要。完整的构建说明、算法说明和已知限制请以
> [README.md](README.md) 与 [ALGORITHM.md](ALGORITHM.md) 为准。

---

## 功能特点

- **主界面导航**：提供清晰的功能入口，可快速切换到不同的操作界面
- **图像压缩**：支持灰度图像压缩，误差容限（sigma）可调
- **图像解压**：支持解压此前写出的 `.nam` 文件
- **实时质量指标**：每次运行后显示压缩比、耗时，以及原图与重建图之间的 PSNR
- **项目介绍与帮助窗口**
- **可复用编解码库**：`opencv_codec` 是不依赖 Qt 的静态库
- **跨平台**：编解码核心可在 Linux、macOS 与 Windows 上构建

---

## 项目结构

```
.
├── CMakeLists.txt        # CMake 构建：GUI 程序 + 编解码库 + 测试
├── openCV_test.pro       # Qt Creator 构建（保留）
├── ALGORITHM.md          # 格式规范与算法参考
│
├── Compress_gray.h/.cpp  # 核心编解码：压缩、解压、PSNR
├── Bits.h/.cpp           # 位流读写
├── Block.h/.cpp          # 图像块基类
├── RBlock.h/.cpp         # 可重建块与块有效性模型
│
├── mainwindow.*          # 主导航窗口
├── compresswindow.*      # 压缩窗口
├── uncompresswindow.*    # 解压窗口
├── introducewindow.*     # 项目简介窗口
├── helpwindow.*          # 帮助窗口
│
├── tests/                # 自检程序、参考向量与基准测试
├── images/               # 测试与基准所用的示例图像
└── Resources/            # 应用图标
```

---

## 构建

### 环境要求

| 依赖 | 版本 | 说明 |
|------|------|------|
| Qt | 5.x 或 6.x | 只需 `Core`、`Gui`、`Widgets` 模块 |
| OpenCV | 4.x | 已针对 4.10.0 验证 |
| CMake | 3.16 及以上 | 仅 CMake 构建需要 |
| 编译器 | C++17 | MSVC 2019+、GCC 8+、Clang 8+ |

### 方式一 —— CMake（推荐）

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

运行前需把 OpenCV 与 Qt 的 `bin` 目录加入 `PATH`：

```
C:\Program Files\opencv\build\x64\vc16\bin
C:\Qt\6.6.2\msvc2019_64\bin
```

### 方式二 —— Qt Creator（`.pro`）

原有的 `openCV_test.pro` 已保留，可直接在 Qt Creator 中打开：

1. **File → Open File or Project**，选择 `openCV_test.pro`
2. 选择与本机 Qt 和编译器匹配的 Kit，例如 *Desktop Qt 6.6.2 MSVC2019 64-bit*
3. `.pro` 中默认指向 `C:/Program Files/opencv/build`，若路径不同请修改
   `INCLUDEPATH`、`DEPENDPATH` 与 `LIBS`

两种构建方式使用同一份源代码，产物一致。

---

## 使用说明

### 图像压缩

1. 主界面点击 **Compress**
2. 点击 **Choose image** 选择图像（`*.png`、`*.jpg`、`*.bmp`）
3. 选择灰度或彩色模式
4. 用数值框调整**误差容限**（sigma）：数值越小画质越好、文件越大
5. 点击 **Start processing**，窗口会显示原图大小、压缩比、耗时与压缩后大小
6. 点击 **Decompress now** 可预览重建效果及其 PSNR
7. 点击 **Save** 将压缩流保存为 `.nam` 文件

### 图像解压

1. 主界面点击 **Decompress**
2. 点击 **Choose file** 选择 `.nam` 文件
3. 点击 **Decompress** 解码，再点击 **Save** 保存结果

---

## 压缩算法

编解码器是面向 **8 位灰度图像** 的有损分块编解码器。编码器扫描图像并将其划分为矩形块，
每个块由四个角点颜色描述，内部区域通过双线性插值重建；无法被任何块覆盖的像素则作为孤立像素原样存储。

参数 `sigma` 表示像素与覆盖它的块之间允许的最大偏差（灰度级），因此它同时决定了画质与文件大小。

在本仓库自带的 512x512 示例上实测（由 `bench_sweep` 产生）：

| sigma | lena512 比特数 | lena512 文件/原始 | lena512 PSNR | baboon 比特数 | baboon 文件/原始 | baboon PSNR |
|-------|----------------|-------------------|--------------|-------------|------------------|-------------|
| 1 | 3 282 134 | 12.52x | 59.23 dB | 3 512 520 | 13.40x | 67.26 dB |
| 5 | 1 699 187 | 6.48x | 42.29 dB | 3 161 627 | 12.06x | 46.81 dB |
| 10 | 799 631 | 3.05x | 36.39 dB | 2 543 844 | 9.70x | 38.48 dB |
| 20 | 368 150 | 1.40x | 30.38 dB | 1 689 296 | 6.44x | 31.26 dB |
| 30 | 226 626 | 0.86x | 26.69 dB | 1 194 116 | 4.56x | 27.30 dB |
| 50 | 103 842 | 0.40x | 22.36 dB | 652 517 | 2.49x | 22.56 dB |

"文件/原始"是 `.nam` 文件在磁盘上的大小除以原始像素数据的大小，
因此**小于 1.0 表示文件比原图更小**。PSNR 为原图与重建图之间的信噪比。

三点值得注意：

- **文本容器带来八倍开销。** 比特流是编解码器实际产生的数据，
  但 `.nam` 每个比特用一个 ASCII 字符存储，因此磁盘上的文件比比特流大八倍。
- **sigma 很小时文件会大很多。** 在 `sigma = 1` 时，每个块自身的描述开销
  远高于它所替代的像素，文件大小是原始像素的 12.5 倍。
  直到 `sigma ≈ 30` 时该格式才比原始图像更小，而且画质已有明显损失。
- **合适的 sigma 取决于图像内容。** 纹理平滑的 `lena512` 在同等容限下比纹理复杂的
  `baboonzyp512` 压缩得好得多，因为后者几乎找不到大块的均匀矩形。

> **关于 `getRate()` 的说明。** 该方法沿用自原始实现，返回的是**每像素字节数**，
> 而非压缩比，且基于比特流长度而非磁盘文件大小。
> 上表使用的是磁盘上的实际文件大小，这才是用户关心的数字。
> 详见 [ALGORITHM.md §9.8](ALGORITHM.md#98-getrate-reports-bytes-per-pixel-not-a-compression-ratio)。

位流的精确格式见 **[ALGORITHM.md](ALGORITHM.md)**。

---

## 测试与参考向量

```bash
./build/Release/codec_selftest.exe
```

自检程序包含三类检查：

1. **往返测试** —— 压缩再解压内置示例图像，校验尺寸、PSNR 下限与压缩比
2. **参考向量** —— 解码 `tests/vectors/gradient_16x16.nam`，
   断言结果与 `gradient_16x16_reference.bmp` **逐像素完全一致**；
   独立实现应以此为基准
3. **错误处理** —— 断言文件缺失与空图会被正确报错而非崩溃

重新生成参考向量：

```bash
./build/Release/codec_vector_gen.exe tests/vectors
```

复现压缩比/PSNR 表格：

```bash
./build/Release/bench_sweep.exe images/lena512.bmp
```

---

## 已知限制

- **仅支持灰度图。** 分块模型假定空间邻接关系，跨颜色通道没有意义。
  界面虽提供彩色选项，但彩色图会被当作交错采样处理，结果并不具有实际意义。
- **压缩格式是文本。** `.nam` 文件每个比特用一个 ASCII `'0'`/`'1'` 字符表示，
  比打包后的二进制流大约大八倍。这样设计是为了便于查看与移植。
- **没有版本号与校验和。** 文件被截断时会解码出残缺图像而不报错。
- **小块图像的开销占比很高。** 16x16 样例的压缩结果比原始像素还大，
  因此该编解码器只在数百像素以上的图像上才有意义，
  且需要以画质明显下降为代价。
- **`getRate()` 返回的是每像素字节数**，而非压缩比，
  且基于比特流长度而非磁盘文件大小。上表使用的是磁盘上的实际文件大小。
- **`isColorful` 由图像尺寸推断**，并非写入文件。

---

## 许可证

基于 [MIT 许可证](LICENSE) 发布。
