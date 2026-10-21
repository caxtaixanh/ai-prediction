cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(PredictiveInputBuffer VERSION 1.0.0)

# Ép thư viện fmt chạy ở chế độ Header-Only để tránh biên dịch các tệp nguồn lỗi trên Android NDK
set(FMT_HEADER_ONLY ON CACHE BOOL "" FORCE)
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
