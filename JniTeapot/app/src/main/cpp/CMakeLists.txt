cmake_minimum_required(VERSION 3.4.1)
project(JniTeapot LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# set up common compile options
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -std=c++2a -fno-exceptions -fno-rtti -fdeclspec")

# build app libd
#set(SOURCES JniTeapotActivity.h )
set(SOURCES jniTeapot.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
set_target_properties(${PROJECT_NAME}
    PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        CXX_EXTENSIONS NO
)

# build FreeType lib - Warn can only target_include_directories after we set target with add_library
add_subdirectory("${PROJECT_SOURCE_DIR}/FreeType2.9")
target_include_directories(${PROJECT_NAME} PRIVATE "${FREETYPE_INC}")

## link libs
target_link_libraries(
    ${PROJECT_NAME}
    log # android log
    android # android jni internals
    EGL
    GLESv3
    ${FREETYPE_LIB}
)
