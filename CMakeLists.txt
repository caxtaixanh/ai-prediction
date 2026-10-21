cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(PredictiveInputBuffer VERSION 1.0.0)

# Ép trình biên dịch bỏ qua toàn bộ warning và lỗi kiểm tra vặt của thư viện fmt
add_compile_options(-w)

# Geode SDK
if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR
        "GEODE_SDK environment variable not set.\n"
    )
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} geode-sdk)

setup_geode_mod(${PROJECT_NAME})
