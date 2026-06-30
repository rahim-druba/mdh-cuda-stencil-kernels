cmake_minimum_required(VERSION 2.8.11)
project(mdh_cuda_stencil_kernels)

set(CMAKE_CXX_STANDARD 14)

# Framework library - shared by all three generators
include_directories(framework)
include_directories(framework/include)

file(GLOB FRAMEWORK_SRC framework/src/*.cpp)
add_library(mdh_cuda_generator STATIC ${FRAMEWORK_SRC})

# Kernel generators - each writes its .cu output to the build directory
add_executable(matmul    kernels/matmul/spec/matmul.cpp)
add_executable(poisson   kernels/poisson/spec/poisson.cpp)
add_executable(reservoir kernels/reservoir/spec/reservoir.cpp)

target_link_libraries(matmul    mdh_cuda_generator)
target_link_libraries(poisson   mdh_cuda_generator)
target_link_libraries(reservoir mdh_cuda_generator)
