cmake_minimum_required(VERSION 3.16)
project(atlas-hero CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
  add_compile_options(/O2 /EHsc /permissive- /wd4100)
else()
  add_compile_options(-O3 -Wall -Wextra -Wno-unused-parameter)
endif()

set(ENGINE_SOURCES
  src/bitboard.cpp
  src/magic.cpp
  src/position.cpp
  src/movegen.cpp
  src/eval.cpp
  src/search.cpp
)

add_library(engine_core STATIC ${ENGINE_SOURCES})
target_include_directories(engine_core PUBLIC src)

add_executable(atlas-hero src/uci.cpp src/main.cpp)
target_link_libraries(atlas-hero PRIVATE engine_core)
if(NOT MSVC)
  find_package(Threads REQUIRED)
  target_link_libraries(atlas-hero PRIVATE Threads::Threads)
endif()

add_executable(perft test/perft.cpp)
target_link_libraries(perft PRIVATE engine_core)
