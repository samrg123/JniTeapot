#pragma once

#include <glm.hpp>
#include <ext.hpp>
#include "include/arcore_c_api.h"
#include "jni.h"
#include "GLES2/gl2ext.h"
#include <sstream>

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
        {
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

        {
             static constexpr const char* kVertexSource = "attribute vec4 a_Position;"
                                                        // "attribute vec4 a_worldPos;"
                                                        "uniform mat4 m_viewMat;"
                                                        "varying vec4 viewPos;"
                                                        "uniform int i_face;"
                                                        "void main() {"
                                                        "   gl_Position = a_Position;"
                                                        "   vec4 fakeWorldPos;"
                                                        "   if (i_face == 0) { fakeWorldPos = vec4(1.f, -a_Position.y, -a_Position.x, 0.f); }"
                                                        "   if (i_face == 1) { fakeWorldPos = vec4(-1.f, -a_Position.y, a_Position.x, 0.f); }"
                                                        "   if (i_face == 2) { fakeWorldPos = vec4(a_Position.x, 1.f, a_Position.y, 0.f); }" // befre just -x
                                                        "   if (i_face == 3) { fakeWorldPos = vec4(a_Position.x, -1.f, -a_Position.y, 0.f); }"
                                                        "   if (i_face == 4) { fakeWorldPos = vec4(a_Position.x, -a_Position.y, 1.f, 0.f); }"  
                                                        "   if (i_face == 5) { fakeWorldPos = vec4(-a_Position.x, -a_Position.y, -1.f, 0.f); }"


                                                        "   viewPos = m_viewMat * vec4(fakeWorldPos.rgb, 0.f);"
                                                        "}";
            static constexpr const char* kFragmentSource = "#extension GL_OES_EGL_image_external : require\n"
                                                        ""
                                                        "precision mediump float;"
                                                        "uniform samplerExternalOES sTexture;"
                                                        "uniform mat4 m_viewMat;"
                                                        "varying vec4 viewPos;"
                                                        ""
                                                        "void main() {"
                                                        "    if (viewPos.x > -1.f && viewPos.x < 1.f && viewPos.y > -1.f && viewPos.y < 1.f && viewPos.z > 0.f) {"
                                                        "        vec2 cameraTexCoord = vec2((viewPos.x + 1.f)/2.f, (1.0f - (viewPos.y + 1.f)/2.f));"
                                                        "        gl_FragColor = texture2D(sTexture, cameraTexCoord);"
                                                        "    } else {"
                                                        "        gl_FragColor = vec4(0.f);"
                                                        "    }"
                                                        // "    gl_FragColor += vec4(abs(viewPos.rgb), 1.0f);"
                                                        "}";


            cubemapProgram = GlContext::CreateGlProgram(kVertexSource, kFragmentSource);
            
            cubemapCameraTextureUniform = glGetUniformLocation(cubemapProgram, "sTexture");


            // glBindAttribLocation(cubemapProgram, cubemapWorldPositionAttrib, "worldPos");
            // glBindAttribLocation(cubemapProgram, cubemapScreenPositionAttrib, "screenPos");
            cubemapScreenPositionAttrib = glGetAttribLocation(cubemapProgram, "a_Position");
            // cubemapWorldPositionAttrib = glGetAttribLocation(cubemapProgram, "a_worldPos");
            cubemapCameraMatrixUniform = glGetUniformLocation(cubemapProgram, "m_viewMat");
            cubemapFaceNumberUniform = glGetUniformLocation(cubemapProgram, "i_face");
        }

    }

    void UpdateScreenSize(int width_, int height_, int rotation = 1) {
        if (arSession != nullptr) {
            ArSession_setDisplayGeometry(arSession, rotation, width_, height_);
        }
        width = width_;
        height = height_;
    }

    void Update(GlCamera &cam, GlCubemap &cubemap) {
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

        ArCamera_getViewMatrix(arSession, arCamera, viewMat.values);
        ArCamera_getProjectionMatrix(arSession, arCamera,
                /*near=*/0.1f, /*far=*/100.f,
                                     projMat.values);

        cam.SetProjectionMatrix(projMat);
        cam.SetTransformMatrix(viewMat);

        ArCamera_release(arCamera);
        UpdateCubemap(cubemap);
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

    GLuint cubemapWorldPositionAttrib;
    GLuint cubemapScreenPositionAttrib;
    GLuint cubemapCameraMatrixUniform;
    GLuint cubemapCameraTextureUniform;
    GLuint cubemapFaceNumberUniform;

    Mat4<float> viewMat;
    Mat4<float> projMat;

    GLuint cubemapProgram;
    GLuint cubemapFbo;

    int width, height;

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

    void UpdateCubemap(GlCubemap& cubemap) {
        //make a framebuffer, bind it, and make it the render target
        glGenFramebuffers(1, &cubemapFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cubemapFbo);
        //glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, cubemapFbo);
        
        //set size of cubemap face
        glViewport(0, 0, (GLsizei)cubemap.getSize(), (GLsizei)cubemap.getSize());

        for (int i = 0; i < 6; ++i) {
            DrawCubemapFace(cubemap, i);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        //return viewport back to normal
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    }

    // https://gamedev.stackexchange.com/questions/19461/opengl-glsl-render-to-cube-map
    void DrawCubemapFace(GlCubemap& cubemap, int iFace) {
        static bool first[6] = {true, true, true, true, true, true};
        const static GLfloat kCameraVerts[] = {-1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f};
        
        //attach a texture image to a framebuffer object
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + iFace, 
                                cubemap.getCubeTexture(), 0);
        GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        GlAssertNoError("Error drawing cubemap faceFIRST TWO");


        if(status != GL_FRAMEBUFFER_COMPLETE) {
            Log("Status error: %08x\n", status);
        }

        glDepthMask(GL_FALSE);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, backgroundTextureId);
        //use our shader to do thing
        glUseProgram(cubemapProgram);

        if (first[iFace]) {
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            first[iFace] = false;
        }

        
        glUniform1i(cubemapCameraTextureUniform, 0);

        glUniform1i(cubemapFaceNumberUniform, iFace);


        glVertexAttribPointer(cubemapScreenPositionAttrib, 2, GL_FLOAT, false, 0, kCameraVerts);
        glEnableVertexAttribArray(cubemapScreenPositionAttrib);
        
        // glEnableVertexAttribArray(cubemapWorldPositionAttrib);
        Mat4<float> viewProj = projMat * viewMat;
        glUniformMatrix4fv(cubemapCameraMatrixUniform, 1, GL_FALSE, viewProj.values);
       
        GlAssertNoError("Error drawing before draw");

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        GlAssertNoError("Error drawing after draw");


        // glDisableVertexAttribArray(cubemapWorldPositionAttrib);
        glDisableVertexAttribArray(cubemapScreenPositionAttrib);

        glUseProgram(0);
        glDepthMask(GL_TRUE);


        //DrawCameraBackground();
        

        
        // glClear(GL_COLOR_BUFFER_BIT);
        GlAssertNoError("Error drawing cCLEARED OR WHATEVER");

        
        //activate texture and bind it
        // glActiveTexture(GL_TEXTURE0);
        // glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.getCubeTexture());

        // glUseProgram(0);

        GlAssertNoError("Error drawing cubemap face");
    }
    
    void setCubemapVerts(int iFace){
        //GLfloat cubemapVerts[12];
        // +x , -x , +y, -y, +z, -z
        switch(iFace) {
            case 0: // +x
                {
                const static GLfloat cubemapVerts[12] = {
                    +1.0f, -1.0f, -1.0f,
                    +1.0f, +1.0f, -1.0f,
                    +1.0f, -1.0f, +1.0f,
                    +1.0f, +1.0f, +1.0f
                };
                glVertexAttribPointer(cubemapWorldPositionAttrib, 3, GL_FLOAT, false, 0, cubemapVerts);
                break;
                }
            case 1: // -x
                {
                const static GLfloat cubemapVerts[12] = {
                    -1.0f, -1.0f, -1.0f,
                    -1.0f, +1.0f, -1.0f,
                    -1.0f, -1.0f, +1.0f,
                    -1.0f, +1.0f, +1.0f
                };
                glVertexAttribPointer(cubemapWorldPositionAttrib, 3, GL_FLOAT, false, 0, cubemapVerts);
                break;
                }
             case 2: // +y
                {
                const static GLfloat cubemapVerts[12] = {
                    -1.0f, +1.0f, -1.0f,
                    +1.0f, +1.0f, -1.0f,
                    -1.0f, +1.0f, +1.0f,
                    +1.0f, +1.0f, +1.0f
                };
                glVertexAttribPointer(cubemapWorldPositionAttrib, 3, GL_FLOAT, false, 0, cubemapVerts);
                break;
                }
             case 3: // -y
                {
                const static GLfloat cubemapVerts[12] = {
                    -1.0f, -1.0f, -1.0f,
                    +1.0f, -1.0f, -1.0f,
                    -1.0f, -1.0f, +1.0f,
                    +1.0f, -1.0f, +1.0f
                };
                glVertexAttribPointer(cubemapWorldPositionAttrib, 3, GL_FLOAT, false, 0, cubemapVerts);
                break;
                }
             case 4: // +z
                {
                const static GLfloat cubemapVerts[12] = {
                    -1.0f, -1.0f, +1.0f,
                    +1.0f, -1.0f, +1.0f,
                    -1.0f, +1.0f, +1.0f,
                    +1.0f, +1.0f, +1.0f
                };
                glVertexAttribPointer(cubemapWorldPositionAttrib, 3, GL_FLOAT, false, 0, cubemapVerts);
                break;
                }
             case 5: // -z
                {
                const static GLfloat cubemapVerts[12] = {
                    -1.0f, -1.0f, -1.0f,
                    +1.0f, -1.0f, -1.0f,
                    -1.0f, +1.0f, -1.0f,
                    +1.0f, +1.0f, -1.0f
                };  
                glVertexAttribPointer(cubemapWorldPositionAttrib, 3, GL_FLOAT, false, 0, cubemapVerts);
                break;
                }
        }
    } 
};

ARWrapper* ARWrapper::instance = nullptr;