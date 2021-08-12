cmake_minimum_required(VERSION 3.10)
project(JniTeapot LANGUAGES C CXX)

# set up common compile options
set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_FLAGS 
    "-DARCORE_HEADER=\"\\\"${ARCORE_HEADER}\\\"\"   \
    -Wall                                           \
    -std=c++2a                                      \
    -fno-exceptions                                 \
    -fno-rtti -fdeclspec                            \
    -fno-threadsafe-statics                         \
    -ftemplate-depth=4096                           \
    -g                                              \
    -O0                                             \
    -DOPTIMIZED_BUILD=0                             \
")
    
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -save-temps=obj")

# build app
set(SOURCES jniTeapot.cpp lodepng/lodepng.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
set_target_properties(${PROJECT_NAME}
    PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        CXX_EXTENSIONS NO
)

# build FreeType lib - Warn can only target_include_directories after we set target with add_library
add_subdirectory("${PROJECT_SOURCE_DIR}/FreeType2.9")
target_include_directories(${PROJECT_NAME} PRIVATE "${FREETYPE_INC}")

# Import the ARCore (Google Play Services for AR) library.
add_library(arcore SHARED IMPORTED)
set_target_properties(arcore PROPERTIES IMPORTED_LOCATION
    ${ARCORE_LIBPATH}/${ANDROID_ABI}/libarcore_sdk_c.so
    INTERFACE_INCLUDE_DIRECTORIES ${ARCORE_INCLUDE}
    )

# Import the glm header file from the NDK.
add_library( glm INTERFACE )
set_target_properties( glm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${GLM_INCLUDE})

## link libs
target_link_libraries(
    ${PROJECT_NAME}
    log          # android log
    android      # android jni internals
    nativewindow # used for ANativeWindow_setFrameRate
    camera2ndk   # unsed for ACamera
    EGL
    GLESv3
    ${FREETYPE_LIB}
    glm         # glm - TODO: depreciate this
    arcore
)
