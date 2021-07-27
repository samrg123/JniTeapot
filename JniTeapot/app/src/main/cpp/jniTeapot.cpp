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
#include "ShadowMap.h"
#include "Texture.h"
#include <gtc/matrix_transform.hpp>
#include "Light.h"

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
Vec2<float> DrawCoordinates(GlText *glText, Vec3<float> coordinates, Vec2<float> textBaseline,
                            Vec2<float> lineAdvance) {
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

[[noreturn]] void *activityLoop(void *params_) {

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

    GlObject sphere("meshes/sphere.obj",
                    &backCamera,
                    &skybox,
                    GlTransform(Vec3(0.f, 0.f, -1.f), Vec3(.05f, .05f, .05f))
    );

    GlObject plane("meshes/plane.obj",
                   &backCamera,
                   &skybox,
                   GlTransform(Vec3(1.f, 0.0f, -1.0f), Vec3(.1f, .1f, .1f))
    );

    PlaneRenderer plane_renderer;
    int32_t plane_count;
    plane_renderer.InitializeGlContent(FileManager::assetManager);

    PointCloudRenderer point_cloud_renderer;
    point_cloud_renderer.InitializeGlContent(FileManager::assetManager);

    ShadowMap::Init_Shaders(FileManager::assetManager);

    std::vector<QuadLight> quad_lights;
    quad_lights.emplace_back(glm::vec3(-0.25, 0.5, -0.25), glm::vec3(0, 0, 0.5),
                             glm::vec3(0.5, 0, 0), glm::vec3(1,1, 1));
//    quad_lights.emplace_back(glm::vec3(-0.125, 0.5, -0.125), glm::vec3(0, 0, 0.25),
//                             glm::vec3(0.25, 0, 0), glm::vec3(1,1, 1));
    // 17 14 12
//    quad_lights.emplace_back(glm::vec3(-0.7,0.0,-0.7), glm::vec3(0,0,1.4), glm::vec3(0,1.4,0), glm::vec3(1,0,0));
//    quad_lights.emplace_back(glm::vec3(0.25,0,-0.25), glm::vec3(0,0,0.5), glm::vec3(0,0.5,0), glm::vec3(0,1,0));
//    quad_lights.emplace_back(glm::vec3(-0.25,0.5,-0.25), glm::vec3(0,0,0.5), glm::vec3(0.5,0,0), glm::vec3(1,1,1));
    for (auto &light : quad_lights) {
        light.init_shadow_map(1024, 1024);
        light.update_light_space();
    }

    LightRenderer light_renderer;
    light_renderer.init_gl(FileManager::assetManager);
    light_renderer.update_verts(quad_lights.data(), quad_lights.size());

    Timer fpsTimer(true), physicsTimer(true);

    glm::mat4 sphere_transform(1);
    glm::mat4 plane_transform(1);
    plane_transform = glm::scale(plane_transform, glm::vec3(0.7, 0.7, 0.7));
    plane_transform = glm::translate(plane_transform, glm::vec3(0.0, -0.5, 0.0));
    sphere_transform = glm::scale(sphere_transform, glm::vec3(0.1, 0.1, 0.1));
    sphere_transform = glm::translate(sphere_transform, glm::vec3(0, -1, 0));

    glm::mat4 lights_transform(1);
//    lights_transform = glm::translate(lights_transform, glm::vec3(0.0, 0.0, 0.0));

    GLuint object_shader = util::CreateProgram("shaders/object.vert", "shaders/object.frag",
                                               FileManager::assetManager);

    Texture brick_texture;
    brick_texture.create();
    brick_texture.load_image("textures/brick.png", true);
    brick_texture.configure_params(true, false, true);

    for (Timer loopTimer(true);; loopTimer.SleepLapMs(kTargetMsFrameTime)) {
        float secElapsed = physicsTimer.LapSec();

        backCamera.SetTransform(ARWrapper::Instance()->UpdateFrame());
//        ArSession* arSession = ARWrapper::Instance()->arSession;
//        ArFrame* arFrame = ARWrapper::Instance()->arFrame;
        glm::mat4 view_matrix = ARWrapper::Instance()->ViewMatrix();

        // TODO: POLL ANDROID MESSAGE LOOP
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // first render depth of scene to shadow map

        //udate skybox
        {
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

        {
            // render scene to shadow maps
            for (auto &light : quad_lights) {
                auto& map = light.shadow_map;
                map.configure();
                map.set_model(sphere_transform);
                sphere.Draw();
                map.set_model(plane_transform);
                plane.Draw();
            }
            FBO::use_defualt();
//            quad_lights[0].shadow_map.render_debug_quad();
            glViewport(0, 0, glContext.Width(), glContext.Height());
        }
        // visualize shadow map

        {
            // render scene
            auto& light = quad_lights[0];
            auto& shadow_map = quad_lights[0].shadow_map;
            glUseProgram(object_shader);
            util::SetVec3(object_shader, "uLightDir", glm::vec3(0, 1, 0));
            util::SetMat4(object_shader, "projection", projection_matrix);
            util::SetMat4(object_shader, "view", view_matrix);
            util::SetMat4(object_shader, "lightSpace", shadow_map.light_space);
            util::SetMat4(object_shader, "uLightView", shadow_map.light_view);

            util::SetInt(object_shader, "uEnvMap", GlObject::TextureUnits::TU_SKY_MAP);
            shadow_map.depth_tex.bind(1, glGetUniformLocation(object_shader, "uShadowMap"));

            util::SetMat4(object_shader, "model", plane_transform);
            util::SetVec4(object_shader, "uColor", glm::vec4(0, 0, 0, 0));
            brick_texture.bind(2, glGetUniformLocation(object_shader, "uAlbedo"));
            plane.Draw();

            util::SetMat4(object_shader, "model", sphere_transform);
            util::SetVec4(object_shader, "uColor", glm::vec4(1, 0.8, 0, 0));
            sphere.Draw();
        }
        light_renderer.render_lights(projection_matrix, view_matrix, lights_transform);

        Vec3<float> coordinates = backCamera.GetTransform().position;
        Log("Coordinates: (%7.3f, %7.3f, %7.3f)",
            coordinates.x, coordinates.y, coordinates.z);
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