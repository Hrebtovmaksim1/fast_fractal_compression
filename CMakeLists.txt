cmake_minimum_required( VERSION 3.21 )
project( fractal_compression LANGUAGES CXX )

set(CMAKE_CXX_FLAGS "-std=c++20 -O3 -march=native -funroll-loops -flax-vector-conversions")

file(GLOB_RECURSE sources
  ./src/*.cpp
)


add_executable(fcomp main.cpp ${sources})
set_target_properties(fcomp PROPERTIES PREFIX "../")

include_directories(include)