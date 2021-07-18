#pragma once

#include "glUtil.h"
#include "Memory.h"

class Texture {
public:
    GLuint id;

    Texture() : id(-1) {}
    ~Texture() { clear(); }

    void create();
    void use(GLuint target = GL_TEXTURE_2D);
    void clear();
    void bind(int unit, int shader_loc);
    void bind_to_unit(int unit);
    void configure_params(bool repeat, bool interpolate, bool mipmap = false);
    void load_image(const char *path, bool generate_mipmaps = false);
};
