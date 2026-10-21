cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(PredictiveInputBuffer VERSION 1.0.0)

# Tắt kiểm tra định dạng nghiêm ngặt của FMT
add_definitions(-DFMT_ENFORCE_COMPILE_STRING=0)
add_definitions(-DFMT_HEADER_ONLY)

# Geode SDK
if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR
        "GEODE_SDK environment variable not set.\n"
        "Point it at your Geode SDK clone, e.g.:\n"
        "  export GEODE_SDK=/path/to/geode\n"
    )
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} geode-sdk)

setup_geode_mod(${PROJECT_NAME})
