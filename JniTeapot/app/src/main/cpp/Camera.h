#pragma once

#include "log.h"
#include "Error.h"

#include "customAssert.h"

#include <camera/NdkCameraManager.h>

class Camera {
    
    private:
        ACameraDevice* device;
    
        static void OnDisconnect(void* context, ACameraDevice* device) {
            Panic("camera disconnected { context: %p, device: %p }", context, device);
        }
        
        static void OnError(void* context, ACameraDevice* device, int error) {
            Panic("camera error { context: %p, device: %p, error: %d }", context, device, error);
        }
        
        static inline ACameraDevice_StateCallbacks cameraCallbacks = {
            .onDisconnected = OnDisconnect,
            .onError = OnError
        };
        
    public:
        enum Direction { FRONT_CAMERA = ACAMERA_LENS_FACING_FRONT, BACK_CAMERA = ACAMERA_LENS_FACING_BACK };
        
        static constexpr const char* DirectionString(Direction direction) {
            return direction == BACK_CAMERA ? "Back" :
                   direction == FRONT_CAMERA ? "Front" :
                   "UNKNOWN";
        }
        
        Camera(Direction direction): device(nullptr) {
            ACameraManager *cameraManager = ACameraManager_create();
    
            ACameraIdList* cameraIdList;
            RUNTIME_ASSERT(ACameraManager_getCameraIdList(cameraManager, &cameraIdList) == ACAMERA_OK, "Failed to get cameraIdList");
            
            for(int i = 0; i < cameraIdList->numCameras; ++i) {
                const char* cameraId = cameraIdList->cameraIds[i];
    
                ACameraMetadata* metadata;
                ACameraManager_getCameraCharacteristics(cameraManager, cameraId, &metadata);
    
                ACameraMetadata_const_entry info;
                RUNTIME_ASSERT( ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &info) == ACAMERA_OK,
                                "Failed to get query camera direction");
                
                ACameraMetadata_free(metadata);
                
                if(info.data.u8[0] == direction) {
                    
                    int error = ACameraManager_openCamera(cameraManager, cameraId, &cameraCallbacks, &device);
                    if(error != ACAMERA_OK) {
                        Error("Failed to open camera %d/%d { cameraId: '%s', direction: %d [%s], error: %d }",
                              (i+1), cameraIdList->numCameras, cameraId, direction, DirectionString(direction), error);
                        continue;
                    }
    
                    Log("Using camera %d/%d { cameraId: '%s', direction: %d [%s] }",
                        (i+1), cameraIdList->numCameras, cameraId, direction, DirectionString(direction));
                    
                    break;
                }
            }
            ACameraManager_deleteCameraIdList(cameraIdList);
            ACameraManager_delete(cameraManager);

            if(!device) {
                Panic("Failed to open camera { direction: %d [%s] }", direction, DirectionString(direction));
            }
        }
};