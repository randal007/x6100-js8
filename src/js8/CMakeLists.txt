find_package(Threads REQUIRED)

add_library(JS8 STATIC
    resampler.cpp
    render.cpp
    assembler.cpp
    classify.cpp
    receiver.cpp
    wav.cpp
    js8_rx.cpp
)

# js8core needs C++20. Link it PRIVATE so the requirement stops here: the
# app only sees the C header js8_rx.h and keeps its own C++ standard.
target_link_libraries(JS8 PRIVATE js8core PUBLIC Threads::Threads)
target_include_directories(JS8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Test-signal generator for tests and tools/js8_wavgen; not linked into the app.
add_library(JS8TestSignal STATIC testsignal.cpp)
target_link_libraries(JS8TestSignal PRIVATE js8core)
target_include_directories(JS8TestSignal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
