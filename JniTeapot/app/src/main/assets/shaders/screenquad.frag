#version 310 es
precision mediump float;

#extension GL_OES_EGL_image_external : require
#extension GL_OES_EGL_image_external_essl3 : require

in vec2 v_TexCoord;
uniform samplerExternalOES sTexture;

out vec4 frag_color;

void main() {
    frag_color = texture(sTexture, v_TexCoord);
}
