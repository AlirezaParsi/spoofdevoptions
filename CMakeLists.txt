cmake_minimum_required(VERSION 3.21)
project(SpoofModule)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure NDK sysroot includes are available
include_directories(${ANDROID_NDK}/sysroot/usr/include)

add_library(spoof SHARED spoof_module.cpp)

find_library(log-lib log)

include_directories(${CMAKE_SOURCE_DIR}/zygisk)
include_directories(${CMAKE_SOURCE_DIR}/include)

target_link_libraries(spoof ${log-lib})
