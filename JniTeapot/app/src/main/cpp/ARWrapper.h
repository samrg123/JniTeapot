#pragma once

#include "jni.h"
#include ARCORE_HEADER

#include "GlContext.h"
#include "GlTransform.h"

class ARWrapper {
    private:
        ArSession* arSession;
        ArCamera* arCamera;
        ArFrame* arFrame;
        ArPose* cameraPose;
        
        GLuint eglCameraTexture;
        int width, height;
        
        ARWrapper(): arSession(nullptr), eglCameraTexture(0) {}
        
        bool IsDepthSupported() {
            int is_supported;
            ArSession_isDepthModeSupported(arSession, AR_DEPTH_MODE_AUTOMATIC, &is_supported);
            return is_supported;
        }
        
        void ConfigureSession() {
            
            ArConfig* arConfig;
            ArConfig_create(arSession, &arConfig);
            
            ArConfig_setDepthMode(arSession, arConfig, IsDepthSupported() ? AR_DEPTH_MODE_AUTOMATIC : AR_DEPTH_MODE_DISABLED);
            
            //Note: InstantPlacement and AugmentedImages have significant CPU overhead so we disable them
            ArConfig_setInstantPlacementMode(arSession, arConfig, AR_INSTANT_PLACEMENT_MODE_DISABLED);
            ArConfig_setAugmentedImageDatabase(arSession, arConfig, nullptr);
            
            ArSession_configure(arSession, arConfig);
            ArConfig_destroy(arConfig);
        }
        
    public:
        
        static inline
        ARWrapper* Instance() {
            static ARWrapper instance;
            return &instance;
        }
        
        inline Mat4<float> ProjectionMatrix(float nearPlane, float farPlane) const {
            RUNTIME_ASSERT(arSession, "arSession not Initialized");
            
            Mat4<float> projMatrix;
            ArCamera_getProjectionMatrix(arSession, arCamera, nearPlane, farPlane, projMatrix.values);
            return projMatrix;
        }
        inline void SetEglCameraTexture(GLuint eglTexture) {
            eglCameraTexture = eglTexture;
            ArSession_setCameraTextureName(arSession, eglTexture);
        }
        
        void UpdateScreenSize(int width_, int height_, int rotation = 1) {
            RUNTIME_ASSERT(arSession, "arSession not Initialized");
            
            ArSession_setDisplayGeometry(arSession, rotation, width_, height_);
            width = width_;
            height = height_;
        }
        
        void InitializeARWrapper(void* jniEnv, jobject jActivity) {
            RUNTIME_ASSERT(!arSession, "arSession already Initialized { arSession: %p }", arSession);
            
            ArAvailability arCoreAvailability;
            ArCoreApk_checkAvailability(jniEnv, jActivity, &arCoreAvailability);
    
            if(arCoreAvailability != AR_AVAILABILITY_SUPPORTED_INSTALLED) {
                Log("Installing ArCore { arCoreAvailability: %d }", arCoreAvailability);
        
                ArInstallStatus installStatus;
                ArCoreApk_requestInstall(jniEnv, jActivity, 1, &installStatus);
        
                //If ArCore was installed from open play store prompt update the install status
                if(installStatus == AR_INSTALL_STATUS_INSTALL_REQUESTED) {
                    ArCoreApk_requestInstall(jniEnv, jActivity, 0, &installStatus);
                }
        
                RUNTIME_ASSERT(installStatus == AR_INSTALL_STATUS_INSTALLED, "Failed to install ArCore { installStatus: %d }", installStatus);
            }
    
            RUNTIME_ASSERT(ArSession_create(jniEnv, jActivity, &arSession) == AR_SUCCESS, "Failed to create arSession");
            ConfigureSession();
    
            ArFrame_create(arSession, &arFrame);
            ArPose_create(arSession, nullptr, &cameraPose);
            ArFrame_acquireCamera(arSession, arFrame, &arCamera);

            ArSession_resume(arSession);
    
            Log("Successfully Initialized ArWrapper");
        }
        
        inline GlTransform UpdateFrame() {
            RUNTIME_ASSERT(arSession, "arSession not Initialized");
            RUNTIME_ASSERT(eglCameraTexture, "eglCameraTexture not set");
    
            /*TODO: fix AR_ERROR_FATAL error when updating session of galaxy s9
             *      ################### Stack Trace Begin ################
                    ARCoreError: third_party/arcore/ar/core/session.cc:1627	https://cs.corp.google.com/piper///depot/google3/third_party/arcore/ar/core/session.cc?g=0&l=1627
                    ARCoreError: third_party/arcore/ar/core/c_api/session_lite_c_api.cc:75	https://cs.corp.google.com/piper///depot/google3/third_party/arcore/ar/core/c_api/session_lite_c_api.cc?g=0&l=75
                    ################### Stack Trace End #################
                    
                    ################### Undecorated Trace Begin  #################
                    UNKNOWN:
                    ARCoreError: third_party/arcore/ar/core/session.cc:1627
                    ACameraCaptureSession_setRepeatingRequest ACAMERA_ERROR_UNKNOWN
                    ################### Undecorated Trace End  #################
             *
             */
            RUNTIME_ASSERT(ArSession_update(arSession, arFrame) == AR_SUCCESS, "Failed to update arFrame: status: %d", status);

            union RawPose {
                struct {
                    Quaternion<float> rotation;
                    Vec3<float> position;
                };
                float vals[7];
            } rawPose;
            
            //ArCamera_getPose(arSession, arCamera, cameraPose);
            ArCamera_getDisplayOrientedPose(arSession, arCamera, cameraPose);
            ArPose_getPoseRaw(arSession, cameraPose, rawPose.vals);
            
            return GlTransform(rawPose.position,
                               Vec3<float>::one, //scale
                               rawPose.rotation);
        }
};
