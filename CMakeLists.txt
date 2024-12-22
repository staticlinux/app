cmake_minimum_required(VERSION 3.30)

project(app)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_compile_options(-g)

include_directories(.)

add_subdirectory(src)