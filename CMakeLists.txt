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

# GoogleTest drives the unit tests. It is fetched rather than vendored so
# that the dependency stays pinned to a known-good upstream release.
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# blockcodec - the reusable codec library
# ---------------------------------------------------------------------------
add_library(blockcodec STATIC
    src/core/bitstream.cpp
    src/core/block.cpp
    src/core/rblock.cpp
    src/core/codec.cpp
    src/core/metrics.cpp
    src/format.cpp
)
target_include_directories(blockcodec
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(blockcodec PUBLIC ${OpenCV_LIBS})
set_target_properties(blockcodec PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(blockcodec::blockcodec ALIAS blockcodec)

# ---------------------------------------------------------------------------
# legacy_codec - the previous implementation, kept for the GUI and as the
# oracle of the golden regression test. It is removed once the GUI has been
# migrated and the golden test has served its purpose.
# ---------------------------------------------------------------------------
add_library(legacy_codec STATIC
    tests/legacy/Compress_gray.cpp
    tests/legacy/Bits.cpp
    tests/legacy/Block.cpp
    tests/legacy/RBlock.cpp
)

target_include_directories(legacy_codec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy)
target_link_libraries(legacy_codec PUBLIC ${OpenCV_LIBS})
set_target_properties(legacy_codec PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

    target_include_directories(openCV_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(openCV_test PRIVATE
        legacy_codec
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
    target_link_libraries(codec_selftest PRIVATE legacy_codec)
    target_compile_definitions(codec_selftest PRIVATE
        OPENCV_CODEC_TEST_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")

    # The self-test loads its sample images through relative paths, so it has
    # to run with the source directory as the working directory. Ctest would
    # otherwise use the build directory and the samples would be skipped.
    add_test(NAME codec_selftest COMMAND codec_selftest
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # Regenerates the reference vectors. Not part of the test run itself,
    # because the vectors are versioned and must stay stable.
    add_executable(codec_vector_gen tests/codec_vector_gen.cpp)
    target_link_libraries(codec_vector_gen PRIVATE legacy_codec)

    # Sweeps sigma over a sample image and prints the ratio/PSNR tradeoff.
    # Used to produce the numbers quoted in README.md and ALGORITHM.md.
    add_executable(bench_sweep tests/bench_sweep.cpp)
    target_link_libraries(bench_sweep PRIVATE legacy_codec)

    add_executable(test_bitstream tests/unit/test_bitstream.cpp)
    target_link_libraries(test_bitstream PRIVATE blockcodec gtest_main)
    add_test(NAME test_bitstream COMMAND test_bitstream
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(test_format tests/unit/test_format.cpp)
    target_link_libraries(test_format PRIVATE blockcodec gtest_main)
    add_test(NAME test_format COMMAND test_format
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(test_codec tests/unit/test_codec.cpp)
    target_link_libraries(test_codec PRIVATE blockcodec gtest_main)
    add_test(NAME test_codec COMMAND test_codec
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
