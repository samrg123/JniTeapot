#pragma once

#include "GlContext.h"
#include "GlCubemap.h"

#include <glm.hpp>
#include <ext.hpp>
#include "include/arcore_c_api.h"
#include "jni.h"
#include "GLES2/gl2ext.h"

#include "shaderUtil.h"
#include "StringLiteral.h"

class ARWrapper {
private:
    enum Uniforms       { UNIFORM_CAM_VIEW_MATRIX };
    enum Attribs        { ATTRIB_POSITION};
    enum TextureUnits   { TU_TEXTURE };
    enum UniformBlocks  { UBLOCK_SKYBOX };
    
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
            static constexpr const char* kVertexSource =
                                                         "uniform mat4 viewMatrix;"
                                                         "attribute vec4 a_Position;"
                                                         "attribute vec2 a_TexCoord;"
                                                         "varying vec2 v_TexCoord;"
                                                         ""
                                                         "void main() {"
                                                         "  gl_Position = vec4(a_Position.xy, 1., 1.);"
                                                         //"  float rotatedY = (viewMatrix * vec4(0., -a_Position.y, 0., 0.)).y;"
                                                         "  float rotatedY = -a_Position.y;"
                                                         "  v_TexCoord.x = (.5*a_Position.x) + .5;"
                                                         "  v_TexCoord.y = (.5*rotatedY) + .5;"
                                                         "  v_TexCoord = a_TexCoord;"
                                                         "}";
            static constexpr const char* kFragmentSource =
                                                        "#extension GL_OES_EGL_image_external : require\n"
                                                        "#extension GL_OES_EGL_image_external_essl3 : require\n"
                                                        "precision mediump float;"
                                                        "varying vec2 v_TexCoord;"
                                                        "uniform samplerExternalOES sTexture;"
                                                        ""
                                                        "void main() {"
                                                        "    gl_FragColor = texture2D(sTexture, v_TexCoord);"
                                                        "    gl_FragColor.a = .15;"
                                                        "}";


            cameraProgram = GlContext::CreateGlProgram(kVertexSource, kFragmentSource);
            glUseProgram(cameraProgram);
            cameraTextureUniform  = glGetUniformLocation(cameraProgram, "sTexture");
            cameraViewMatPosition = glGetUniformLocation(cameraProgram, "viewMatrix");
            
            cameraPositionAttrib = glGetAttribLocation(cameraProgram, "a_Position");
            cameraTexCoordAtrrib = glGetAttribLocation(cameraProgram, "a_TexCoord");
        }
    
        static StringLiteral kVertexSource =
            ShaderVersionStr+
    
            ShaderUniformBlock(UBLOCK_SKYBOX)+ "SkyBox {"
            "   mat4 cameraTransform;"
            "   mat4[6] cubeProjections;"
            "};" +
            
            ShaderIn(ATTRIB_POSITION) + "vec3 position;" +
            ShaderOut(0) + "vec2 uvPosition;" +
        
            "void main() {"
            "   const vec2[4] vertices = vec2[]("
            "       vec2(-1., -1.),"
            "       vec2(-1.,  1.),"
            "       vec2( 1., -1.),"
            "       vec2( 1.,  1.)"
            "   );"
            "  vec4 screenPosition = vec4(vertices[gl_VertexID], 0., 1.);"
            "  vec4 worldPosition = cameraTransform * screenPosition;"
            "  gl_Position = cubeProjections[gl_InstanceID] * worldPosition;"
            "  uvPosition = screenPosition.xy;"
            "}";
        {
    
    
            static StringLiteral kFragmentSource =
                ShaderVersionStr+
                ShaderExtension("GL_OES_EGL_image_external")+
                ShaderExtension("GL_OES_EGL_image_external_essl3") +
                "precision mediump float;" +

                ShaderBinding(TU_TEXTURE) + "uniform samplerExternalOES sTexture;" +
                ShaderIn(0) + "vec2 v_TexCoord;" +
                ShaderOut(0) + "vec4 fragColor;" +

                "void main() {"
                "    fragColor = vec4(0.85, 0.4, 0.53, 1.0);"
                "}";
                
    
            //static constexpr const char*  = "attribute vec4 a_Position;"
            //                                            "attribute vec2 a_TexCoord;"
            //                                            "varying vec2 v_TexCoord;"
            //                                            ""
            //                                            "void main() {"
            //                                            "   gl_Position = a_Position;"
            //                                            "   v_TexCoord = a_TexCoord;"
            //                                            "}";
            


            cubemapProgram = GlContext::CreateGlProgram(kVertexSource.str, kFragmentSource.str);
        }

    }

    void UpdateScreenSize(int width_, int height_, int rotation = 1) {
        if (arSession != nullptr) {
            ArSession_setDisplayGeometry(arSession, rotation, width_, height_);
        }
        width = width_;
        height = height_;
    }

    inline Mat4<float> ProjectionMatrix(float nearPlane, float farPlane) const {
        Mat4<float> projMatrix;
        ArCamera_getProjectionMatrix(arSession, arCamera, nearPlane, farPlane, projMatrix.values);
        return projMatrix;
    }
    
    inline uint32 EglCameraTexture() const { return backgroundTextureId; }
    
    void Update(GlCamera &cam, GlCubemap &cubemap) {

        ArSession_setCameraTextureName(arSession, backgroundTextureId);

        ArStatus status = ArSession_update(arSession, arFrame);
        if (status != AR_SUCCESS) {
            Log("Error in AR Session update\n");
        }

        //update camera
        {
            ArFrame_acquireCamera(arSession, arFrame, &arCamera);

            union RawPose {
                struct {
                    Quaternion<float> rotation;
                    Vec3<float> position;
                };
                float vals[7];
            } rawPose;

            ArPose* cameraPose;
            ArPose_create(arSession, nullptr, &cameraPose);
            //ArCamera_getPose(arSession, arCamera, cameraPose);
            ArCamera_getDisplayOrientedPose(arSession, arCamera, cameraPose);
            ArPose_getPoseRaw(arSession, cameraPose, rawPose.vals);
            ArPose_destroy(cameraPose);

            //Mat4<float> mat;
            //ArCamera_getViewMatrix(arSession, arCamera, mat.values);
            //cam.SetViewMatrix(mat);

            GlTransform transform = cam.GetTransform();
            transform.SetRotation(rawPose.rotation);
            transform.position = rawPose.position;
            cam.SetTransform(transform);


            ArCamera_release(arCamera);
        }

        //TODO: in the process of merging with GlSkybox
        //UpdateCubemap(cubemap, cam);
    }

    void DrawCameraBackground(GlCamera& cam) {
        const static GLfloat kCameraVerts[] = {-1.0f, -1.0f,
                                               +1.0f, -1.0f,
                                               -1.0f, +1.0f,
                                               +1.0f, +1.0f};

        // TODO: this only needs to happen once unless the display geometry changes
        //for(int i = 0; i < ArrayCount(kCameraVerts); ++i) transformedUVs[i] = .5f * (kCameraVerts[i] + 1.f);
        ArFrame_transformCoordinates2d(arSession, arFrame, AR_COORDINATES_2D_OPENGL_NORMALIZED_DEVICE_COORDINATES, 4, kCameraVerts, AR_COORDINATES_2D_TEXTURE_NORMALIZED, transformedUVs);

        glDepthMask(GL_FALSE);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, backgroundTextureId);
        glUseProgram(cameraProgram);
        glUniform1i(cameraTextureUniform, 0);
    
        glUniformMatrix4fv(cameraViewMatPosition, 1, GL_FALSE, cam.Matrix().values);
        
        glVertexAttribPointer(cameraPositionAttrib, 2, GL_FLOAT, false, 0, kCameraVerts);
        glVertexAttribPointer(cameraTexCoordAtrrib, 2, GL_FLOAT, false, 0, transformedUVs);
        glEnableVertexAttribArray(cameraPositionAttrib);
        glEnableVertexAttribArray(cameraTexCoordAtrrib);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


        glUseProgram(0);
        glDepthMask(GL_TRUE);

        GlAssertNoError("Error drawing camera background texture");
    }
    
