#pragma once

#include "GlContext.h"

/**
 * cubemap object
 */
class GlCubemap {
   private:
    GLuint cubeSampler, cubemapTexture;
    int size;

   public:
    GlCubemap(const char* (&images)[6]) {
        cubemapTexture = GlContext::LoadCubemap(images, size);
        Log("Cubemap Texture: %d", cubemapTexture);

        glGenSamplers(1, &cubeSampler);
        glSamplerParameteri(cubeSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(cubeSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(cubeSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(cubeSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        GlAssertNoError("Failed to create cubemap sampler: %d", cubeSampler);
    }

    GLuint getCubeSampler() { return cubeSampler; }

    GLuint getCubeTexture() { return cubemapTexture; }

    int getSize() { return size; }
};