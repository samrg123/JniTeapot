//
// Created by nachi on 9/11/20.
//

#include <GLES3/gl32.h>
#include "argraphics.h"
#include <string>

// Note(Same): hack to get text on the screen
#include "GlContext.h"
#include "GlText.h"
#include "FileManager.h"

static GlContext glContext;
static GlText* glText;



ARGraphicsApplication::ARGraphicsApplication(JNIEnv* env, jobject j_asset_manager) : asset_manager_(AAssetManager_fromJava(env, j_asset_manager)){

    //Note(Sam): hack to get text to work
    FileManager::Init(env, j_asset_manager);
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
    obj_renderer_.InitializeGlContent(asset_manager_, "meshes/cow.obj");
    // NOTE(Sam): hack to get GlText to work!!!
    {
        delete glText;
        glText = new GlText(&glContext, "fonts/xolonium_regular.ttf");

        glText->RenderTexture(GlText::RenderParams {
            .targetGlyphSize = 25,
            .renderStringAttrib = { .rgba = RGBA(1.f, 0, 0) },
        });

        //NOTE: just for text blending - this should be turned off otherwise to accelerate rendering
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }
}

void ARGraphicsApplication::OnDisplayGeometryChanged(int display_rotation, int width, int height) {
    glViewport(0, 0, width, height);
    display_rotation_ = display_rotation;
    width_ = width;
    height_ = height;
    if (ar_session_ != nullptr) {
        ArSession_setDisplayGeometry(ar_session_, display_rotation, width, height);
    }

    // NOTE(Sam): hack to get GlText to work!!!
    {
        glContext.UpdateScreenSize(width, height);
        glText->UpdateScreenSize(width, height);
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
    ArCamera* ar_camera;
    ArFrame_acquireCamera(ar_session_, ar_frame_, &ar_camera);

    ArPose* cameraPose;
    float poseRaw[7];

    ArPose_create(ar_session_, NULL, &cameraPose);
    ArCamera_getPose(ar_session_, ar_camera, cameraPose);
    ArPose_getPoseRaw(ar_session_, cameraPose, poseRaw);

    glm::mat4 view_mat;
    glm::mat4 projection_mat;
    ArCamera_getViewMatrix(ar_session_, ar_camera, glm::value_ptr(view_mat));
    ArCamera_getProjectionMatrix(ar_session_, ar_camera,
            /*near=*/0.1f, /*far=*/100.f,
                                 glm::value_ptr(projection_mat));

    ArCamera_release(ar_camera);

    LOGI("Camera position: (%f, %f, %f)\n", poseRaw[4], poseRaw[5], poseRaw[6]);



    if (IsDepthSupported()) {
        depth_texture_.UpdateWithDepthImageOnGlThread(*ar_session_, *ar_frame_);
    }


    // Note(Sam): 'background_renderer_.Draw' doesn't use vao or vbo, because most glPrograms will
    //            use them we unbind before we call background_renderer_.Draw instead of in GlText::Draw
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    background_renderer_.Draw(ar_session_, ar_frame_,
                              false); //depth info
    glm::mat4 model_mat(0.1f);
    float color_correction[4] = {1, 1, 1, 1};
    float object_color[4] = {1, 1, 1, 1};
    obj_renderer_.Draw(projection_mat, view_mat, model_mat, color_correction, object_color);

    // Note(Sam): Example opengl text
//    if constexpr(0)
    {
        glText->SetStringAttrib(GlText::StringAttrib {
            .scale = Vec2(3.f, 3.f),
            .rgba = RGBA(1.f, 1.f, 1.f),
            .depth = -.9f
        });

        static int counter = 0;
        glText->PushString(Vec2(50, 50),  "Hello World: %d", counter++);
        glText->Draw();
        glText->Clear();
    }

}

void ARGraphicsApplication::OnTouched(float x, float y) {}

//char * ARGraphicsApplication::getDebugString()
//{
   /* int64_t * out_timestamp_ns = 0;
    ArFrame_getTimestamp(ar_session_, ar_frame_, out_timestamp_ns);
    std::string vOut = std::to_string(reinterpret_cast<long>(out_timestamp_ns));
    char * cstring = new char [vOut.length()+1];
    std::strcpy (cstring, vOut.c_str());
    return cstring; */
 //  return message;
//}