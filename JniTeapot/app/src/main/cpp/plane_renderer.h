//
// Created by varun on 6/25/2021.
//

#ifndef JNITEAPOT_PLANE_RENDERER_H
#define JNITEAPOT_PLANE_RENDERER_H


#include "glUtil.h"
#include <android/asset_manager.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "arcore_c_api.h"
#include "glm.hpp"

// PlaneRenderer renders ARCore plane type.
class PlaneRenderer {
public:
    PlaneRenderer() = default;

    ~PlaneRenderer() = default;

    // Sets up OpenGL state used by the plane renderer.  Must be called on the
    // OpenGL thread.
    void InitializeGlContent(AAssetManager *asset_manager);

    // Draws the provided plane.
    void Draw(const glm::mat4 &projection_mat, const glm::mat4 &view_mat,
              const ArSession &ar_session, const ArPlane &ar_plane);

private:
    void UpdateForPlane(const ArSession &ar_session, const ArPlane &ar_plane);

    std::vector <glm::vec3> vertices_;
    std::vector <GLushort> triangles_;
    glm::mat4 model_mat_ = glm::mat4(1.0f);
    glm::vec3 normal_vec_ = glm::vec3(0.0f);

    GLuint texture_id_;

    GLuint shader_program_;
    GLint attri_vertices_;
    GLint uniform_mvp_mat_;
    GLint uniform_texture_;
    GLint uniform_model_mat_;
    GLint uniform_normal_vec_;
};


#endif //JNITEAPOT_PLANE_RENDERER_H
