//
// Created by varun on 6/23/2021.
//

#include "ShadowMap.h"
#include <glm/gtc/matrix_transform.hpp>

void ShadowMap::init_gl(AAssetManager* asset_manager) {
    shadow_program = util::CreateProgram(kShadowVert, kShadowFrag, asset_manager);
    glGenTextures(1, &shadow_depth);
    glBindTexture(GL_TEXTURE_2D, shadow_depth);
    shadow_depth_fbo.create();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    shadow_depth_fbo.use();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_depth, 0);
    GLuint buffs = GL_NONE;
    glDrawBuffers(1, &buffs);
    glReadBuffer(GL_NONE);
    FBO::use_defualt();

    light_space = glm::ortho(-10, 10, -10, 10, 1, 1000) * glm::lookAt(glm::vec3(-2, -2, -2), glm::vec3(0,0,0), glm::vec3(0, 1, 0));
}

void ShadowMap::bind_fbo() {
    shadow_depth_fbo.use();
}
