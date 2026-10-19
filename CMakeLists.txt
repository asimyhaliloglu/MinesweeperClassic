cmake_minimum_required(VERSION 3.20)
project(MinesweeperClassic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. Point this exactly to the root folder where you see 'src' and 'examples' ---
# Based on your previous screen, it should be:
set(RAYLIB_SRC_DIR "C:/raylib/raylib")

# Turn off building raylib examples/games to keep your build lightning fast
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# --- 2. Tell CMake to include and build raylib from your drive ---
add_subdirectory(${RAYLIB_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/raylib)

# --- 3. Define Your Executable ---
add_executable(MinesweeperClassic main.cpp ClassicMinesweeper.cpp
        BaseMinesweeper.cpp
        BaseMinesweeper.h
        NoGuessMinesweeper.cpp
        NoGuessMinesweeper.h
        GameController.cpp
        GameController.h
        Config.h
        Solver.cpp
        Solver.h
)

# --- 4. Link it up ---
# 'raylib' is the internal target name exported by raylib's CMakeLists
target_link_libraries(MinesweeperClassic PRIVATE raylib)