#pragma once

#include "FBO.h"
#include "Texture.h"

#include <glm.hpp>
#include <GLES3/gl3.h>

#include "glUtil.h"
#include "android/asset_manager.h"

class ShadowMap {
public:
    glm::mat4 light_space;
    unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
    FBO shadow_depth_fbo;
    Texture depth_tex;

    static GLuint shadow_program;
    static GLuint debug_program;
    static GLuint quadVBO;
    static GLuint quadVAO;
    static GLint light_space_loc;
    static GLint model_loc;

    ShadowMap() {}
    ~ShadowMap() {}

    static void Init_Shaders(AAssetManager* asset_manager);
    static void set_model(glm::mat4& model);

    void init_gl(unsigned int width=1024, unsigned int height=1024);
    void update_light_space(glm::vec3& pos, glm::vec3& u, glm::vec3& v);
    void configure();
    void render_debug_quad();
};
