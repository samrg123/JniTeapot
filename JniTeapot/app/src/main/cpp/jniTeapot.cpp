#include <jni.h>
#include <pthread.h>

#include "tests.h"

#include "panic.h"
#include "customAssert.h"

#include "FileManager.h"
#include "GlContext.h"
#include "GlText.h"
#include "Timer.h"

#include "Memory.h"

#include "GlObject.h"
#include "GlSkybox.h"

#include "ARWrapper.h"
//#include "Camera.h"

#include "background_renderer.h"
#include "plane_renderer.h"
#include "point_cloud_renderer.h"

#include <android/native_window_jni.h>
#include "FBO.h"

#define JFunc(jClass, jMethod) JNIEXPORT JNICALL Java_com_eecs487_jniteapot_##jClass##_ ##jMethod

//TODO: package this up somewhere
GlContext glContext;

constexpr int kTargetFPS = 60;
constexpr float kTargetMsFrameTime = 1000.f / kTargetFPS;

struct RenderThreadParams {
    ANativeWindow *androidNativeWindow;
};

void InitGlesState() {
    //glClearColor(1.f, 1.f, 0.f, 1.f); // yellow
    glClearColor(0.f, 0.f, 0.f, 1.f); //black
    glClearDepthf(1.f);

    Log("Initialized GLES state");

    //NOTE: just for text blending - this should be turned off otherwise
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepthf(1.f);

    //glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

inline
Vec2<float> DrawMemoryStats(GlText *glText, Vec2<float> textBaseline, Vec2<float> lineAdvance) {
#ifdef ENABLE_MEMORY_STATS
    glText->PushString(textBaseline, "Memory Bytes: %u | Blocks: %u | Reserve Blocks: %u",
                       Memory::memoryBytes, Memory::memoryBlockCount,
                       Memory::memoryBlockReserveCount);
    textBaseline += lineAdvance;

    glText->PushString(textBaseline, "Memory Unused Bytes: %u | Reserve Bytes: %u | Pad Bytes: %u",
                       Memory::memoryUnusedBytes, Memory::memoryBlockReservedBytes,
                       Memory::memoryPadBytes);
    textBaseline += lineAdvance;
#endif

    return textBaseline;
}

inline
Vec2<float> DrawCoordinates(GlText* glText, Vec3<float> coordinates, Vec2<float> textBaseline, Vec2<float> lineAdvance) {
    glText->PushString(textBaseline, "Coordinates: (%7.3f, %7.3f, %7.3f)",
                       coordinates.x, coordinates.y, coordinates.z);
    textBaseline += lineAdvance;
    return textBaseline;
}



Vec2<float> DrawFPS(GlText *glText, float renderTime, float frameTime,
                    Vec2<float> textBaseline, Vec2<float> lineAdvance) {
    constexpr uint32 kAverageFrames = CeilFraction(500, kTargetMsFrameTime); //target ~500 ms

    struct FrameInfo {
        float renderTime, frameTime,
                realFPS, maxFPS;
    };

    //Note frameinfo is a ring buffer
    static uint32 frameIndex;
    static FrameInfo frameInfo[kAverageFrames];
    static FrameInfo cumulativeInfo;

    float realFPS = 1.f / frameTime,
            maxFPS = 1.f / renderTime;

    //remove the stale frame from the average
    //TODO: think of way of mitigating floating point error - store cumulative time in the last frame and set that
    //      YOU CAN TEST THE FLOATING ROUNDING ERROR by commenting out loopTimer.SleepLapMs in for loop and watching renderTime decay
    cumulativeInfo.renderTime -= frameInfo[frameIndex].renderTime;
    cumulativeInfo.frameTime -= frameInfo[frameIndex].frameTime;
    cumulativeInfo.realFPS -= frameInfo[frameIndex].realFPS;
    cumulativeInfo.maxFPS -= frameInfo[frameIndex].maxFPS;

    //add our frame metrics to cumulative
    cumulativeInfo.renderTime += renderTime;
    cumulativeInfo.frameTime += frameTime;
    cumulativeInfo.realFPS += realFPS;
    cumulativeInfo.maxFPS += maxFPS;

    //cache frame info
    frameInfo[frameIndex] = {
            .renderTime = renderTime,
            .frameTime  = frameTime,
            .realFPS    = realFPS,
            .maxFPS     = maxFPS,
    };

    //increment/wrap the index
    frameIndex = (frameIndex < kAverageFrames - 1) ? frameIndex + 1 : 0;

    //compute average - TODO: correct for initial buffer being filled with zeros and artificially low average
    constexpr float avgNormalizer = 1.f / kAverageFrames;
    float avgRenderTime = avgNormalizer * cumulativeInfo.renderTime,
            avgFrameTime = avgNormalizer * cumulativeInfo.frameTime,
            avgMaxFPS = avgNormalizer * cumulativeInfo.maxFPS,
            avgRealFPS = avgNormalizer * cumulativeInfo.realFPS;

    glText->PushString(textBaseline,
                       "frameTime: %03.3fms | renderTime: %03.3fms | FPS: %03.3f | maxFPS: %03.3f",
                       1E3f * avgFrameTime, 1E3f * avgRenderTime, avgRealFPS, avgMaxFPS);

    textBaseline += lineAdvance;

    return textBaseline;
}

inline
void DrawStrings(GlText *glText, Vec3<float> coordinates,
                 float renderTime, float frameTime,
                 Vec2<float> textBaseline, Vec2<float> lineAdvance) {

    textBaseline = DrawMemoryStats(glText, textBaseline, lineAdvance);
    textBaseline = DrawFPS(glText, renderTime, frameTime, textBaseline, lineAdvance);
    textBaseline = DrawCoordinates(glText, coordinates, textBaseline, lineAdvance);

    glText->Draw();
    glText->Clear();
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

void *activityLoop(void *params_) {

    RenderThreadParams *params = (RenderThreadParams *) params_;

    //Intialize EGL on current thread
    glContext.Init(params->androidNativeWindow);

    // setup ARCore
    ARWrapper::Instance()->UpdateScreenSize(glContext.Width(), glContext.Height());

    GlText glText(&glContext, "fonts/xolonium_regular.ttf");
    glText.RenderTexture(GlText::RenderParams{
            .targetGlyphSize = 25,
            .renderStringAttrib = {.rgba = RGBA(1.f, 0, 0)},
    });

    InitGlesState();

    //GlCamera backCamera(Mat4<float>::Orthogonal(Vec2<float>(glContext.Width(), glContext.Height())*.001f , 0, 2000));
    //GlCamera backCamera(Mat4<float>::Orthogonal(Vec2<float>(glContext.Width(), glContext.Height()), 0, 2000)); //TODO: see if we can replace scaling view with scaling camera so glSkymap draws properly
    //GlCamera backCamera(Mat4<float>::Perspective((float)glContext.Width()/glContext.Height(), ToRadians(85.f), 0.01f, 2000.f), GlTransform(Vec3(0.f, 0.f, 1.f)));
    GlCamera backCamera, frontCamera;

    //Note: bind camera texture to arCore
    //Warn: Order dependent.
    //      We must call SetEglCameraTexture before we update the frame
    //      we also only want to query the ArWrapper ProjectionMatrix after the frame is updated
    ARWrapper::Instance()->SetEglCameraTexture(backCamera.EglTexture());
    backCamera.SetTransform(ARWrapper::Instance()->UpdateFrame());
    backCamera.SetProjectionMatrix(ARWrapper::Instance()->ProjectionMatrix(.01f, 1000.f));
    glm::mat4 projection_matrix = ARWrapper::Instance()->ProjectionMatrix_glm(0.1f, 1000.f);

    {
        //Camera camera(Camera::FRONT_CAMERA);
    }

    const Vec3 omega = ToRadians(Vec3(0.f, 0.f, 0.f));
    const float mirrorOmega = ToRadians(180.f / 10.f);

    GlSkybox skybox({

                            //.posX = "textures/skymap/px.png",
                            //.negX = "textures/skymap/nx.png",
                            //.posY = "textures/skymap/py.png",
                            //.negY = "textures/skymap/ny.png",
                            //.posZ = "textures/skymap/pz.png",
                            //.negZ = "textures/skymap/nz.png",

                            .posX = "textures/uvGrid.png",
                            .negX = "textures/uvGrid.png",
                            .posY = "textures/uvGrid.png",
                            .negY = "textures/uvGrid.png",
                            .posZ = "textures/uvGrid.png",
                            .negZ = "textures/uvGrid.png",

                            //.posX = "textures/debugTexture.png",
                            //.negX = "textures/debugTexture.png",
                            //.posY = "textures/debugTexture.png",
                            //.negY = "textures/debugTexture.png",
                            //.posZ = "textures/debugTexture.png",
                            //.negZ = "textures/debugTexture.png",

                            .camera = &backCamera,
                            .generateMipmaps = true, //Note: used for object roughness parameter
                    });

//    GlObject sphere("meshes/cow.obj",
//                    &backCamera,
//                    &skybox,
//                    GlTransform(Vec3(0.f, 0.f, -1.f), Vec3(.03f, .03f, .03f))
//                    );

    GlObject sphere("meshes/sphere.obj",
                    &backCamera,
                    &skybox,
                    GlTransform(Vec3(0.f, 0.f, -.5f), Vec3(.1f, .1f, .1f))
            //GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(.1f, .1f, .1f))
    );

    BackgroundRenderer background_renderer;
    GLuint depth_tex = 0;
    background_renderer.InitializeGlContent(FileManager::assetManager, depth_tex);


    // TODO: Abstract out into separate class
    {
    }

    //GlObject sphere("meshes/triangle.obj",
    //                &backCamera,
    //                &skybox,
    //                GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(.2f, .2f, .2f))
    //               );
    PlaneRenderer plane_renderer;
    int32_t plane_count;
    plane_renderer.InitializeGlContent(FileManager::assetManager);

    PointCloudRenderer point_cloud_renderer;
    point_cloud_renderer.InitializeGlContent(FileManager::assetManager);
    Timer fpsTimer(true), physicsTimer(true);

    for (Timer loopTimer(true);; loopTimer.SleepLapMs(kTargetMsFrameTime)) {

        float secElapsed = physicsTimer.LapSec();

        backCamera.SetTransform(ARWrapper::Instance()->UpdateFrame());
        ArSession* arSession = ARWrapper::Instance()->arSession;
        ArFrame* arFrame = ARWrapper::Instance()->arFrame;
        glm::mat4 view_matrix = ARWrapper::Instance()->ViewMatrix();

        // TODO: POLL ANDROID MESSAGE LOOP
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //udate skybox
        {

            //rotate camera
            if (false) {
                GlTransform transform = backCamera.GetTransform();

                static float totalTime = 0.f;
                totalTime += secElapsed;
                float theta = ToRadians(10.f) * totalTime;
                while (theta >= 2. * Pi()) theta -= 2. * Pi();

                transform.position = Vec3(FastCos(theta), 0.f, FastSin(theta)) * .3f;

                Quaternion<float> rotation = Quaternion<float>::LookAt(transform.position,
                                                                       sphere.GetTransform().position);
                transform.SetRotation(rotation);

                backCamera.SetTransform(transform);
            }


            Timer cameraTimer(true);
            int cameraInvocations = 1; // TODO: set to 100 for benchmarking
            for (int i = 0; i < cameraInvocations; ++i) {
                skybox.UpdateTexture(&glContext);
                glFinish();
            }

            float cameraMs = cameraTimer.ElapsedMs();
            glText.PushString(Vec3(10.f, 500.f, 0.f), "CameraMs: %f (%f ms per invokation)",
                              cameraMs, cameraMs / cameraInvocations);

            skybox.Draw();
        }

        //backCamera.Draw();
        //background_renderer.Draw(arSession, arFrame, false, backCamera);

        // Update and render point cloud.
        ArPointCloud* ar_point_cloud = nullptr;
        ArStatus point_cloud_status = ArFrame_acquirePointCloud(arSession, arFrame, &ar_point_cloud);
        if (point_cloud_status == AR_SUCCESS) {
            point_cloud_renderer.Draw(projection_matrix * view_matrix, arSession,
                                       ar_point_cloud);
            ArPointCloud_release(ar_point_cloud);
        }

        {
            ArTrackableList* plane_list = nullptr;
            ArTrackableList_create(arSession, &plane_list);
            assert(plane_list != nullptr);

            ArTrackableType plane_tracked_type = AR_TRACKABLE_PLANE;
            ArSession_getAllTrackables(arSession, plane_tracked_type, plane_list);

            int32_t plane_list_size = 0;
            ArTrackableList_getSize(arSession, plane_list, &plane_list_size);
            plane_count = plane_list_size;

            for (int i = 0; i < plane_list_size; ++i) {
                ArTrackable* ar_trackable = nullptr;
                ArTrackableList_acquireItem(arSession, plane_list, i, &ar_trackable);
                ArPlane* ar_plane = ArAsPlane(ar_trackable);
                ArTrackingState out_tracking_state;
                ArTrackable_getTrackingState(arSession, ar_trackable,
                                             &out_tracking_state);

                //TODO: TURN THIS OPTIMIZATION BACK ON WHEN WE FIGURE OUT HOW To FIX THE BUG
                // ArPlane* subsume_plane;
                // ArPlane_acquireSubsumedBy(arSession, ar_plane, &subsume_plane);
                // if (subsume_plane != nullptr) {
                //     ArTrackable_release(ArAsTrackable(subsume_plane));
                //     ArTrackable_release(ar_trackable);
                //     continue;
                // }

                if (ArTrackingState::AR_TRACKING_STATE_TRACKING != out_tracking_state) {
                    ArTrackable_release(ar_trackable);
                    continue;
                }

                //plane_renderer.Draw(projection_matrix, view_matrix, *arSession, *ar_plane);
                ArTrackable_release(ar_trackable);
            }

            //TODO: DEBUG CODE
            //Log("%d", plane_list_size);

            ArTrackableList_destroy(plane_list);
            plane_list = nullptr;
        }

        //Update sphere
        {
            //GlTransform transform = sphere.GetTransform();
            //transform.Rotate(omega * secElapsed);
            //sphere.SetTransform(transform);
//            static float mirrorTheta = 0.f;
//            mirrorTheta += mirrorOmega * secElapsed;

            //float r = .5f*(FastSin(mirrorTheta)+1.f);
            float r = .5f;

            //sphere.Draw(r);
        }

        Vec3<float> coordinates = backCamera.GetTransform().position;
//        Log("Coordinates: (%7.3f, %7.3f, %7.3f)",
//                    coordinates.x, coordinates.y, coordinates.z);
//        InitGlesState();
//        DrawStrings(&glText,
//                    backCamera.GetTransform().position,
//                    loopTimer.ElapsedSec(),
//                    fpsTimer.LapSec(),
//                    Vec2(50.f, 50.f), //textBaseline
//                    Vec2(0.f, 50.f)   //textAdvance
//        );

        // present back buffer
        if (!glContext.SwapBuffers()) InitGlesState();
    }

    return nullptr;
}

#pragma clang diagnostic pop

extern "C" {

void JFunc(App, NativeStartApp)
(JNIEnv *jniEnv, jclass clazz, jobject surface, jobject jAssetManager, jobject jActivity) {

    RUNTIME_ASSERT(surface, "Surface Is NULL!");

    FileManager::Init(jniEnv, jAssetManager);

    ARWrapper::Instance()->InitializeARWrapper(jniEnv, jActivity);

    ANativeWindow *androidNativeWindow = ANativeWindow_fromSurface(jniEnv, surface);

    // Spin off render thread
    pthread_t thread;
    pthread_attr_t threadAttribs;
    RUNTIME_ASSERT(!pthread_attr_init(&threadAttribs), "Failed to Init threadAttribs");

    static RenderThreadParams renderThreadParams;
    renderThreadParams = {.androidNativeWindow = androidNativeWindow};
    pthread_create(&thread, &threadAttribs, activityLoop, &renderThreadParams);
}

void JFunc(App, NativeSurfaceRedraw)(JNIEnv *env, jclass clazz,
                                     int rotation, int width, int height) {

    RUNTIME_ASSERT(width == glContext.Width(),
                   "Changing Width not implemented yet! { oldWidth: %d, newWidth: %d }",
                   glContext.Width(), width);

    RUNTIME_ASSERT(height == glContext.Height(),
                   "Changing Height not implemented yet! { oldHeight: %d, newHeight: %d }",
                   glContext.Height(), height);

    Log("Updating context { rotation: %d, width: %d, height: %d }", rotation, width, height);

    //ARWrapper::Get()->arSession->
    //setDisplayGeometry(rotation, width, height);
}

// TODO: Func to release native window context
// TODO: func to resize egl context
// TODO: funcs to pause and resume rendering!
}
extern "C"
JNIEXPORT void JNICALL
Java_com_eecs487_jniteapot_App_onTouched(JNIEnv *env, jclass clazz, jlong native_application,
                                         jfloat x, jfloat y) {
    Log("touched");
}  