cmake_minimum_required(VERSION 3.20)
project(MinesweeperClassic LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MINESWEEPER_IOS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR IOS)
    set(MINESWEEPER_IOS ON)
endif()

set(mobile_controls_default OFF)
if(ANDROID OR MINESWEEPER_IOS)
    set(mobile_controls_default ON)
endif()
option(MINESWEEPER_MOBILE_CONTROLS "Show the mobile toolbar, including on desktop for preview" ${mobile_controls_default})
set(RAYLIB_SRC_DIR "" CACHE PATH "Optional raylib source checkout for desktop, Android, or Web")

if(ANDROID)
    set(PLATFORM "Android" CACHE STRING "raylib platform" FORCE)
    set(WITH_PIC ON CACHE BOOL "Build raylib with position-independent code" FORCE)
elseif(EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "raylib platform" FORCE)
endif()

if(TARGET raylib)
    message(STATUS "Using the raylib target supplied by the parent project")
elseif(MINESWEEPER_IOS)
    message(FATAL_ERROR
            "iOS builds provide minesweeper_core only. Supply an iOS-compatible raylib target "
            "in a parent project before add_subdirectory(MineSweeper). The Apple launcher, "
            "raylib backend integration, signing, and packaging are not included.")
elseif(RAYLIB_SRC_DIR)
    if(NOT EXISTS "${RAYLIB_SRC_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "RAYLIB_SRC_DIR must point to a raylib source checkout containing CMakeLists.txt")
    endif()
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${RAYLIB_SRC_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/raylib")
else()
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.5
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

find_package(Threads REQUIRED)

add_library(minesweeper_core STATIC
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
set_target_properties(minesweeper_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(minesweeper_core PUBLIC cxx_std_20)
target_include_directories(minesweeper_core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_compile_definitions(minesweeper_core PRIVATE
        MINESWEEPER_MOBILE_CONTROLS=$<BOOL:${MINESWEEPER_MOBILE_CONTROLS}>
)
if(MINESWEEPER_IOS)
    target_compile_definitions(minesweeper_core PRIVATE PLATFORM_IOS)
endif()
target_link_libraries(minesweeper_core PUBLIC raylib Threads::Threads)

if(NOT MINESWEEPER_IOS)
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED main.cpp)
        target_link_options(${PROJECT_NAME} PRIVATE "-u" "ANativeActivity_onCreate")
    elseif(APPLE)
        add_executable(${PROJECT_NAME} MACOSX_BUNDLE main.cpp)
    else()
        add_executable(${PROJECT_NAME} main.cpp)
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE minesweeper_core)

    if(EMSCRIPTEN)
        set(MINESWEEPER_WEB_SHELL "" CACHE FILEPATH "Optional Emscripten HTML shell")
        if(NOT MINESWEEPER_WEB_SHELL)
            get_target_property(raylib_target_source_dir raylib SOURCE_DIR)
            find_file(minesweeper_default_web_shell minshell.html
                    HINTS "${raylib_target_source_dir}" "${raylib_target_source_dir}/src"
                    "${RAYLIB_SRC_DIR}/src" "${raylib_SOURCE_DIR}" "${raylib_SOURCE_DIR}/src"
                    NO_DEFAULT_PATH
            )
            if(NOT minesweeper_default_web_shell)
                message(FATAL_ERROR "Cannot locate raylib minshell.html; set MINESWEEPER_WEB_SHELL explicitly")
            endif()
            set(MINESWEEPER_WEB_SHELL "${minesweeper_default_web_shell}")
        endif()
        set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
        target_compile_options(minesweeper_core PUBLIC "-pthread")
        target_link_options(${PROJECT_NAME} PRIVATE
                "-pthread"
                "-sUSE_GLFW=3"
                "-sALLOW_MEMORY_GROWTH=1"
                "-sEXPORTED_RUNTIME_METHODS=ccall"
                "--shell-file" "${MINESWEEPER_WEB_SHELL}"
        )
    endif()
endif()
