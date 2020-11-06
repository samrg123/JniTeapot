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
#include "GlSkybox.h"
#include "ARWrapper.h"

#include <android/native_window_jni.h>

#define JFunc(jClass, jMethod) JNIEXPORT JNICALL Java_com_eecs487_jniteapot_##jClass##_ ##jMethod

GlContext glContext;

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
    glDepthFunc(GL_LEQUAL);
    glClearDepthf(1.f);
    
    //glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
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
    
    //TODO: right now on startup ArCore gives us a distorted projection matrix? Maybe query the fov and just make our own?
    //GlCamera camera(Mat4<float>::Orthogonal(Vec2<float>(glContext.Width(), glContext.Height())*.001f , 0, 2000));
    //GlCamera camera(Mat4<float>::Orthogonal(Vec2<float>(glContext.Width(), glContext.Height()), 0, 2000)); //TODO: see if we can replace scaling view with scaling camera so glSkymap draws properly
    //GlCamera camera(Mat4<float>::Perspective((float)glContext.Width()/glContext.Height(), ToRadians(85.f), 0.01f, 2000.f));
    GlCamera camera(ARWrapper::Get()->ProjectionMatrix(.01f, 1000.f));
    
    //const Vec3 omega = ToRadians(Vec3(0.f, 10.f, 0.f));
    //const Vec3 omega = ToRadians(Vec3(10.f, 10.f, 10.f));
    const Vec3 omega = ToRadians(Vec3(0.f, 0.f, 0.f));
    const float mirrorOmega = ToRadians( 180.f / 10.f);
    
    //GlObject sphere("meshes/triangle.obj",
    //                &camera,
    //                GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(110.f, 110.f, 110.f))
    //               );
    
    GlSkybox skybox({
        //.posX = "textures/skymap/px.png",
        //.negX = "textures/skymap/nx.png",
        //.posY = "textures/skymap/py.png",
        //.negY = "textures/skymap/ny.png",
        //.posZ = "textures/skymap/pz.png",
        //.negZ = "textures/skymap/nz.png",
        
        //.posX = "textures/uvGrid.png",
        //.negX = "textures/uvGrid.png",
        //.posY = "textures/uvGrid.png",
        //.negY = "textures/uvGrid.png",
        //.posZ = "textures/uvGrid.png",
        //.negZ = "textures/uvGrid.png",
    
        .posX = "textures/debugTexture.png",
        .negX = "textures/debugTexture.png",
        .posY = "textures/debugTexture.png",
        .negY = "textures/debugTexture.png",
        .posZ = "textures/debugTexture.png",
        .negZ = "textures/debugTexture.png",

        .camera = &camera
    });
    
    const char* cubemapImages[] = {
                    "textures/skymap/px.png",
                    "textures/skymap/nx.png",
                    "textures/skymap/py.png",
                    "textures/skymap/ny.png",
                    "textures/skymap/pz.png",
                    "textures/skymap/nz.png"
                };
    GlCubemap cubemap(cubemapImages);

    //GlObject sphere("meshes/cow.obj",
    //                &camera,
    //                &cubemap,
    //                //GlTransform(Vec3(0.f, 0.f, 5.f), Vec3(1.f, 1.f, 1.f))
    //                GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(.05f, .05f, .05f))
    //                //GlTransform(Vec3(0.f, 0.f, 5.f), Vec3(100.f, 100.f, 100.f))
    //         );

    GlObject sphere("meshes/sphere.obj",
                    &camera,
                    skybox,
                    GlTransform(Vec3(0.f, 0.f, -.5f), Vec3(.1f, .1f, .1f))
                   );
    
    Timer fpsTimer(true),
          physicsTimer(true);
    
    for(Timer loopTimer(true) ;; loopTimer.SleepLapMs(kTargetMsFrameTime) ) {

        // TODO: POLL ANDROID MESSAGE LOOP
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    
        float secElapsed = physicsTimer.LapSec();
    

        //TODO: only set this when it changes!
        ARWrapper::Get()->Update(camera, cubemap);
        camera.SetProjectionMatrix(ARWrapper::Get()->ProjectionMatrix(.01f, 1000.f));
        
        //udate skybox
        {
            
            //GlTransform transform = camera.GetTransform();
            //transform.Rotate(Vec3<float>(0.f, ToRadians(10.f)*secElapsed, 0.f));
            //camera.SetTransform(transform);
            
            static int i = 0;
            //if(i%120 == 0)
            {
                //GlTransform transform = camera.GetTransform();
                //transform.SetRotation(transform.GetRotation().Rotate(Vec3<float>(0.f, ToRadians(180.f), 0.f )));
                //camera.SetTransform(transform);
                
                //skybox.UpdateTextureEGL(ARWrapper::Get()->EglCameraTexture(), &glContext);
            }
            i++;
    
            skybox.Draw();
        }
    
        ARWrapper::Get()->DrawCameraBackground(camera);
        
        //Update sphere
        {
            GlTransform transform = sphere.GetTransform();
            transform.Rotate(omega*secElapsed);
            sphere.SetTransform(transform);
    
            static float mirrorTheta = 0.f;
            mirrorTheta+= mirrorOmega*secElapsed;
    
            //float r = .5f*(FastSin(mirrorTheta)+1.f);
            float r = .5f;
            
            sphere.Draw(r);
        }

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
    
    void JFunc(App, NativeSurfaceRedraw)(JNIEnv* env, jclass clazz,
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
