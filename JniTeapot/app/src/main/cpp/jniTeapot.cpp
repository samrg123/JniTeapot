#include <jni.h>
#include <pthread.h>

#include "tests.h"

#include "log.h"
#include "panic.h"
#include "customAssert.h"

#include "FileManager.h"
#include "GlContext.h"
#include "GlText.h"
#include "Timer.h"

#include "Memory.h"

#include "GlObject.h"
#include "ARWrapper.h"

#include <android/native_window_jni.h>

#define JFunc(jClass, jMethod) JNIEXPORT JNICALL Java_com_eecs487_jniteapot_##jClass##_ ##jMethod

constexpr float kTargetMsFrameTime = 1000.f/60;

struct RenderThreadParams {
    void* nativeWindow;
    void* env;
    void* context;
    void* activity;
    JavaVM* vm;
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
    glDepthFunc(GL_LESS);
}

void DrawStrings(GlText* glText, float renderTime, float frameTime) {
    //print memory stats
    constexpr Vec2 kLineAdvance(0.f, 50.f);
    Vec2 textBaseline(50.f, 50.f);
    
    // print fps
    if constexpr(true)
    {
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
              maxFPS  = 1.f / renderTime;
        
        //remove the stale frame from the average
        //TODO: think of way of mitigating floating point error - store cumulative time in the last frame and set that
        //      YOU CAN TEST THE FLOATING ROUNDING ERROR by commenting out loopTimer.SleepLapMs in for loop and watching renderTime decay
        cumulativeInfo.renderTime-= frameInfo[frameIndex].renderTime;
        cumulativeInfo.frameTime-=  frameInfo[frameIndex].frameTime;
        cumulativeInfo.realFPS-=    frameInfo[frameIndex].realFPS;
        cumulativeInfo.maxFPS-=     frameInfo[frameIndex].maxFPS;
        
        //add our frame metrics to cumulative
        cumulativeInfo.renderTime+= renderTime;
        cumulativeInfo.frameTime+= frameTime;
        cumulativeInfo.realFPS+= realFPS;
        cumulativeInfo.maxFPS+= maxFPS;
        
        //cache frame info
        frameInfo[frameIndex] = {
            .renderTime = renderTime,
            .frameTime  = frameTime,
            .realFPS    = realFPS,
            .maxFPS     = maxFPS,
        };
        
        //increment/wrap the index
        frameIndex = (frameIndex < kAverageFrames-1) ? frameIndex+1 : 0;
        
        //compute average - TODO: correct for initial buffer being filled with zeros and artificially low average
        constexpr float avgNormalizer = 1.f/kAverageFrames;
        float avgRenderTime     = avgNormalizer * cumulativeInfo.renderTime,
              avgFrameTime      = avgNormalizer * cumulativeInfo.frameTime,
              avgMaxFPS         = avgNormalizer * cumulativeInfo.maxFPS,
              avgRealFPS        = avgNormalizer * cumulativeInfo.realFPS;
        
        //Log("renderTime: %f | maxFPS: %f | realFPS: %f | avgRenderTime: %f | avgMaxFPS: %f | avgRealFPS: %f", renderTime, maxFPS, realFPS, avgRenderTime, avgMaxFPS, avgRealFPS);
        glText->PushString(textBaseline,  "frameTime: %03.3fms | renderTime: %03.3fms | FPS: %03.3f | maxFPS: %03.3f", 1E3f*avgFrameTime, 1E3f*avgRenderTime, avgRealFPS, avgMaxFPS);
        textBaseline+= kLineAdvance;
    }
    
    #ifdef ENABLE_MEMORY_STATS && 1
        glText->PushString(textBaseline, "Memory Bytes: %u | Blocks: %u | Reserve Blocks: %u", Memory::memoryBytes, Memory::memoryBlockCount, Memory::memoryBlockReserveCount);
        textBaseline+= kLineAdvance;
    
        glText->PushString(textBaseline, "Memory Unused Bytes: %u | Reserve Bytes: %u | Pad Bytes: %u", Memory::memoryUnusedBytes, Memory::memoryBlockReservedBytes,  Memory::memoryPadBytes);
        textBaseline+= kLineAdvance;
    
    #endif
    
    glText->Draw();
    glText->Clear();
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void* activityLoop(void* _params) {
    RenderThreadParams* params = (RenderThreadParams*) _params;
    auto nativeWindow = params->nativeWindow;

    GlContext glContext;
    glContext.Init((ANativeWindow*)nativeWindow);
    
    GlText glText(&glContext, "fonts/xolonium_regular.ttf");
    glText.RenderTexture(GlText::RenderParams {
        .targetGlyphSize = 25,
        .renderStringAttrib = { .rgba = RGBA(1.f, 0, 0) },
    });

    InitGlesState();

    // setup ARCore
    ARWrapper::Get()->InitializeGlContent();
    ARWrapper::Get()->UpdateScreenSize(glContext.Width(), glContext.Height());

    
    //GlCamera camera(Mat4<float>::Orthogonal(Vec2<float>(glContext.Width(), glContext.Height()), 0, 2000),
    //                GlTransform(Vec3(0.f, 0.f, -1000.f), Vec3<float>(1.f, 1.f, 1.f))
    //                //GlTransform(Vec3<float>::zero, Vec3<float>(200.f, 200.f, 200.f))
    //);

    float fovX = ToRadians(90.f);
    GlCamera camera(Mat4<float>::Perspective((float)glContext.Width()/glContext.Height(), fovX, 0.f, 2000.f),
                        GlTransform(Vec3(0.f, 0.f, -1000.f), Vec3(1.f, 1.f, 1.f))
                    //GlTransform(Vec3<float>::zero, Vec3<float>(.1f, .1f, .1f))
                   );
    
    const Vec3 omega = ToRadians(Vec3(0.f, 10.f, 0.f));
    //const Vec3 omega = ToRadians(Vec3(0.f, 0.f, 0.f));
    const float mirrorOmega = ToRadians( 180.f / 10.f);
    
    //GlObject sphere("meshes/triangle.obj",
    //                &camera,
    //                GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(110.f, 110.f, 110.f))
    //               );

    const char* cubemapImages[] = {
                    "textures/skymap/px.png",
                    "textures/skymap/nx.png",
                    "textures/skymap/py.png",
                    "textures/skymap/ny.png",
                    "textures/skymap/pz.png",
                    "textures/skymap/nz.png"
                };
    GlCubemap cubemap(cubemapImages);

    GlObject sphere("meshes/sphere.obj",
                    &camera,
                    &cubemap,
                    GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(.05f, .05f, .05f))
             );

    //GlObject sphere("meshes/sphere.obj",
    //                &camera,
    //                GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(500.f, 500.f, 500.f))
    //               );
    
    Timer fpsTimer(true),
          physicsTimer(true);
    
    for(Timer loopTimer(true) ;; loopTimer.SleepLapMs(kTargetMsFrameTime) ) {

        // TODO: POLL ANDROID MESSAGE LOOP
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    
        float secElapsed = physicsTimer.LapSec();

        ARWrapper::Get()->Update(camera, cubemap);

        //Update sphere
        {
            GlTransform transform = sphere.GetTransform();
            transform.Rotate(omega*secElapsed);
            
            //camera.SetTransform(camera.GetTransform().Translate(Vec3(0.f, 0.f, .001f)));
            //transform.Translate(Vec3(0.f, 0.f, .001f));
            
            sphere.SetTransform(transform);
            
        }

        static float mirrorTheta = 0.f;
        mirrorTheta+= mirrorOmega*secElapsed;
        
        float r = .5f*(FastSin(mirrorTheta)+1.f);
        //float r = .5f;

        ARWrapper::Get()->DrawCameraBackground();

        sphere.Draw(1.0f);

        DrawStrings(&glText, loopTimer.ElapsedSec(), fpsTimer.LapSec());
        
        // present back buffer
        if(!glContext.SwapBuffers()) InitGlesState();
    }

    return nullptr;
}
#pragma clang diagnostic pop

extern "C" {

    void JFunc(App, NativeOnSurfaceCreated)(JNIEnv* env, jclass clazz,
                                            jobject surface, jobject jAssetManager, jobject context, jobject activity) {
        
        RUNTIME_ASSERT(surface, "Surface Is NULL!");

        FileManager::Init(env, jAssetManager);
        
        // Spin off render thread
        pthread_t thread;
        pthread_attr_t threadAttribs;
        RUNTIME_ASSERT(!pthread_attr_init(&threadAttribs), "Failed to Init threadAttribs");

        JNIEnv* jenv = (JNIEnv*) env;
        JavaVM* vm;
        jenv->GetJavaVM(&vm);

        ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
        auto* r = new RenderThreadParams{.nativeWindow = nativeWindow, .env = env, .context = context, .activity = activity, .vm = vm};

        // This needs to happen on the JNI thread or bad things happen
        ARWrapper::Get()->InitializeARSession(env, context);

        pthread_create(&thread, &threadAttribs, activityLoop, (void*)r);
    }

    // TODO: Func to release native window context
    // TODO: func to resize egl context
    // TODO: funcs to pause and resume rendering!
}
