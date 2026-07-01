# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(app)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

add_subdirectory("SDL")

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${NATIVERENDER_ROOT_PATH}/SDL/include)

add_library(main SHARED napi_init.cpp)
target_link_libraries(main PUBLIC SDL3::SDL3)
