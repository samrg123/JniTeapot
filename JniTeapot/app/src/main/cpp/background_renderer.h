//
// Created by varun on 6/20/2021.
//

#pragma once

#include <GLES3/gl31.h>
#include "arcore_c_api.h"
#include "android/asset_manager.h"
#include "util.h"
#include "GLES2/gl2ext.h"

#include "log.h"
#include "GlCamera.h"

namespace {
// Positions of the quad vertices in clip space (X, Y).
    const GLfloat kVertices[] = {
            -1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f,
    };

    constexpr char kCameraVertexShaderFilename[] = "shaders/screenquad.vert";
    constexpr char kCameraFragmentShaderFilename[] = "shaders/screenquad.frag";

    constexpr char kDepthVisualizerVertexShaderFilename[] =
            "shaders/background_show_depth_color_visualization.vert";
    constexpr char kDepthVisualizerFragmentShaderFilename[] =
            "shaders/background_show_depth_color_visualization.frag";

}  // namespace

class BackgroundRenderer {
private:
    static constexpr int kNumVertices = 4;

    GLuint camera_program_;
    GLuint depth_program_;

    GLuint depth_texture_id_;

    GLuint camera_position_attrib_;
    GLuint camera_tex_coord_attrib_;
    GLuint camera_texture_uniform_;

    GLuint depth_texture_uniform_;
    GLuint depth_position_attrib_;
    GLuint depth_tex_coord_attrib_;

    float transformed_uvs_[kNumVertices * 2];
    bool uvs_initialized_ = false;

public:
    BackgroundRenderer() = default;

    ~BackgroundRenderer() = default;

    // Sets up OpenGL state.  Must be called on the OpenGL thread and before any
    // other methods below.
    inline void InitializeGlContent(AAssetManager *asset_manager, int depthTextureId) {
//    glGenTextures(1, &camera_texture_id_);
//    glBindTexture(GL_TEXTURE_EXTERNAL_OES, camera_texture_id_);
//    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        camera_program_ =
                util::CreateProgram(kCameraVertexShaderFilename,
                                    kCameraFragmentShaderFilename, asset_manager);

        if (!camera_program_) {
            Log("Could not create program.\n");
            exit(0);
        }
        camera_texture_uniform_ = glGetUniformLocation(camera_program_, "sTexture");
        camera_position_attrib_ = glGetAttribLocation(camera_program_, "a_Position");
        camera_tex_coord_attrib_ = glGetAttribLocation(camera_program_, "a_TexCoord");

        depth_program_ = util::CreateProgram(kDepthVisualizerVertexShaderFilename,
                                             kDepthVisualizerFragmentShaderFilename,
                                             asset_manager);
        if (!depth_program_) {
            Log("Could not create program.\n");
            exit(0);
        }

        depth_texture_uniform_ =
                glGetUniformLocation(depth_program_, "u_DepthTexture");
        depth_position_attrib_ = glGetAttribLocation(depth_program_, "a_Position");
        depth_tex_coord_attrib_ = glGetAttribLocation(depth_program_, "a_TexCoord");

        depth_texture_id_ = depthTextureId;
    }

    // Draws the background image.  This methods must be called for every ArFrame
    // returned by ArSession_update() to catch display geometry change events.
    //  debugShowDepthMap Toggles whether to show the live camera feed or latest
    //  depth image.
    inline void Draw(const ArSession *session, const ArFrame *frame,
                     bool debug_show_depth_map, GlCamera &camera) {
        static_assert(std::extent<decltype(kVertices)>::value == kNumVertices * 2,
                      "Incorrect kVertices length");

        // If display rotation changed (also includes view size change), we need to
        // re-query the uv coordinates for the on-screen portion of the camera image.
        int32_t geometry_changed = 0;
        ArFrame_getDisplayGeometryChanged(session, frame, &geometry_changed);
        if (geometry_changed != 0 || !uvs_initialized_) {
            ArFrame_transformCoordinates2d(
                    session, frame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES,
                    kNumVertices, kVertices, AR_COORDINATES_2D_TEXTURE_NORMALIZED,
                    transformed_uvs_);
            uvs_initialized_ = true;
        }

        int64_t frame_timestamp;
        ArFrame_getTimestamp(session, frame, &frame_timestamp);
        if (frame_timestamp == 0) {
            // Suppress rendering if the camera did not produce the first frame yet.
            // This is to avoid drawing possible leftover data from previous sessions if
            // the texture is reused.
            return;
        }

        glDepthMask(GL_FALSE);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, camera.EglTexture());
        glUseProgram(camera_program_);
        glUniform1i(camera_texture_uniform_, 0);

        // Set the vertex positions and texture coordinates.
        glVertexAttribPointer(0, 2, GL_FLOAT, false, 0,
                              kVertices);
        glVertexAttribPointer(1, 2, GL_FLOAT, false, 0,
                              transformed_uvs_);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Disable vertex arrays
        if (debug_show_depth_map) {
            glDisableVertexAttribArray(depth_position_attrib_);
            glDisableVertexAttribArray(depth_tex_coord_attrib_);
        } else {
            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
        }

        glUseProgram(0);
        glDepthMask(GL_TRUE);
        GlAssertNoError("BackgroundRenderer::Draw() error");
    }

};

