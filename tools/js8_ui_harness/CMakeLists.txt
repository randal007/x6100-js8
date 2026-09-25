# Headless host build of the real JS8 dialog. See README.md.
cmake_minimum_required(VERSION 3.23)
project(js8_ui_harness C CXX)

get_filename_component(X ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
set(D ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(CMAKE_C_STANDARD 11)

option(HARNESS_SANITIZE "Build with ASan/UBSan" ON)

add_compile_definitions(LV_DRAW_NEON=0 JS8_TEXTS_PATH="${CMAKE_BINARY_DIR}/js8_texts.txt"
                        JS8_LOG_PATH="${CMAKE_BINARY_DIR}/js8call_log.adi")
if(HARNESS_SANITIZE)
    add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

include_directories(${X} ${X}/src ${X}/src/qth)
include_directories(SYSTEM ${D}/include)

add_subdirectory(${D}/src/lvgl lvgl)
add_subdirectory(${X}/third-party/js8core js8core)
add_subdirectory(${X}/src/js8 js8)

# The radio loads theme images from /dev/shm; point styles.c at the repo.
file(READ ${X}/src/styles.c STYLES)
string(REPLACE "A:/dev/shm/" "A:${X}/" STYLES "${STYLES}")
file(WRITE ${CMAKE_BINARY_DIR}/styles_host.c "${STYLES}")

file(GLOB FONTS ${X}/src/fonts/*.c)

add_executable(js8_ui_harness
    main.cpp stubs.c driver.c
    ${X}/src/dialog_js8.c
    ${X}/src/textarea_window.c
    ${X}/src/qth/qth.c
    ${X}/src/dialog.c
    ${X}/src/scheduler.cpp
    ${X}/src/widgets/lv_waterfall.c
    ${X}/src/widgets/lv_finder.c
    ${CMAKE_BINARY_DIR}/styles_host.c
    ${FONTS}
)
set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-std=c++20")
# Upstream relies on GCC 12 pulling <cstdlib> in transitively; newer host GCC does not.
set_source_files_properties(${X}/src/scheduler.cpp PROPERTIES COMPILE_OPTIONS "-include;cstdlib")
target_link_libraries(js8_ui_harness PRIVATE lvgl JS8 js8core ${D}/lib/libliquid.a m)
