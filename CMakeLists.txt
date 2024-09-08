cmake_minimum_required(VERSION 3.28)
project(alsoft_backends_test)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DNOMINMAX)
add_definitions(-DWIN32_LEAN_AND_MEAN)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

link_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/include)
add_executable(alsoft_backends_test main.cpp WaveReader.cpp)
target_link_libraries(alsoft_backends_test OpenAL32 winmm)
