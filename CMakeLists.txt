cmake_minimum_required(VERSION 3.16)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(asyncio)

add_subdirectory(third_party/fmt)

option(WITH_IO "With Linux IO Support." ON)
if (NOT (UNIX AND NOT APPLE))  # NOT LINUX
    # If cmake version is high, use LINUX directly. (https://cmake.org/cmake/help/latest/variable/LINUX.html)
    set(WITH_IO OFF)
endif ()

add_library(asyncio include/asyncio/asyncio.h src/handle.cpp)
target_include_directories(asyncio PUBLIC include)
target_link_libraries(asyncio PUBLIC fmt::fmt)

if (NOT WITH_IO)
    MESSAGE(WARNING "IO is disabled.")
    target_compile_definitions(asyncio PUBLIC NO_IO)
endif ()

add_subdirectory(src_main)
