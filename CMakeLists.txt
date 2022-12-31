cmake_minimum_required(VERSION 3.16)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

project(asyncio)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party/fmt)

add_library(asyncio include/asyncio/asyncio.h src/handle.cpp)
target_include_directories(asyncio PUBLIC include)
target_link_libraries(asyncio PUBLIC fmt::fmt)

add_executable(main main.cpp)
target_link_libraries(main PUBLIC asyncio)
