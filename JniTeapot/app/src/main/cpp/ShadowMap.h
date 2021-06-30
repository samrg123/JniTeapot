//
// Created by varun on 6/23/2021.
//

#pragma once

#include "FBO.h"

#include "glUtil.h"
#include <GLES/gl3.h>
#include "android/asset_manager.h"
#include <glm/glm.hpp>

class ShadowMap {
    unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
    FBO shadow_depth_fbo;
    GLuint shadow_depth;
    Gluint shadow_program;
    const char* kShadowVert = "shaders/shadow.vert";
    const char* kShadowFrag = "shaders/shadow.frag";
    glm::mat4 light_space;
    glm::mat4 model;

    ShadowMap(unsigned int width=1024, unsigned int height=1024) : SHADOW_WIDTH(width), SHADOW_HEIGHT(height) {}
    ~ShadowMap() {}

    void init_gl(AAssetManager* asset_manager);
    void update_map();
};
