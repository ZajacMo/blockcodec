cmake_minimum_required(VERSION 3.16)

project(OpenCV-Image-Processing-and-Compression-Tool
        VERSION 1.0
        DESCRIPTION "Qt + OpenCV image compression tool with a reusable block codec library"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# The codec is the reusable part of this project: a self-contained static
# library with no dependency on Qt. Consumers that only need the
# compression algorithm can link `opencv_codec` and ignore the GUI.
option(BUILD_GUI_APP "Build the Qt GUI application" ON)
option(BUILD_TESTING "Build the codec round-trip tests" ON)

find_package(OpenCV REQUIRED)

# ---------------------------------------------------------------------------
# opencv_codec - the compression algorithm, usable without Qt
# ---------------------------------------------------------------------------
add_library(opencv_codec STATIC
    Compress_gray.cpp
    Compress_gray.h
    Bits.cpp
    Bits.h
    Block.cpp
    Block.h
    RBlock.cpp
    RBlock.h
)

target_include_directories(opencv_codec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(opencv_codec PUBLIC ${OpenCV_LIBS})
set_target_properties(opencv_codec PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Qt GUI application
# ---------------------------------------------------------------------------
if(BUILD_GUI_APP)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

    set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(openCV_test WIN32 MACOSX_BUNDLE
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        compresswindow.cpp
        compresswindow.h
        compresswindow.ui
        uncompresswindow.cpp
        uncompresswindow.h
        uncompresswindow.ui
        introducewindow.cpp
        introducewindow.h
        introducewindow.ui
        helpwindow.cpp
        helpwindow.h
        helpwindow.ui
        save.cpp
        save.h
        operatorsignals.cpp
        operatorsignals.h
        resourses.qrc
    )

    target_link_libraries(openCV_test PRIVATE
        opencv_codec
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
    )
endif()

# ---------------------------------------------------------------------------
# Tests - round-trip the codec and validate the reference vectors
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    enable_testing()

    add_executable(codec_selftest tests/codec_selftest.cpp)
    target_link_libraries(codec_selftest PRIVATE opencv_codec)
    target_compile_definitions(codec_selftest PRIVATE
        OPENCV_CODEC_TEST_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")

    add_test(NAME codec_selftest COMMAND codec_selftest)

    # Regenerates the reference vectors. Not part of the test run itself,
    # because the vectors are versioned and must stay stable.
    add_executable(codec_vector_gen tests/codec_vector_gen.cpp)
    target_link_libraries(codec_vector_gen PRIVATE opencv_codec)

    # Sweeps sigma over a sample image and prints the ratio/PSNR tradeoff.
    # Used to produce the numbers quoted in README.md and ALGORITHM.md.
    add_executable(bench_sweep tests/bench_sweep.cpp)
    target_link_libraries(bench_sweep PRIVATE opencv_codec)
endif()
