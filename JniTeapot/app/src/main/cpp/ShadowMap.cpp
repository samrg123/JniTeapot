//
// Created by varun on 6/23/2021.
//

#include "ShadowMap.h"
#include <gtc/matrix_transform.hpp>
#include "gtc/type_ptr.hpp"
#include "util.h"

#define GLM_ENABLE_EXPERIMENTAL

#include "glm/gtx/string_cast.hpp"

GLint ShadowMap::model_loc = -1;
GLint ShadowMap::light_space_loc = -1;
GLuint ShadowMap::quadVBO = 0;
GLuint ShadowMap::quadVAO = 0;
GLuint ShadowMap::shadow_program = 0;
GLuint ShadowMap::debug_program = 0;

void ShadowMap::Init_Shaders(AAssetManager *asset_manager) {
    const char *kShadowVert = "shaders/simple_shadow.vert";
    const char *kShadowFrag = "shaders/simple_shadow.frag";
    const char *kDebugVert = "shaders/debug_texture.vert";
    const char *kDebugFrag = "shaders/debug_texture.frag";

    ShadowMap::shadow_program = util::CreateProgram(kShadowVert, kShadowFrag, asset_manager);
    ShadowMap::debug_program = util::CreateProgram(kDebugVert, kDebugFrag, asset_manager);
    ShadowMap::light_space_loc = glGetUniformLocation(shadow_program, "light_space");
    ShadowMap::model_loc = glGetUniformLocation(shadow_program, "model");

    // initialize debug quad
    {
        float quadVertices[] = {
                -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void *) (3 * sizeof(float)));
        GlAssertNoError("Error initializing debug quad in ShadowMap::init_gl");
    }

}

void ShadowMap::init_gl(unsigned int width, unsigned int height) {
    // configure transforms, textures, and fbo
    SHADOW_WIDTH = width;
    SHADOW_HEIGHT = height;

    shadow_depth_fbo.create();
    depth_tex.create();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    depth_tex.configure_params(false, true, false);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

    shadow_depth_fbo.use();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex.id, 0);
    GLuint buffs = GL_NONE;
    glDrawBuffers(1, &buffs);
    glReadBuffer(GL_NONE);
    FBO::use_defualt();
    float aspect_ratio = SHADOW_WIDTH / SHADOW_HEIGHT;
//    glm::mat4 light_view = glm::lookAt(glm::vec3(1.0f, 3.0f, 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
//    glm::mat4 _light_view = glm::lookAt(glm::vec3(0.01f, 3.0f, 0.0f), glm::vec3(0, 0, 0),glm::vec3(0, 1, 0));
    glm::mat4 _light_view = glm::lookAt(glm::vec3(0.01f, 1.1f, 0.0f), glm::vec3(0, 0, 0),glm::vec3(0, 1, 0));
    glm::mat4 _light_proj = glm::perspectiveRH(glm::radians(30.0f), aspect_ratio, 1.0f, 7.5f);
//    glm::mat4 _light_proj = glm::ortho(-1.f, 1.f, -1.f, 1.f, 0.1f, 7.5f);
//    light_space = _light_proj * _light_view;
//    light_view = _light_view;
    GlAssertNoError("Error configuring in ShadowMap::init_gl");
}

void ShadowMap::update_light_space(glm::vec3 &pos, glm::vec3 &u, glm::vec3 &v) {
    glm::vec3 center = pos + u / 2.f + v / 2.f;
    glm::vec3 un = glm::normalize(u), vn = glm::normalize(v);
    glm::vec3 normal = glm::normalize(glm::cross(u, v));
    float aspect = glm::length(u) / glm::length(v);
    float fovy = glm::atan(glm::length(v / 2.f));
    Log("%s", glm::to_string(center).c_str());
   Log("%f", fovy);
    glm::mat4 _light_view = glm::lookAt(center + glm::vec3(0.001f), glm::vec3(0), vn);
//    glm::mat4 light_proj = glm::perspectiveFovRH(fovy, glm::length(u), glm::length(v), 1.0f, 7.5f);
//    glm::mat4 _light_proj = glm::perspectiveRH(glm::radians(60.0f), aspect, 1.0f, 7.5f);
    const float frustum_radius = 0.5f;
    glm::mat4 _light_proj = glm::frustumRH(-frustum_radius, frustum_radius, -frustum_radius, frustum_radius, 0.5f, 5.0f);
//    glm::mat4 _light_view = glm::lookAt(glm::vec3(1.0f, 3.0f, 2.0f), glm::vec3(0, 0, 0),glm::vec3(0, 1, 0));
//    glm::mat4 _light_proj = glm::ortho(-1.f, 1.f, -1.f, 1.f, 1.0f, 7.5f);
    light_space = _light_proj * light_view;
    light_view = _light_view;
}

void ShadowMap::configure() {
    glEnable(GL_DEPTH_TEST);
    glUseProgram(shadow_program);
    glUniformMatrix4fv(light_space_loc, 1, GL_FALSE, glm::value_ptr(light_space));

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    shadow_depth_fbo.use();
    glClear(GL_DEPTH_BUFFER_BIT);
    GlAssertNoError("Error in ShadowMap::configure");
}

void ShadowMap::render_debug_quad() {
    glUseProgram(debug_program);
    depth_tex.bind(1, glGetUniformLocation(debug_program, "shadow_map"));

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    GlAssertNoError("Error in ShadowMap::render_debug_quad");
}

void ShadowMap::set_model(glm::mat4 &model) {
    glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
}
