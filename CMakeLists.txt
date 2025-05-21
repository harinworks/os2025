cmake_minimum_required(VERSION 3.15.0)
project(hnu-os)

if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W2 /wd4996 /utf-8")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W2 /wd4996 /utf-8 /EHs-c- /GR-")

    add_definitions(-D_CRT_NONSTDC_NO_WARNINGS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_HAS_EXCEPTIONS=0)
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c14 -Wall -Wno-extra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wno-extra -fno-exceptions -fno-rtti")

    if(BUILD_SHARED_LIBS)
        add_compile_options(-fvisibility=hidden)
    endif()
endif()

if(WIN32)
    add_definitions(-D_UNICODE -DUNICODE)
endif()

add_subdirectory(hw1)
add_subdirectory(hw2)
