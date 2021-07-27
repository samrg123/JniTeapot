#pragma once

#include "glUtil.h"
#include <GLES3/gl3.h>
#include <glm.hpp>

class QuadLight {
public:
    glm::vec3 pos;
    glm::vec3 u;
    glm::vec3 v;
    glm::vec3 emission;
    ShadowMap shadow_map;

    QuadLight(glm::vec3 _pos, glm::vec3 _u, glm::vec3 _v, glm::vec3 _emission) : pos(_pos), u(_u), v(_v), emission(_emission) {}
    ~QuadLight() {}

    void init_shadow_map(unsigned int shadow_width=1024, unsigned int shadow_height=1024) {
        shadow_map.init_gl(shadow_width, shadow_height);
        shadow_map.update_light_space(pos, u, v);
    }

    void update_light_space() {
        shadow_map.update_light_space(pos, u, v);
    }
};

struct LightVert {
    glm::vec3 pos;
    glm::vec3 emission;

    LightVert(glm::vec3 _pos, glm::vec3 _emission) : pos(_pos), emission(_emission) {}
};

class LightRenderer {
public:
    GLuint vbo, ibo, vao;
    const char* kLightVert = "shaders/light.vert";
    const char* kLightFrag = "shaders/light.frag";
    GLuint light_program;
    std::vector<LightVert> light_verts;
    std::vector<unsigned int> light_indices;

    LightRenderer() {}
    ~LightRenderer() {}

    inline void init_gl(AAssetManager* asset_manager) {
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ibo);
        glGenVertexArrays(1, &vao);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LightVert), (void*) offsetof(LightVert, pos));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,  sizeof(LightVert), (void*) offsetof(LightVert, emission));
        glEnableVertexAttribArray(1);

        light_program = util::CreateProgram(kLightVert, kLightFrag, asset_manager);
    }

    inline void update_verts(QuadLight* lights, size_t count) {
        for(size_t i = 0; i < count; ++i) {
            QuadLight& l = lights[i];
            unsigned int index = light_verts.size();

            light_verts.emplace_back(l.pos, l.emission);
            light_verts.emplace_back(l.pos+l.u, l.emission);
            light_verts.emplace_back(l.pos+l.v, l.emission);
            light_verts.emplace_back(l.pos+l.u+l.v, l.emission);

            light_indices.emplace_back(index + 0);
            light_indices.emplace_back(index + 1);
            light_indices.emplace_back(index + 2);
            light_indices.emplace_back(index + 1);
            light_indices.emplace_back(index + 3);
            light_indices.emplace_back(index + 2);
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(LightVert)*light_verts.size(), light_verts.data(), GL_STATIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int)*light_indices.size(), light_indices.data(), GL_STATIC_DRAW);

        GlAssertNoError("Gl Error at LightRenderer::update_verts");
    }

    inline void render_lights(glm::mat4& proj, glm::mat4& view, glm::mat4& model) {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

        glUseProgram(light_program);
        glUniformMatrix4fv(glGetUniformLocation(light_program, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(light_program, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(light_program, "model"), 1, GL_FALSE, glm::value_ptr(model));

        glDrawElements(GL_TRIANGLES, light_indices.size(), GL_UNSIGNED_INT, 0);
    }

};
