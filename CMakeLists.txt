cmake_minimum_required(VERSION 3.5.0)
project(dwarfInfoToJson VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CONFIG_RELATIVE_PATHS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)

include_directories(include)
include_directories(third_party/libdwarf/src/lib/)

add_subdirectory(third_party/libdwarf)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -finput-charset=UTF-8 -fexec-charset=UTF-8")
# set(CMAKE_EXE_LINKER_FLAGS "-static")

file(GLOB_RECURSE native_srcs src/*.cpp)
aux_source_directory(src DIR_SRCS)
add_executable(dwarfInfoToJson ${native_srcs})

target_link_libraries(dwarfInfoToJson stdc++exp libdwarf::dwarf-static) 