public:
    //Note: pulled out for debugging
    ArSession* arSession;
    
private:
    ARWrapper() {}
    static ARWrapper* instance;
    
    ArCamera* arCamera;
    ArFrame* arFrame;

    GLuint backgroundTextureId;
    GLuint cameraProgram;
    GLuint cameraViewMatPosition;
    GLuint cameraPositionAttrib;
    GLuint cameraTexCoordAtrrib;
    GLuint cameraTextureUniform;

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

    void UpdateCubemap(GlCubemap& cubemap, const GlCamera& cam) {
        //make a framebuffer, bind it, and make it the render target
        glGenFramebuffers(1, &cubemapFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, cubemapFbo);
        //glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, cubemapFbo);
        
        //set size of cubemap face
        glViewport(0, 0, (GLsizei)cubemap.getSize(), (GLsizei)cubemap.getSize());

        for (int i = 0; i < 6; ++i) {
            DrawCubemapFace(cubemap, i, cam);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        //return viewport back to normal
        glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    }

    // https://gamedev.stackexchange.com/questions/19461/opengl-glsl-render-to-cube-map
    void DrawCubemapFace(GlCubemap& cubemap, int iFace, const GlCamera& cam) {
        
        //attach a texture image to a framebuffer object
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + iFace, 
                                cubemap.getCubeTexture(), 0);
        GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        GlAssertNoError("Error drawing cubemap faceFIRST TWO");


        if(status != GL_FRAMEBUFFER_COMPLETE) {
            Log("Status error: %08x\n", status);
        }
        

        //use our shader to do thing
        glUseProgram(cubemapProgram);
        
        //Debug colors
        //if constexpr(false)
        {
            if(iFace%2) {
                glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            } else {
                glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT);
        }
        
        //glVertexArrayPointer(ATTRIB_POSITION, );
        
        //TODO: this
        //glUniformMatrix4fv(UNIFORM_CAM_VIEW_MATRIX, 1, false, cam.GetViewMatrix().values);
        //glDrawArrays(GL_TRIANGLE_STRIP, 0, 14);
    
        
        // glClear(GL_COLOR_BUFFER_BIT);
        GlAssertNoError("Error drawing cCLEARED OR WHATEVER");

        
        //activate texture and bind it
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.getCubeTexture());

        // glUseProgram(0);

        GlAssertNoError("Error drawing cubemap face");
    }
};

ARWrapper* ARWrapper::instance = nullptr;