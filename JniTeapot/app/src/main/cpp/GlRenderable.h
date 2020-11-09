#pragma once

#include "GlContext.h"
#include "GlCamera.h"

#include "types.h"
#include "mat.h"
#include "StringLiteral.h"

class GlRenderable {

    protected:
        GlCamera* camera;
        uint32 cameraMatrixId;
        
        // Warn: cameraMatrixId repeats every 2^32 iterations so its possible that this check fails if the cameraMatrixId differs by a multiple of 2^32.
        //       This has a failure probability of 4.6566129e-10, way smaller than the chance of the earth being destroyed massive meteor (1e-8)
        //       Failure mode is using the previous projection matrix until the next update to camera or object matrix (most likely will occur on the next frame)
        //       If this failure rate/mode is still concerning use a uint64 for the camera matrixId instead
        inline bool CameraUpdated() { return cameraMatrixId != camera->MatrixId(); }
        inline void ApplyCameraUpdate() { cameraMatrixId = camera->MatrixId(); }
        
        inline GlRenderable(GlCamera* c) { SetCamera(c); }
        
    public:
        
        inline void SetCamera(GlCamera* c) {
            RUNTIME_ASSERT(c, "Camera cannot be null");
            camera = c;
            
            //set the matrixId to an stale one
            cameraMatrixId = c->MatrixId()-1;
        }
};