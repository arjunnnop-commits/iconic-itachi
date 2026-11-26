cmake_minimum_required(VERSION 3.22)
project(itachi_fp_duchamp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(itachi_fp_duchamp SHARED
    itachi_fp_duchamp.cpp
)

target_link_libraries(itachi_fp_duchamp
    binder_ndk
    log
    dl
)

add_executable(itachi_icon_init
    itachi_icon_init.c
)

target_link_libraries(itachi_icon_init
    log
)
