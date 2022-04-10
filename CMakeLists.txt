cmake_minimum_required(VERSION 3.17)
project(Ocean)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-DPROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(Ocean main.cpp FourierTransform.cpp WavesGenerator.cpp)

# glfw
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/glfw EXCLUDE_FROM_ALL glfw.out)
target_link_libraries(Ocean PRIVATE glfw)

# blast
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/Blast Blast.out)
target_link_libraries(Ocean PUBLIC Blast)

# glm
add_library(glm INTERFACE)
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
target_include_directories(glm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/glm)
target_link_libraries(Ocean PUBLIC glm)

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/stb)
target_link_libraries(Ocean PRIVATE stb)