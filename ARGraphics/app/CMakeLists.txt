cmake_minimum_required(VERSION 3.4.1)

string(REPLACE " -Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -std=c++2a -fno-exceptions -fno-rtti -fdeclspec")

# Import the ARCore (Google Play Services for AR) library.
add_library(arcore SHARED IMPORTED)
set_target_properties(arcore PROPERTIES IMPORTED_LOCATION
                      ${ARCORE_LIBPATH}/${ANDROID_ABI}/libarcore_sdk_c.so
                      INTERFACE_INCLUDE_DIRECTORIES ${ARCORE_INCLUDE}
)

# Import the glm header file from the NDK.
add_library( glm INTERFACE )
set_target_properties( glm PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${GLM_INCLUDE})

# This is the main app library.
add_library(argraphics_native SHARED
            src/main/cpp/jni_interface.cpp
            src/main/cpp/argraphics.cpp
            src/main/cpp/background_renderer.cpp
            src/main/cpp/util.cpp
            src/main/cpp/texture.cpp)

target_include_directories(argraphics_native PRIVATE
           src/main/cpp)


add_subdirectory("${PROJECT_SOURCE_DIR}/src/main/cpp/FreeType2.9")
target_include_directories(argraphics_native PRIVATE "${FREETYPE_INC}")

target_link_libraries(argraphics_native
                      android
                      log
                      GLESv3
                      glm
                      arcore
                      ${FREETYPE_LIB}
)


