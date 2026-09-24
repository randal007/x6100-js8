find_package(Threads REQUIRED)

add_library(JS8 STATIC
    resampler.cpp
    render.cpp
    assembler.cpp
    classify.cpp
    receiver.cpp
    js8_rx.cpp
)

# js8core requires C++20; it propagates through its PUBLIC compile feature.
target_link_libraries(JS8 PUBLIC js8core Threads::Threads)
target_include_directories(JS8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
