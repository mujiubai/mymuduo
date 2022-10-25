cmake_minimum_required(VERSION 2.5)
project(mymuduo)

#设置动态库路径
set(LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
#设置调试信息和c++11标准
set(CMAKE_CXX_FLAGS "$CMAKE_CXX_FLAGS -g -std=c++11")

#定义参与编译文件
aux_source_directory(. SRC_LIST)
add_library(mymuduo SHARED ${SRC_LIST})

