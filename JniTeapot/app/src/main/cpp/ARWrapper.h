#pragma once

#include <glm.hpp>
#include <ext.hpp>
#include "include/arcore_c_api.h"
#include "jni.h"
#include "GLES2/gl2ext.h"

class ARWrapper {
public:
    static ARWrapper* Get() {
        if (!instance) {
            instance = new ARWrapper();
        }
        return instance;
    }

    void InitializeARSession(void* env, void* context) {
        //TODO use void ArCoreApk_checkAvailability(
        //  void *env,
        //  void *context,
        //  ArAvailability *out_availability
        //) instead

        ArStatus status = ArSession_create(env, context, &arSession);
        // TODO: check status

        ConfigureSession();

        ArFrame_create(arSession, &arFrame);
        ArSession_resume(arSession);
    }

    void InitializeGlContent() {
        glGenTextures(1, &backgroundTextureId);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, backgroundTextureId);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        static constexpr const char* kVertexSource = "attribute vec4 a_Position;"
                                                     "attribute vec2 a_TexCoord;"
                                                     "varying vec2 v_TexCoord;"
                                                     ""
                                                     "void main() {"
                                                     "   gl_Position = a_Position;"
                                                     "   v_TexCoord = a_TexCoord;"
                                                     "}";
        static constexpr const char* kFragmentSource = "#extension GL_OES_EGL_image_external : require\n"
                                                       ""
                                                       "precision mediump float;"
                                                       "varying vec2 v_TexCoord;"
                                                       "uniform samplerExternalOES sTexture;"
                                                       ""
                                                       "void main() {"
                                                       "    gl_FragColor = texture2D(sTexture, v_TexCoord);"
                                                       "}";


        cameraProgram = GlContext::CreateGlProgram(kVertexSource, kFragmentSource);
        cameraTextureUniform = glGetUniformLocation(cameraProgram, "sTexture");
        cameraPositionAttrib = glGetAttribLocation(cameraProgram, "a_Position");
        cameraTexCoordAtrrib = glGetAttribLocation(cameraProgram, "a_TexCoord");

    }

    void UpdateScreenSize(int width, int height, int rotation = 1) {
        if (arSession != nullptr) {
            ArSession_setDisplayGeometry(arSession, rotation, width, height);
        }
    }

    void Update(GlCamera &cam) {
        Log("Starting ARWrapper Update\n");
        ArSession_setCameraTextureName(arSession, backgroundTextureId);

        ArStatus status = ArSession_update(arSession, arFrame);
        if (status != AR_SUCCESS) {
            Log("Error in AR Session update\n");
        }
        ArFrame_acquireCamera(arSession, arFrame, &arCamera);

        ArPose* cameraPose;
        float poseRaw[7];

        ArPose_create(arSession, NULL, &cameraPose);
        ArCamera_getPose(arSession, arCamera, cameraPose);
        ArPose_getPoseRaw(arSession, cameraPose, poseRaw);

        Mat4<float> viewMat;
        Mat4<float> projMat;
        ArCamera_getViewMatrix(arSession, arCamera, viewMat.values);
        ArCamera_getProjectionMatrix(arSession, arCamera,
                /*near=*/0.1f, /*far=*/100.f,
                                     projMat.values);

        cam.SetProjectionMatrix(projMat);
        cam.SetTransformMatrix(viewMat);

        ArCamera_release(arCamera);
    }

    void DrawCameraBackground() {
        const static GLfloat kCameraVerts[] = {-1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f};

        // TODO: this only needs to happen once unless the display geometry changes
        ArFrame_transformCoordinates2d(arSession, arFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES, 4, kCameraVerts, AR_COORDINATES_2D_TEXTURE_NORMALIZED, transformedUVs);

        glDepthMask(GL_FALSE);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, backgroundTextureId);
        glUseProgram(cameraProgram);
        glUniform1i(cameraTextureUniform, 0);

        glVertexAttribPointer(cameraPositionAttrib, 2, GL_FLOAT, false, 0, kCameraVerts);
        glVertexAttribPointer(cameraTexCoordAtrrib, 2, GL_FLOAT, false, 0, transformedUVs);
        glEnableVertexAttribArray(cameraPositionAttrib);
        glEnableVertexAttribArray(cameraTexCoordAtrrib);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(cameraPositionAttrib);
        glDisableVertexAttribArray(cameraTexCoordAtrrib);

        glUseProgram(0);
        glDepthMask(GL_TRUE);

        GlAssertNoError("Error drawing camera background texture");
    }

private:
    ARWrapper() {}
    static ARWrapper* instance;

    ArSession* arSession;
    ArCamera* arCamera;
    ArFrame* arFrame;

    GLuint backgroundTextureId;
    GLuint cameraProgram;
    GLuint cameraPositionAttrib;
    GLuint cameraTexCoordAtrrib;
    GLuint cameraTextureUniform;

    float transformedUVs[8];

    bool IsDepthSupported() {
        int is_supported = 0;
        ArSession_isDepthModeSupported(arSession, AR_DEPTH_MODE_AUTOMATIC,
                                       &is_supported);
        return is_supported;
    }


    void ConfigureSession() {
        ArConfig* arConfig = nullptr;
        ArConfig_create(arSession, &arConfig);
        if (IsDepthSupported()) {
            ArConfig_setDepthMode(arSession, arConfig, AR_DEPTH_MODE_AUTOMATIC);
        } else {
            ArConfig_setDepthMode(arSession, arConfig, AR_DEPTH_MODE_DISABLED);
        }

        ArConfig_setInstantPlacementMode(arSession, arConfig,
                                             AR_INSTANT_PLACEMENT_MODE_DISABLED);
        ArSession_configure(arSession, arConfig);
        ArConfig_destroy(arConfig);
    }
};

ARWrapper* ARWrapper::instance = nullptr;