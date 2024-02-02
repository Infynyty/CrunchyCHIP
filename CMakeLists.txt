cmake_minimum_required(VERSION 3.27)
project(CHIP_8 C)

find_package(SDL2 REQUIRED)

set(CMAKE_C_STANDARD 17)

include_directories(CHIP_8 ${SDL2_INCLUDE_DIRS})

add_executable(CHIP_8 main.c
        CHIP-8.c
        CHIP-8.h
)

target_link_libraries(CHIP_8 ${SDL2_LIBRARIES})
