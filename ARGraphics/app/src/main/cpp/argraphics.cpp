//
// Created by nachi on 9/11/20.
//

#include <GLES3/gl31.h>
#include "argraphics.h"

ARGraphicsApplication::ARGraphicsApplication(AAssetManager* asset_manager) : asset_manager_(asset_manager){

}

ARGraphicsApplication::~ARGraphicsApplication() {}

void ARGraphicsApplication::OnPause() {}

void ARGraphicsApplication::OnResume(void *env, void *context, void *activity) {
    if (ar_session_ == nullptr) {
        ArInstallStatus install_status;
        // If install was not yet requested, that means that we are resuming the
        // activity first time because of explicit user interaction (such as
        // launching the application)
        bool user_requested_install = !install_requested_;

        // === ATTENTION!  ATTENTION!  ATTENTION! ===
        // This method can and will fail in user-facing situations.  Your
        // application must handle these cases at least somewhat gracefully.  See
        // HelloAR Java sample code for reasonable behavior.
        ArCoreApk_requestInstall(env, activity, user_requested_install,
                                       &install_status);

        switch (install_status) {
            case AR_INSTALL_STATUS_INSTALLED:
                break;
            case AR_INSTALL_STATUS_INSTALL_REQUESTED:
                install_requested_ = true;
                return;
        }

        // === ATTENTION!  ATTENTION!  ATTENTION! ===
        // This method can and will fail in user-facing situations.  Your
        // application must handle these cases at least somewhat gracefully.  See
        // HelloAR Java sample code for reasonable behavior.
        ArSession_create(env, context, &ar_session_);
//        CHECK(ar_session_);

        ConfigureSession();
        ArFrame_create(ar_session_, &ar_frame_);
//        CHECK(ar_frame_);

        ArSession_setDisplayGeometry(ar_session_, display_rotation_, width_,
                                     height_);
    }

    ArSession_resume(ar_session_);
//    CHECK(status == AR_SUCCESS);
}


bool ARGraphicsApplication::IsDepthSupported() {
    int32_t is_supported = 0;
    ArSession_isDepthModeSupported(ar_session_, AR_DEPTH_MODE_AUTOMATIC,
                                   &is_supported);
    LOGI("depth supported: %d", is_supported);
    return is_supported;
}

void ARGraphicsApplication::ConfigureSession() {
    const bool is_depth_supported = IsDepthSupported();

    ArConfig* ar_config = nullptr;
    ArConfig_create(ar_session_, &ar_config);
    if (is_depth_supported) {
        ArConfig_setDepthMode(ar_session_, ar_config, AR_DEPTH_MODE_AUTOMATIC);
    } else {
        ArConfig_setDepthMode(ar_session_, ar_config, AR_DEPTH_MODE_DISABLED);
    }

    if (is_instant_placement_enabled_) {
        ArConfig_setInstantPlacementMode(ar_session_, ar_config,
                                         AR_INSTANT_PLACEMENT_MODE_LOCAL_Y_UP);
    } else {
        ArConfig_setInstantPlacementMode(ar_session_, ar_config,
                                         AR_INSTANT_PLACEMENT_MODE_DISABLED);
    }
    ArSession_configure(ar_session_, ar_config);
    ArConfig_destroy(ar_config);
}

void ARGraphicsApplication::OnSurfaceCreated() {
    depth_texture_.CreateOnGlThread();
    background_renderer_.InitializeGlContent(asset_manager_,
                                             depth_texture_.GetTextureId());
}

void ARGraphicsApplication::OnDisplayGeometryChanged(int display_rotation, int width, int height) {
    glViewport(0, 0, width, height);
    display_rotation_ = display_rotation;
    width_ = width;
    height_ = height;
    if (ar_session_ != nullptr) {
        ArSession_setDisplayGeometry(ar_session_, display_rotation, width, height);
    }
}
void ARGraphicsApplication::OnDrawFrame(bool depthColorVisualizationEnabled,
                                        bool useDepthForOcclusion) {
    glClearColor(0.1f, 0.1f, 0.9f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    if (ar_session_ == nullptr) return;

    ArSession_setCameraTextureName(ar_session_,
                                   background_renderer_.GetTextureId());

    if (ArSession_update(ar_session_, ar_frame_) != AR_SUCCESS) {
        LOGE("OnDrawFrame ArSession_update error");
    }
    if (IsDepthSupported()) {
        depth_texture_.UpdateWithDepthImageOnGlThread(*ar_session_, *ar_frame_);
    }

    background_renderer_.Draw(ar_session_, ar_frame_,
                              false);

}

void ARGraphicsApplication::OnTouched(float x, float y) {}

