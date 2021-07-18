//
// Created by varun on 6/20/2021.
//
#define GLM_ENABLE_EXPERIMENTAL
#include "gtc/matrix_transform.hpp"
#include <gtx/quaternion.hpp>
#include "util.h"
#include "log.h"

namespace util {

    glm::vec3 GetPlaneNormal(const ArSession& ar_session,
                             const ArPose& plane_pose) {
        float plane_pose_raw[7] = {0.f};
        ArPose_getPoseRaw(&ar_session, &plane_pose, plane_pose_raw);
        glm::quat plane_quaternion(plane_pose_raw[3], plane_pose_raw[0],
                                   plane_pose_raw[1], plane_pose_raw[2]);
        // Get normal vector, normal is defined to be positive Y-position in local
        // frame.
        return glm::rotate(plane_quaternion, glm::vec3(0., 1.f, 0.));
    }


    bool LoadTextFileFromAssetManager(const char *file_name,
                                      AAssetManager *asset_manager,
                                      std::string *out_file_text_string) {
        // If the file hasn't been uncompressed, load it to the internal storage.
        // Note that AAsset_openFileDescriptor doesn't support compressed
        // files (.obj).
        AAsset *asset =
                AAssetManager_open(asset_manager, file_name, AASSET_MODE_STREAMING);
        if (asset == nullptr) {
            Log("Error opening asset %s\n", file_name);
            return false;
        }

        off_t file_size = AAsset_getLength(asset);
        out_file_text_string->resize(file_size);
        int ret = AAsset_read(asset, &out_file_text_string->front(), file_size);

        if (ret <= 0) {
            Log("Failed to open file: %s\n", file_name);
            AAsset_close(asset);
            return false;
        }

        AAsset_close(asset);
        return true;
    }

    GLuint LoadShader(GLenum shader_type, const char *shader_source) {
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

            char *buf = reinterpret_cast<char *>(malloc(info_len));
            if (!buf) {
                return shader;
            }
            glGetShaderInfoLog(shader, info_len, nullptr, buf);
            Log("util::Could not compile shader %d:\n%s\n", shader_type,
                buf);
            free(buf);
            glDeleteShader(shader);
            shader = 0;
            exit(0);
        }

        return shader;
    }

    GLuint CreateProgram(const char *vertex_shader_file_name,
                         const char *fragment_shader_file_name,
                         AAssetManager *asset_manager) {
        std::string vertexShaderContent;
        if (!LoadTextFileFromAssetManager(vertex_shader_file_name, asset_manager,
                                          &vertexShaderContent)) {
            Log("Failed to load file: %s\n", vertex_shader_file_name);
//            return 0;
            exit(0);
        }

        std::string fragmentShaderContent;
        if (!LoadTextFileFromAssetManager(fragment_shader_file_name, asset_manager,
                                          &fragmentShaderContent)) {
            Log("Failed to load file: %s\n", fragment_shader_file_name);
//            return 0;
            exit(0);
        }

        // Prepend any #define values specified during this run.


        // TODO: FIX TO WORK WITH DEFINES
//    std::stringstream defines;
//    for (const auto& entry : define_values_map) {
//        defines << "#define " << entry.first << " " << entry.second << "\n";
//    }
//    fragmentShaderContent = defines.str() + fragmentShaderContent;
//    vertexShaderContent = defines.str() + vertexShaderContent;

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
            //GlAssertNoError("util::glAttachShader");
            glAttachShader(program, fragment_shader);
            //GlAssertNoError("util::glAttachShader");
            glLinkProgram(program);
            GLint link_status = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &link_status);
            if (link_status != GL_TRUE) {
                GLint buf_length = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &buf_length);
                if (buf_length) {
                    char *buf = reinterpret_cast<char *>(malloc(buf_length));
                    if (buf) {
                        glGetProgramInfoLog(program, buf_length, nullptr, buf);
                        Log("hello_ar::util::Could not link program:\n%s\n", buf);
                        free(buf);
                    }
                }
                glDeleteProgram(program);
                program = 0;
            }
        }
        return program;
    }

    GLuint CreateProgram(const char *vertex_shader_file_name,
                         const char *fragment_shader_file_name,
                         AAssetManager *asset_manager,
                         const std::map<std::string, int> &define_values_map) {
        std::map<std::string, int> empty_define;
        return CreateProgram(vertex_shader_file_name, fragment_shader_file_name,
                             asset_manager, empty_define);
    }


}
