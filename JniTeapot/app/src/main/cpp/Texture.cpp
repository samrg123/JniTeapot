#include "Texture.h"
#include "FileManager.h"
#include "lodepng/lodepng.h"

void Texture::create() {
    clear();
    glGenTextures(1, &id);
    use();
}

void Texture::use(GLuint target) {
    glBindTexture(target, id);
}

void Texture::clear() {
    if (id != -1) {
        glDeleteTextures(1, &id);
        id = -1;
    }
}

void Texture::bind(int unit, int shader_loc) {
    bind_to_unit(unit);
    glUniform1i(shader_loc, unit);
}

void Texture::bind_to_unit(int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    use();
}

void Texture::configure_params(bool repeat, bool interpolate, bool mipmap) {
    use();
    if (mipmap)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        interpolate ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
    else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        interpolate ? GL_LINEAR : GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interpolate ? GL_LINEAR : GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}

void Texture::load_image(const char *path, bool generate_mipmaps) {
    // only supports png files
    Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
    FileManager::AssetBuffer *pngBuffer = FileManager::OpenAsset(path, &Memory::temporaryArena);
    uchar *bitmap;
    uint width, height;
    lodepng_decode_memory(&bitmap, &width, &height,
                          pngBuffer->data, pngBuffer->size,
                          LodePNGColorType::LCT_RGBA, 8);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 bitmap);

    // this causes glError for some reason TODO: find out why
    // if(generate_mipmaps) glGenerateMipmap(id);

    GlAssertNoError("Could not load image, %s", path);
    free(bitmap);
    Memory::temporaryArena.FreeBaseRegion(tmpRegion);
}

