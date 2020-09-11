cmake_minimum_required(VERSION 3.4.1)

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
target_link_libraries(argraphics_native
                      android
                      log
                      GLESv2
                      glm
                      arcore)


