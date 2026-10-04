# Headless host build of the real JS8 dialog. See README.md.
cmake_minimum_required(VERSION 3.23)
project(js8_ui_harness C CXX)

get_filename_component(X ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
set(D ${CMAKE_CURRENT_SOURCE_DIR}/deps)
set(CMAKE_C_STANDARD 11)

option(HARNESS_SANITIZE "Build with ASan/UBSan" ON)

add_compile_definitions(LV_DRAW_NEON=0 JS8_TEXTS_PATH="${CMAKE_BINARY_DIR}/js8_texts.txt"
                        JS8_LOG_PATH="${CMAKE_BINARY_DIR}/js8call_log.adi"
                        JS8_INBOX_PATH="${CMAKE_BINARY_DIR}/js8_inbox.txt"
                        JS8_HELD_PATH="${CMAKE_BINARY_DIR}/js8_held.txt"
                        JS8_SAVED_PATH="${CMAKE_BINARY_DIR}/js8_saved.txt"
                        JS8_HISTORY_PATH="${CMAKE_BINARY_DIR}/js8_history.db"
                        JS8_MAP_DATA_PATH="${X}/tools/map_data/js8_map.bin"
                        JS8_CTY_PATH="${X}/third-party/cty/cty.dat"
                        JS8_QSO_DB_PATH="${CMAKE_BINARY_DIR}/js8_qso_log.db"
                        HARNESS_DB_DEFAULT="${CMAKE_BINARY_DIR}/params.default.db"
                        HARNESS_DB="${CMAKE_BINARY_DIR}/params.db")
if(HARNESS_SANITIZE)
    add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

# gpsd's gps.h (fetch_deps.sh) before src/: src/gps.h includes <gps.h>.
include_directories(${D}/include/libgps)
include_directories(${X} ${X}/src ${X}/src/qth)
include_directories(SYSTEM ${D}/include)

add_subdirectory(${D}/src/lvgl lvgl)
add_subdirectory(${X}/third-party/js8core js8core)
add_subdirectory(${X}/src/js8 js8)

# R1CBU 1.0's settings (src/cfg, C++ and sqlite) as the radio runs them. It
# needs X6100Control's headers only (types), and a params.db: the one the
# image ships, made from sql/params.sql; each run starts from a copy of it
# (driver.c), so no scenario leaves settings to the next.
add_library(aether_x6100_control INTERFACE)
target_include_directories(aether_x6100_control INTERFACE ${D}/include)
add_subdirectory(${X}/src/cfg cfg)
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/params.default.db
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/params.default.db
    COMMAND sh -c "sqlite3 '${CMAKE_BINARY_DIR}/params.default.db' < params.sql"
    WORKING_DIRECTORY ${X}/sql
    DEPENDS ${X}/sql/params.sql ${X}/sql/digital_modes.csv ${X}/sql/params.csv
    VERBATIM)
add_custom_target(params_db ALL DEPENDS ${CMAKE_BINARY_DIR}/params.default.db)

# The radio loads theme images from /dev/shm; point styles.c at the repo.
file(READ ${X}/src/styles.c STYLES)
string(REPLACE "A:/dev/shm/" "A:${X}/" STYLES "${STYLES}")
file(WRITE ${CMAKE_BINARY_DIR}/styles_host.c "${STYLES}")
# Upstream's scheduler.cpp has "{.id=id, user_data=user_data}": GCC 12 (the
# radio's) takes it, newer host GCC doesn't. Same code, one dot added.
file(READ ${X}/src/scheduler.cpp SCHED)
string(REPLACE "{.id=id, user_data=user_data}" "{.id=id, .user_data=user_data}" SCHED "${SCHED}")
file(WRITE ${CMAKE_BINARY_DIR}/scheduler_host.cpp "${SCHED}")

file(GLOB FONTS ${X}/src/fonts/*.c)

add_executable(js8_ui_harness
    main.cpp stubs.c driver.c
    ${X}/src/dialog_js8.c
    ${X}/src/js8_wf.c
    ${X}/src/textarea_window.c
    ${X}/src/kbd_rollover.c
    ${X}/src/qth/qth.c
    ${X}/src/dialog.c
    ${X}/src/lock_manager.c
    ${CMAKE_BINARY_DIR}/scheduler_host.cpp
    ${X}/src/widgets/lv_waterfall.c
    ${X}/src/widgets/lv_finder.c
    ${CMAKE_BINARY_DIR}/styles_host.c
    ${FONTS}
)
set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-std=c++20")
# Upstream relies on GCC 12 pulling <cstdlib> in transitively; newer host GCC does not.
set_source_files_properties(${CMAKE_BINARY_DIR}/scheduler_host.cpp PROPERTIES COMPILE_OPTIONS "-include;cstdlib")
target_link_libraries(js8_ui_harness PRIVATE lvgl cfg JS8 JS8TestSignal js8core ${D}/lib/libliquid.a sqlite3 m)
add_dependencies(js8_ui_harness params_db)
