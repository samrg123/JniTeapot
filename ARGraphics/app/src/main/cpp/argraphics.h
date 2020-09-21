//
// Created by nachi on 9/11/20.
//

#ifndef ARGRAPHICS_ARGRAPHICS_H
#define ARGRAPHICS_ARGRAPHICS_H


#include <android/asset_manager.h>
#include "include/arcore_c_api.h"
#include "texture.h"
#include "background_renderer.h"
#include "util.h"

#define GLM_FORCE_RADIANS 1
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include "gtx/quaternion.hpp"


class ARGraphicsApplication {
public:

    //char * getDebugString();
    // Constructor and deconstructor.
    explicit ARGraphicsApplication(AAssetManager* asset_manager);
    ~ARGraphicsApplication();

    // OnPause is called on the UI thread from the Activity's onPause method.
    void OnPause();

    // OnResume is called on the UI thread from the Activity's onResume method.
    void OnResume(void* env, void* context, void* activity);

    // OnSurfaceCreated is called on the OpenGL thread when GLSurfaceView
    // is created.
    void OnSurfaceCreated();

    // OnDisplayGeometryChanged is called on the OpenGL thread when the
    // render surface size or display rotation changes.
    //
    // @param display_rotation: current display rotation.
    // @param width: width of the changed surface view.
    // @param height: height of the changed surface view.
    void OnDisplayGeometryChanged(int display_rotation, int width, int height);

    // OnDrawFrame is called on the OpenGL thread to render the next frame.
    void OnDrawFrame(bool depthColorVisualizationEnabled,
                     bool useDepthForOcclusion);

    // OnTouched is called on the OpenGL thread after the user touches the screen.
    // @param x: x position on the screen (pixels).
    // @param y: y position on the screen (pixels).
    void OnTouched(float x, float y);

    bool IsDepthSupported();

private:
//    glm::mat3 GetTextureTransformMatrix(const ArSession* session,
//                                        const ArFrame* frame);
    ArSession* ar_session_ = nullptr;
    ArFrame* ar_frame_ = nullptr;

    bool install_requested_ = false;
    bool calculate_uv_transform_ = false;
    int width_ = 1;
    int height_ = 1;
    int display_rotation_ = 0;
    bool is_instant_placement_enabled_ = true;
    char * message = "hi";
    AAssetManager* const asset_manager_;

    // The anchors at which we are drawing android models using given colors.
//    struct ColoredAnchor {
//        ArAnchor* anchor;
//        ArTrackable* trackable;
//        float color[4];
//    };
//
//    std::vector<ColoredAnchor> anchors_;
//
//    PointCloudRenderer point_cloud_renderer_;
    BackgroundRenderer background_renderer_;
//    PlaneRenderer plane_renderer_;
//    ObjRenderer andy_renderer_;
    Texture depth_texture_;

//    int32_t plane_count_ = 0;

    void ConfigureSession();

//    void UpdateAnchorColor(ColoredAnchor* colored_anchor);
};


#endif //ARGRAPHICS_ARGRAPHICS_H
