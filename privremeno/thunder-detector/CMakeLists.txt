cmake_minimum_required(VERSION 3.16)

set (CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IDF_TARGET esp32s3)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thunder-detector)
