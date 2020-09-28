#include <jni.h>
#include <pthread.h>

#include "log.h"
#include "panic.h"
#include "customAssert.h"

#include "FileManager.h"
#include "GlContext.h"
#include "GlText.h"
#include "Timer.h"

#include "Memory.h"

#include <android/native_window_jni.h>

#define JFunc(jClass, jMethod) JNIEXPORT JNICALL Java_com_eecs487_jniteapot_##jClass##_ ##jMethod

constexpr float kTargetMsFrameTime = 1000.f/60;

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
        //      YOU CAN TEST THE FLOATING ROUNDING ERROR by comenting out loopTimer.SleepLapMs in for loop and watching renderTime decay
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
void* activityLoop(void* nativeWindow) {
    
    GlContext glContext;
    glContext.Init((ANativeWindow*)nativeWindow);
    
    GlText glText(&glContext, "fonts/xolonium_regular.ttf");
    glText.RenderTexture(GlText::RenderParams {
        .targetGlyphSize = 25,
        .renderStringAttrib = { .rgba = RGBA(1.f, 0, 0) },
    });
    
    InitGlesState();

    for(Timer loopTimer(true), fpsTimer(true) ;; loopTimer.SleepLapMs(kTargetMsFrameTime) ) {

        // TODO: POLL ANDROID MESSAGE LOOP

        // TODO: render TEAPOT!
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        //glContext.Draw();
        
        DrawStrings(&glText, loopTimer.ElapsedSec(), fpsTimer.LapSec());
        
        // present back buffer
        if(!glContext.SwapBuffers()) InitGlesState();
    }

    return nullptr;
}
#pragma clang diagnostic pop

extern "C" {

    void JFunc(App, NativeOnSurfaceCreated)(JNIEnv* env, jclass clazz,
                                            jobject surface, jobject jAssetManager) {
        Log("Hello World from JNI!");
	    
        RUNTIME_ASSERT(surface, "Surface Is NULL!");

        FileManager::Init(env, jAssetManager);
        
        // Spin off render thread
        pthread_t thread;
        pthread_attr_t threadAttribs;
        RUNTIME_ASSERT(!pthread_attr_init(&threadAttribs), "Failed to Init threadAttribs");

        ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
        pthread_create(&thread, &threadAttribs, activityLoop, nativeWindow);
    }

    // TODO: Func to release native window context
    // TODO: func to resize egl context
    // TODO: funcs to pause and resume rendering!
}