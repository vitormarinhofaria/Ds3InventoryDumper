cmake_minimum_required(VERSION 3.25)
set(CMAKE_CXX_STANDARD_REQUIRED 23)

set(SRCS
    Include/MemoryItems.hpp src/MemoryItems.cpp
)

add_library(${PROJECT_NAME}_LIB ${SRCS})
target_precompile_headers(${PROJECT_NAME}_LIB PRIVATE Include/pch.h)
target_include_directories(${PROJECT_NAME}_LIB PRIVATE Include)