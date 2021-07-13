//
// Created by varun on 6/23/2021.
//

#include "ShadowMap.h"
#include <gtc/matrix_transform.hpp>
#include "gtc/type_ptr.hpp"
#include "util.h"

void ShadowMap::init_gl(AAssetManager* asset_manager) {
    shadow_program = util::CreateProgram(kShadowVert, kShadowFrag, asset_manager);
    debug_program = util::CreateProgram(kDebugVert, kDebugFrag, asset_manager);

    // configure transforms, textures, and fbo
    {
        light_space_loc = glGetUniformLocation(shadow_program, "light_space");
        model_loc = glGetUniformLocation(shadow_program, "model");
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
        light_space = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 1000.0f) * glm::lookAt(glm::vec3(-2.0f, 4, -2), glm::vec3(0,0,0), glm::vec3(0, 1, 0));
        GlAssertNoError("Error configuring in ShadowMap::init_gl");
    }

    // initalize debug quad
    {
        float quadVertices[] = {
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        GlAssertNoError("Error initializing debug quad in ShadowMap::init_gl");
    }

}

void ShadowMap::configure() {
    glViewport(0,0,SHADOW_WIDTH, SHADOW_HEIGHT);
    shadow_depth_fbo.use();
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(shadow_program);
    glUniformMatrix4fv(light_space_loc, 1, GL_FALSE, glm::value_ptr(light_space));
    GlAssertNoError("Error in ShadowMap::configure");
}

void ShadowMap::render_debug_quad() {
    glUseProgram(debug_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadow_depth);
    glUniform1i(glGetUniformLocation(debug_program, "shadow_map"), 0);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    GlAssertNoError("Error in ShadowMap::render_debug_quad");
}
