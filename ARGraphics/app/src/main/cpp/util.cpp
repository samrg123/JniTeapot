//
// Created by nachi on 9/11/20.
//
#include "util.h"

void CheckGlError(const char* operation) {
    bool anyError = false;
    for (GLint error = glGetError(); error; error = glGetError()) {
        LOGE("after %s() glError (0x%x)\n", operation, error);
        anyError = true;
    }
    if (anyError) {
//        abort();
    }
}

// Convenience function used in CreateProgram below.
static GLuint LoadShader(GLenum shader_type, const char* shader_source) {
    GLuint shader = glCreateShader(shader_type);
    if (!shader) {
        return shader;
    }

    glShaderSource(shader, 1, &shader_source, nullptr);
    glCompileShader(shader);
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        GLint info_len = 0;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
        if (!info_len) {
            return shader;
        }

        char* buf = reinterpret_cast<char*>(malloc(info_len));
        if (!buf) {
            return shader;
        }

        glGetShaderInfoLog(shader, info_len, nullptr, buf);
        LOGE("hello_ar::util::Could not compile shader %d:\n%s\n", shader_type,
             buf);
        free(buf);
        glDeleteShader(shader);
        shader = 0;
    }

    return shader;
}

GLuint CreateProgram(const char* vertex_shader_file_name,
                     const char* fragment_shader_file_name,
                     AAssetManager* asset_manager) {
    std::map<std::string, int> empty_define;
    return CreateProgram(vertex_shader_file_name, fragment_shader_file_name,
                         asset_manager, empty_define);
}
GLuint CreateProgram(const char* vertex_shader_file_name,
                     const char* fragment_shader_file_name,
                     AAssetManager* asset_manager,
                     const std::map<std::string, int>& define_values_map) {
    std::string vertexShaderContent;
    if (!LoadTextFileFromAssetManager(vertex_shader_file_name, asset_manager,
                                      &vertexShaderContent)) {
        LOGE("Failed to load file: %s", vertex_shader_file_name);
        return 0;
    }

    std::string fragmentShaderContent;
    if (!LoadTextFileFromAssetManager(fragment_shader_file_name, asset_manager,
                                      &fragmentShaderContent)) {
        LOGE("Failed to load file: %s", fragment_shader_file_name);
        return 0;
    }

    // Prepend any #define values specified during this run.
    std::stringstream defines;
    for (const auto& entry : define_values_map) {
        defines << "#define " << entry.first << " " << entry.second << "\n";
    }
    fragmentShaderContent = defines.str() + fragmentShaderContent;
    vertexShaderContent = defines.str() + vertexShaderContent;

    // Compiles shader code.
    GLuint vertexShader =
            LoadShader(GL_VERTEX_SHADER, vertexShaderContent.c_str());
    if (!vertexShader) {
        return 0;
    }

    GLuint fragment_shader =
            LoadShader(GL_FRAGMENT_SHADER, fragmentShaderContent.c_str());
    if (!fragment_shader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        CheckGlError("hello_ar::util::glAttachShader");
        glAttachShader(program, fragment_shader);
        CheckGlError("hello_ar::util::glAttachShader");
        glLinkProgram(program);
        GLint link_status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &link_status);
        if (link_status != GL_TRUE) {
            GLint buf_length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_length);
            if (buf_length) {
                char* buf = reinterpret_cast<char*>(malloc(buf_length));
                if (buf) {
                    glGetProgramInfoLog(program, buf_length, nullptr, buf);
                    LOGE("hello_ar::util::Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

bool LoadTextFileFromAssetManager(const char* file_name,
                                  AAssetManager* asset_manager,
                                  std::string* out_file_text_string) {
    // If the file hasn't been uncompressed, load it to the internal storage.
    // Note that AAsset_openFileDescriptor doesn't support compressed
    // files (.obj).
    AAsset* asset =
            AAssetManager_open(asset_manager, file_name, AASSET_MODE_STREAMING);
    if (asset == nullptr) {
        LOGE("Error opening asset %s", file_name);
        return false;
    }

    off_t file_size = AAsset_getLength(asset);
    out_file_text_string->resize(file_size);
    int ret = AAsset_read(asset, &out_file_text_string->front(), file_size);

    if (ret <= 0) {
        LOGE("Failed to open file: %s", file_name);
        AAsset_close(asset);
        return false;
    }

    AAsset_close(asset);
    return true;
}