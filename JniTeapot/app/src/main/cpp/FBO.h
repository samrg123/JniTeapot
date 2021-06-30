#pragma once

#include <GLES3/gl3.h>

class FBO {
public:
    GLuint id = -1;

    FBO() : id(-1) {}

    inline void clear() {
        if(id != -1) {
            glDeleteFramebuffers(1, &id);
            id = -1;
        }
    }

    inline void create() {
        clear();
        glGenFramebuffers(1, &id);
        use();
    }

    inline void use() {
        glBindFramebuffer(GL_FRAMEBUFFER, id);
    }

    inline static void use_defualt() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

