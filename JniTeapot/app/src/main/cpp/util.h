#pragma once

#include <string>
#include <map>
#include "mathUtil.h"
#include "memUtil.h"
#include "stringUtil.h"
#include "android/asset_manager.h"
#include "GLES3/gl3.h"
#include "arcore_c_api.h"
#include "gtc/type_ptr.hpp"

constexpr unsigned int RGBA(uint r, uint g, uint b, uint a = 255) {
    return (r << 24) | (g << 16) | (b << 8) | a;
}

constexpr unsigned int RGBA(float r, float g, float b, float a = 1.f) {
    return (Round(r * 255.f) << 24) | (Round(g * 255.f) << 16) | (Round(b * 255.f) << 8) |
           Round(a * 255.f);
}

struct NoCopyClass {
    NoCopyClass() = default;

    NoCopyClass(const NoCopyClass &) = delete;

    NoCopyClass(const NoCopyClass &&) = delete;

    NoCopyClass &operator=(const NoCopyClass &) = delete;

    NoCopyClass &operator=(const NoCopyClass &&) = delete;
};

template<typename T>
void Swap(T &x, T &y) {
    T tmp = (T &&) x;
    x = (T &&) y;
    y = (T &&) tmp;
}

template<typename T>
constexpr auto InvokedResult(const T &func) { return func(); }

//Note: Returns smallest index that is not less than element in data
template<typename E_t, typename D_t, typename N_t, typename L_t>
inline N_t BinarySearchLower(const E_t &element, const D_t *data, N_t size,
                             const L_t &LessThan = [](auto n1, auto n2) { return n1 < n2; }) {

    N_t index = 0;
    while (size > 0) {

        //Note: this searches even indices more than once, but saves a conditional branch
        //EX: {1, 2, [3], 4} will split to {1, 2} {3, 4} and may recheck 3
        N_t halfSize = size >> 1;
        if (LessThan(data[index + halfSize], element)) index += size - halfSize;
        size = halfSize;
    }

    return index;
}

//Note: Returns smallest index that is greater than element
template<typename E_t, typename D_t, typename N_t, typename L_t>
inline N_t BinarySearchUpper(const E_t &element, const D_t *data, N_t size,
                             const L_t &LessThan = [](auto n1, auto n2) { return n1 < n2; }) {

    return BinarySearchLower(element, data, size,
                             [&](auto n1, auto n2) { return !LessThan(n2, n1); });
}

namespace util {
    class ScopedArPose {
    public:
        explicit ScopedArPose(const ArSession* session) {
            ArPose_create(session, nullptr, &pose_);
        }
        ~ScopedArPose() { ArPose_destroy(pose_); }
        ArPose* GetArPose() { return pose_; }
        // Delete copy constructors.
        ScopedArPose(const ScopedArPose&) = delete;
        void operator=(const ScopedArPose&) = delete;

    private:
        ArPose* pose_;
    };

    glm::vec3 GetPlaneNormal(const ArSession& ar_session, const ArPose& plane_pose);

    GLuint LoadShader(GLenum shader_type, const char *shader_source);

    bool LoadTextFileFromAssetManager(const char *file_name,
                                      AAssetManager *asset_manager,
                                      std::string *out_file_text_string);

    bool LoadPngFromAssetManager(int target, const std::string& path);

    GLuint CreateProgram(const char *vertex_shader_file_name,
                         const char *fragment_shader_file_name,
                         AAssetManager *asset_manager,
                         const std::map<std::string, int> &define_values_map);

    GLuint CreateProgram(const char *vertex_shader_file_name,
                         const char *fragment_shader_file_name,
                         AAssetManager *asset_manager);

    static void SetVec3(GLuint program, const char* uniform, glm::vec3 value) {
        glUniform3fv(glGetUniformLocation(program, uniform), 1, glm::value_ptr(value));
    }

    static void SetMat4(GLuint program, const char* uniform, glm::mat4& value) {
        glUniformMatrix4fv(glGetUniformLocation(program, uniform), 1, GL_FALSE, glm::value_ptr(value));
    }

    static void SetInt(GLuint program, const char* uniform, int value) {
        glUniform1i(glGetUniformLocation(program, uniform), value);
    }

    static void SetVec4(GLuint program, const char* uniform, glm::vec4 value) {
        glUniform4fv(glGetUniformLocation(program, uniform), 1, glm::value_ptr(value));
    }
}
