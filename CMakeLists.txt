cmake_minimum_required(VERSION 3.28)

project(half_life_3 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(vendor/McLaren)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

add_executable(half_life_3 ${GAME_SOURCES})
target_link_libraries(half_life_3 PRIVATE engine)

target_include_directories(half_life_3 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
