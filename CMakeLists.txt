cmake_minimum_required(VERSION 3.20)
project(FloodRelief VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(FLOOD_BUILD_GUI   "Build the raylib GUI (downloads raylib on first configure)" ON)
option(FLOOD_BUILD_TESTS "Build the unit tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # helps VS Code IntelliSense

# Put all .exe files in build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Warnings for our own code only (not for raylib)
function(flood_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# relief_core: data structures + relief logic. Never depends on raylib.
# ---------------------------------------------------------------------------
# CONFIGURE_DEPENDS: new .cpp files are picked up automatically on the next build.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/core/*.cpp
    ${CMAKE_SOURCE_DIR}/src/relief/*.cpp
)

if(CORE_SOURCES)
    add_library(relief_core STATIC ${CORE_SOURCES})
    target_include_directories(relief_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
    flood_warnings(relief_core)
else()
    # No .cpp files yet (header-only so far): still usable as a target.
    add_library(relief_core INTERFACE)
    target_include_directories(relief_core INTERFACE ${CMAKE_SOURCE_DIR}/src)
endif()

# ---------------------------------------------------------------------------
# CLI app
# ---------------------------------------------------------------------------
add_executable(relief_cli app/cli_main.cpp)
target_link_libraries(relief_cli PRIVATE relief_core)
flood_warnings(relief_cli)

# ---------------------------------------------------------------------------
# Unit tests  (run with: ctest --test-dir build --output-on-failure)
# ---------------------------------------------------------------------------
if(FLOOD_BUILD_TESTS)
    enable_testing()
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tests/*.cpp)
    add_executable(relief_tests ${TEST_SOURCES})
    target_link_libraries(relief_tests PRIVATE relief_core)
    flood_warnings(relief_tests)
    add_test(NAME unit_tests COMMAND relief_tests)
endif()

# ---------------------------------------------------------------------------
# GUI app (raylib is downloaded and built automatically)
# ---------------------------------------------------------------------------
if(FLOOD_BUILD_GUI)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(raylib)

    file(GLOB_RECURSE GUI_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/gui/*.cpp)
    add_executable(relief_gui app/gui_main.cpp ${GUI_SOURCES})
    target_link_libraries(relief_gui PRIVATE relief_core raylib)
    flood_warnings(relief_gui)

    # Copy data/ and assets/ next to the .exe so relative paths work
    add_custom_command(TARGET relief_gui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data   $<TARGET_FILE_DIR:relief_gui>/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:relief_gui>/assets
    )
endif()

# ---------------------------------------------------------------------------
# Day-1 tile download test (Windows only, built when tools/tile_test.cpp exists)
# ---------------------------------------------------------------------------
if(WIN32 AND EXISTS ${CMAKE_SOURCE_DIR}/tools/tile_test.cpp)
    add_executable(tile_test tools/tile_test.cpp)
    target_link_libraries(tile_test PRIVATE wininet)
endif()
