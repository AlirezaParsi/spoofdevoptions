cmake_minimum_required(VERSION 3.21)
project(SpoofModule)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include NDK sysroot and architecture-specific headers
include_directories(${ANDROID_NDK}/sysroot/usr/include)
include_directories(${ANDROID_NDK}/sysroot/usr/include/${ANDROID_ABI})

add_library(spoof SHARED spoof_module.cpp)

find_library(log-lib log)

include_directories(${CMAKE_SOURCE_DIR}/zygisk)
include_directories(${CMAKE_SOURCE_DIR}/include)

target_link_libraries(spoof ${log-lib})
