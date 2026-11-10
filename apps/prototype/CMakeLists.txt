# The prototype is kept for reference only. It is not built by default:
# it duplicates the algorithm that now lives in src/core.
add_executable(blockcodec_prototype EXCLUDE_FROM_ALL
    Header.cpp
    Header.h
)
target_include_directories(blockcodec_prototype PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(blockcodec_prototype PRIVATE ${OpenCV_LIBS})
