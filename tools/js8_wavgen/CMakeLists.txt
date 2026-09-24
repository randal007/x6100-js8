cmake_minimum_required(VERSION 3.20)
project(js8_wavgen CXX)

get_filename_component(X ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
add_subdirectory(${X}/third-party/js8core js8core)
add_subdirectory(${X}/src/js8 js8)

add_executable(js8_wavgen js8_wavgen.cpp)
target_link_libraries(js8_wavgen PRIVATE JS8 JS8TestSignal)
