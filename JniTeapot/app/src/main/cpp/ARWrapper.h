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

        static inline ARWrapper* Instance() {
            static ARWrapper instance;
            return &instance;
        }

        static inline ARWrapper* FrontInstance() {
            static ARWrapper instance;
            return &instance;
        }

        inline const ArSession* ArSession() const  { 
            RUNTIME_ASSERT(arSession, "arSession not Initialized");
            return arSession; 
        }

        inline ArStatus Resume() { return ArSession_resume(arSession); }

        inline ArStatus Pause() { return ArSession_pause(arSession); }

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

        struct EglTextureSize {
            Vec2<int32> cpuTextureSize;
            Vec2<int32> gpuTextureSize;
        };

        inline EglTextureSize GetEglTextureSize() {

            EglTextureSize result;

            ArCameraConfig* arCameraConfig;
            ArCameraConfig_create(arSession, &arCameraConfig);

            ArSession_getCameraConfig(arSession, arCameraConfig);
                    
            ArCameraConfig_getImageDimensions(arSession, arCameraConfig, &result.cpuTextureSize.x, &result.cpuTextureSize.y);            
            ArCameraConfig_getTextureDimensions(arSession, arCameraConfig, &result.gpuTextureSize.x, &result.gpuTextureSize.y);

            ArCameraConfig_destroy(arCameraConfig);

            return result;
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
            
            
            // TODO: This is depreciated in ArCore 1.23... we are still using 1.19 so this is our only option
            //       upgrade version of ArCore and use the non-depreciated way via ArSession_setCameraConfig with 
            //       the desired front facing camera config retrieved from ArSession_getSupportedCameraConfigsWithFilter.

            ArSessionFeature arSessionFeatures[] = {
                (this == FrontInstance() ? AR_SESSION_FEATURE_FRONT_CAMERA : AR_SESSION_FEATURE_END_OF_LIST),
                AR_SESSION_FEATURE_END_OF_LIST
            };

            auto arSessionResult = ArSession_createWithFeatures(jniEnv, jActivity, arSessionFeatures, &arSession);
            RUNTIME_ASSERT(arSessionResult == AR_SUCCESS, "Failed to create arSession");
            
            ConfigureSession();

            ArFrame_create(arSession, &arFrame);
            ArPose_create(arSession, nullptr, &cameraPose);
            ArFrame_acquireCamera(arSession, arFrame, &arCamera);

            //TODO: Right now this is just here for logging. Should we move this somewhere else?
            {
                EglTextureSize textureSize = GetEglTextureSize();            
             
                Log("Successfully Initialized ArWrapper - Camera (CPU) image size '%d x %d' | Texture (GPU) Size '%d x %d'",
                    textureSize.cpuTextureSize.x, textureSize.cpuTextureSize.y,
                    textureSize.gpuTextureSize.x, textureSize.gpuTextureSize.y);
            }
        }

        struct UpdateFrameResult {
            GlTransform frameTransform;
            ArTrackingState trackingState;
            ArTrackingFailureReason trackingFailureReason;
        };
    
        inline UpdateFrameResult UpdateFrame() {
            RUNTIME_ASSERT(arSession, "arSession not Initialized");
            RUNTIME_ASSERT(eglCameraTexture, "eglCameraTexture not set");
            RUNTIME_ASSERT(ArSession_update(arSession, arFrame) == AR_SUCCESS, "Failed to update arFrame: status: %d");

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


            UpdateFrameResult frameResult;
            frameResult.frameTransform = GlTransform(rawPose.position,
                                                     Vec3<float>::one, //scale
                                                     rawPose.rotation);

            ArCamera_getTrackingState(arSession, arCamera, &frameResult.trackingState);

            if(frameResult.trackingState != AR_TRACKING_STATE_TRACKING) {
                ArCamera_getTrackingFailureReason(arSession, arCamera, &frameResult.trackingFailureReason);
            } else {
                frameResult.trackingFailureReason = AR_TRACKING_FAILURE_REASON_NONE;
            }

            return frameResult;
        }
};
