//
// Created by nachi on 9/11/20.
//

#ifndef ARGRAPHICS_UTIL_H
#define ARGRAPHICS_UTIL_H

#endif //ARGRAPHICS_UTIL_H
#include <android/log.h>
#include <map>
#include <string>
#include <sstream>
#include <GLES3/gl31.h>
#include <android/asset_manager.h>


#ifndef LOGI
#define LOGI(...) \
  __android_log_print(ANDROID_LOG_INFO, "argraphics_log", __VA_ARGS__)
#endif  // LOGI

#ifndef LOGE
#define LOGE(...) \
  __android_log_print(ANDROID_LOG_ERROR, "hello_ar_example_c", __VA_ARGS__)
#endif  // LOGE

#ifndef CHECK
#define CHECK(condition)                                                   \
  if (!(condition)) {                                                      \
    LOGE("*** CHECK FAILED at %s:%d: %s", __FILE__, __LINE__, #condition); \
    abort();                                                               \
  }
#endif  // CHECK

// Check GL error, and abort if an error is encountered.
//
// @param operation, the name of the GL function call.
void CheckGlError(const char* operation);

// Create a shader program ID.
//
// @param asset_manager, AAssetManager pointer.
// @param vertex_shader_file_name, the vertex shader source file.
// @param fragment_shader_file_name, the fragment shader source file.
// @return a non-zero value if the shader is created successfully, otherwise 0.
GLuint CreateProgram(const char* vertex_shader_file_name,
                     const char* fragment_shader_file_name,
                     AAssetManager* asset_manager);

// Create a shader program ID.
//
// @param asset_manager, AAssetManager pointer.
// @param vertex_shader_file_name, the vertex shader source file.
// @param fragment_shader_file_name, the fragment shader source file.
// @param define_values_map The #define values to add to the top of the shader
// source code.
// @return a non-zero value if the shader is created successfully, otherwise 0.
GLuint CreateProgram(const char* vertex_shader_file_name,
                     const char* fragment_shader_file_name,
                     AAssetManager* asset_manager,
                     const std::map<std::string, int>& define_values_map);

bool LoadTextFileFromAssetManager(const char* file_name,
                                  AAssetManager* asset_manager,
                                  std::string* out_file_text_string);