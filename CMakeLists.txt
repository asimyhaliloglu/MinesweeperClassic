cmake_minimum_required(VERSION 3.20)
project(MinesweeperClassic LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------
# 1. Source Files Collection
# --------------------------------------------------------------------
set(GAME_SOURCES
        main.cpp
        BaseMinesweeper.cpp
        BaseMinesweeper.h
        ClassicMinesweeper.cpp
        ClassicMinesweeper.h
        NoGuessMinesweeper.cpp
        NoGuessMinesweeper.h
        Solver.cpp
        Solver.h
        GameController.cpp
        GameController.h
        Config.h
)

# --------------------------------------------------------------------
# 2. Raylib Dependency (Cross-Platform Automatic Fetch)
# --------------------------------------------------------------------
# Allows local override if RAYLIB_SRC_DIR is explicitly passed;
# otherwise fetches matching Raylib source automatically on any OS.
if (DEFINED RAYLIB_SRC_DIR AND EXISTS "${RAYLIB_SRC_DIR}")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${RAYLIB_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/raylib)
else()
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.5 # Matches the latest stable release
    )
    FetchContent_MakeAvailable(raylib)
endif()

# --------------------------------------------------------------------
# 3. Target Definition Based on Platform
# --------------------------------------------------------------------
if (ANDROID)
    # Android NDK requires a shared library loaded by NativeActivity
    add_library(${PROJECT_NAME} SHARED ${GAME_SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib android log EGL GLESv2 OpenSLES)
else()
    # Desktop (Windows, Linux, macOS) & WebAssembly
    add_executable(${PROJECT_NAME} ${GAME_SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
endif()

# --------------------------------------------------------------------
# 4. Target-Specific Settings
# --------------------------------------------------------------------

# --- A. WebAssembly / Emscripten Configuration ---
if (EMSCRIPTEN)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PRIVATE
            -sUSE_GLFW=3
            -sALLOW_MEMORY_GROWTH=1
            -sEXPORTED_RUNTIME_METHODS=ccall
            --shell-file ${raylib_SOURCE_DIR}/src/minshell.html
    )
endif()

# --- B. Apple (macOS & iOS) Bundle Configuration ---
if (APPLE)
    if (IOS)
        set_target_properties(${PROJECT_NAME} PROPERTIES
                XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" # iPhone & iPad
                MACOSX_BUNDLE YES
        )
    endif()
endif()

# --- C. Threading Support (Linux / macOS) ---
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)