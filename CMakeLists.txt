cmake_minimum_required(VERSION 3.16)

project(blockcodec
        VERSION 0.1.0
        DESCRIPTION "A block based, interpolating image codec with an exactly specified container format"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# The codec is the reusable part of this project: a self-contained static
# library with no dependency on Qt. Consumers that only need the
# compression algorithm can link `blockcodec` and ignore the GUI.
option(BUILD_TESTING "Build the codec round-trip tests" ON)

find_package(OpenCV REQUIRED)

# GoogleTest drives the unit tests. It is fetched rather than vendored so
# that the dependency stays pinned to a known-good upstream release.
#
# INSTALL_GTEST is turned off, because a fetched dependency that installs
# itself would put its headers and libraries into the install prefix of this
# project, which is supposed to contain blockcodec and nothing else.
include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

# CLI11 parses the command line of the blockcodec executable. It is a build
# time dependency of the executable only, so it is not installed.
include(FetchContent)
set(CLI11_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(CLI11)

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
    src/segment.cpp
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
# Installation of the library and of its package description
#
# The installed package is found by a downstream project with
# find_package(blockcodec) and used through the exported target
# blockcodec::blockcodec.
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS blockcodec
        EXPORT blockcodecTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/blockcodec
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT blockcodecTargets
        FILE blockcodecTargets.cmake
        NAMESPACE blockcodec::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blockcodec
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/blockcodecConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blockcodec
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/blockcodecConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/blockcodec
)

# ---------------------------------------------------------------------------
# blockcodec_cli - the command line interface
# ---------------------------------------------------------------------------
add_executable(blockcodec_cli
    apps/cli/main.cpp
    apps/cli/commands.cpp
)
set_target_properties(blockcodec_cli PROPERTIES OUTPUT_NAME blockcodec)
target_include_directories(blockcodec_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/apps)
target_link_libraries(blockcodec_cli PRIVATE blockcodec CLI11::CLI11)

# ---------------------------------------------------------------------------
# blockcodec_bench - the benchmark with a JPEG baseline
# ---------------------------------------------------------------------------
add_executable(blockcodec_bench
    bench/bench_main.cpp
    bench/jpeg_baseline.cpp
)
target_include_directories(blockcodec_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(blockcodec_bench PRIVATE blockcodec)

# ---------------------------------------------------------------------------
# legacy_codec - the previous implementation, kept as the oracle of the
# golden regression test and of the legacy test tools. The GUI no longer
# uses it; it was migrated onto the blockcodec API.
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
# blockcodec_gui - the Qt GUI application
# ---------------------------------------------------------------------------
option(BLOCKCODEC_BUILD_GUI "Build the Qt GUI example" ON)
# The old spelling, kept so that the existing CI workflow keeps working.
# It was declared twice before, and the second declaration won, which
# meant that passing -DBUILD_GUI_APP=OFF still built the GUI.
option(BUILD_GUI_APP "Deprecated alias for BLOCKCODEC_BUILD_GUI" OFF)
if(BUILD_GUI_APP)
    set(BLOCKCODEC_BUILD_GUI ON)
endif()

if(BLOCKCODEC_BUILD_GUI)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

    set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/apps/qt)

    add_executable(blockcodec_gui WIN32 MACOSX_BUNDLE
        apps/qt/main.cpp
        apps/qt/mainwindow.cpp
        apps/qt/mainwindow.h
        apps/qt/mainwindow.ui
        apps/qt/compresswindow.cpp
        apps/qt/compresswindow.h
        apps/qt/compresswindow.ui
        apps/qt/uncompresswindow.cpp
        apps/qt/uncompresswindow.h
        apps/qt/uncompresswindow.ui
        apps/qt/introducewindow.cpp
        apps/qt/introducewindow.h
        apps/qt/introducewindow.ui
        apps/qt/helpwindow.cpp
        apps/qt/helpwindow.h
        apps/qt/helpwindow.ui
        apps/qt/imagedisplay.h
        apps/qt/save.cpp
        apps/qt/save.h
        apps/qt/save.ui
        apps/qt/operatorsignals.cpp
        apps/qt/operatorsignals.h
        apps/qt/resourses.qrc
    )

    target_include_directories(blockcodec_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/apps/qt)
    target_link_libraries(blockcodec_gui PRIVATE
        blockcodec
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

    # Regenerates the reference vectors. Not part of the test run, because
    # the vectors are versioned and must stay stable.
    add_executable(blockcodec_vector_gen tests/vector/blockcodec_vector_gen.cpp)
    target_link_libraries(blockcodec_vector_gen PRIVATE blockcodec)

    # Checks the reference vectors. They are what an independent
    # implementation is verified against, so they must decode to exactly the
    # same pixels on every implementation.
    add_executable(test_vector tests/vector/test_vector.cpp)
    target_link_libraries(test_vector PRIVATE blockcodec gtest_main)
    target_compile_definitions(test_vector PRIVATE
        BLOCKCODEC_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors")
    add_test(NAME test_vector COMMAND test_vector
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # The vectors must not depend on the compiler that produced them, so
    # they are regenerated and compared with the ones in the repository.
    add_executable(test_vector_reproducible tests/vector/test_vector_reproducible.cpp)
    target_link_libraries(test_vector_reproducible PRIVATE blockcodec gtest_main)
    target_compile_definitions(test_vector_reproducible PRIVATE
        BLOCKCODEC_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/vectors"
        BLOCKCODEC_VECTOR_GENERATOR="$<TARGET_FILE:blockcodec_vector_gen>")
    add_dependencies(test_vector_reproducible blockcodec_vector_gen)
    add_test(NAME test_vector_reproducible COMMAND test_vector_reproducible
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # The command line test drives the executable, so it is registered
    # after the target it runs.
    add_executable(test_cli tests/cli/test_cli.cpp)
    add_dependencies(test_cli blockcodec_cli)
    # The path of the executable is passed at run time, because it is only
    # known once the build has finished.
    add_test(NAME test_cli COMMAND test_cli
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(test_cli PROPERTIES
        ENVIRONMENT "BLOCKCODEC_CLI=$<TARGET_FILE:blockcodec_cli>")

    add_executable(test_codec tests/unit/test_codec.cpp)
    target_link_libraries(test_codec PRIVATE blockcodec gtest_main)
    add_test(NAME test_codec COMMAND test_codec
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    add_executable(test_metrics tests/unit/test_metrics.cpp)
    target_link_libraries(test_metrics PRIVATE blockcodec gtest_main)
    add_test(NAME test_metrics COMMAND test_metrics
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # The benchmark test drives the executable, so it is registered after
    # the target it runs. The path of the executable is passed at run time,
    # because it is only known once the build has finished.
    add_executable(test_bench tests/bench/test_bench.cpp)
    add_dependencies(test_bench blockcodec_bench)
    target_link_libraries(test_bench PRIVATE blockcodec gtest_main)
    add_test(NAME test_bench COMMAND test_bench
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(test_bench PROPERTIES
        ENVIRONMENT "BLOCKCODEC_BENCH=$<TARGET_FILE:blockcodec_bench>")

    add_executable(test_robustness tests/unit/test_robustness.cpp)
    target_link_libraries(test_robustness PRIVATE blockcodec gtest_main)
    add_test(NAME test_robustness COMMAND test_robustness
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    # The golden test compares the new codec against the legacy
    # implementation, which is kept as an oracle. The legacy sources include
    # windows.h, so the oracle only exists on Windows and the test is
    # registered only where it does.
    if(TARGET legacy_codec)
        add_executable(test_golden tests/unit/test_golden.cpp)
        target_link_libraries(test_golden PRIVATE blockcodec legacy_codec gtest_main)
        add_test(NAME test_golden COMMAND test_golden
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
endif()
