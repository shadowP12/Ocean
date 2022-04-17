cmake_minimum_required(VERSION 3.13)

project(am_fft)

add_library(am_fft STATIC ${CMAKE_CURRENT_SOURCE_DIR}/am_fft.cpp)
target_include_directories(am_fft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})