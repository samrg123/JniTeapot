//
// Created by varun on 6/23/2021.
//

#pragma once

#include "FBO.h"

#include "glUtil.h"
#include <GLES3/gl3.h>
#include "android/asset_manager.h"
#include <glm.hpp>

class ShadowMap {
public:
    unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
    FBO shadow_depth_fbo;
    GLuint shadow_depth;
    GLuint shadow_program;
    const char* kShadowVert = "shaders/simple_shadow.vert";
    const char* kShadowFrag = "shaders/simple_shadow.frag";
    glm::mat4 light_space;
    glm::mat4 model;
    GLint light_space_loc;
    GLint model_loc;

    ShadowMap(unsigned int width=1024, unsigned int height=1024) : SHADOW_WIDTH(width), SHADOW_HEIGHT(height) {}
    ~ShadowMap() {}

    void init_gl(AAssetManager* asset_manager);
    void confgure_for_rendering();
};
