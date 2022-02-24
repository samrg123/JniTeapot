#include <jni.h>
#include <pthread.h>

#include "tests.h"

#include "util.h"

#include "FileManager.h"
#include "GlContext.h"
#include "GlText.h"
#include "Timer.h"

#include "Memory.h"

#include "GlObject.h"
#include "GlSkybox.h"

#include "ARWrapper.h"

//NOTE: NOT IN REPO YET
// #include "ArPlanes.h"
// #include "Camera.h"

#include <android/native_window_jni.h>

#define JFunc(jClass, jMethod) JNIEXPORT JNICALL Java_com_eecs487_jniteapot_##jClass##_ ##jMethod

//TODO: package this up somewhere
GlContext glContext;

constexpr int kTargetFPS = 60;
constexpr float kTargetMsFrameTime = 1000.f/kTargetFPS;

struct RenderThreadParams {
    ANativeWindow* androidNativeWindow;
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
Vec2<float> DrawMemoryStats(GlText* glText, Vec2<float> textBaseline, Vec2<float> lineAdvance) {
    #ifdef ENABLE_MEMORY_STATS
        glText->PushString(textBaseline, "Memory Bytes: %u | Blocks: %u | Reserve Blocks: %u", Memory::memoryBytes, Memory::memoryBlockCount, Memory::memoryBlockReserveCount);
        textBaseline+= lineAdvance;
        
        glText->PushString(textBaseline, "Memory Unused Bytes: %u | Reserve Bytes: %u | Pad Bytes: %u", Memory::memoryUnusedBytes, Memory::memoryBlockReservedBytes,  Memory::memoryPadBytes);
        textBaseline+= lineAdvance;
    #endif
    
    return textBaseline;
}

Vec2<float> DrawFPS(GlText* glText, float renderTime, float frameTime,
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
    float   avgRenderTime     = avgNormalizer * cumulativeInfo.renderTime,
            avgFrameTime      = avgNormalizer * cumulativeInfo.frameTime,
            avgMaxFPS         = avgNormalizer * cumulativeInfo.maxFPS,
            avgRealFPS        = avgNormalizer * cumulativeInfo.realFPS;
    
    glText->PushString(textBaseline,
                       "frameTime: %03.3fms | renderTime: %03.3fms | FPS: %03.3f | maxFPS: %03.3f",
                       1E3f*avgFrameTime, 1E3f*avgRenderTime, avgRealFPS, avgMaxFPS);

    textBaseline+= lineAdvance;
    
    return textBaseline;
}

inline
void DrawStrings(GlText* glText, float renderTime, float frameTime,
                 Vec2<float> textBaseline, Vec2<float> lineAdvance) {

    textBaseline = DrawMemoryStats(glText, textBaseline, lineAdvance);
    textBaseline = DrawFPS(glText, renderTime, frameTime, textBaseline, lineAdvance);
    
    glText->Draw();
    glText->Clear();
}

inline
void UpdateFrameCamera(GlCamera& camera) {
    ARWrapper::UpdateFrameResult frameResult = ARWrapper::Instance()->UpdateFrame();
    
    //TODO: output this to the screen can use green/yellow/red letters for tracking state

    switch(frameResult.trackingState) {

        case AR_TRACKING_STATE_TRACKING: {

            camera.SetTransform(frameResult.frameTransform);

        } break;
    
        case AR_TRACKING_STATE_PAUSED: {

            Warn("Skipping camera transform update - Tracking paused");

        } break;

        default: {

            //TODO: turn this into some crazy metaprogramming enum reflection thing that can
            //      do a quick hash map lookup to map enum value to enum name... ideally the
            //      map would be generated with something like GenerateEnumNameMap<ArTrackingFailureReason>();

            const char* failureString;
            switch(frameResult.trackingFailureReason) {
                
                case AR_TRACKING_FAILURE_REASON_BAD_STATE: {
                    failureString = "AR_TRACKING_FAILURE_REASON_BAD_STATE";
                } break;
                
                case AR_TRACKING_FAILURE_REASON_CAMERA_UNAVAILABLE: {
                    failureString = "AR_TRACKING_FAILURE_REASON_CAMERA_UNAVAILABLE";
                } break;
                
                case AR_TRACKING_FAILURE_REASON_EXCESSIVE_MOTION: {
                    failureString = "AR_TRACKING_FAILURE_REASON_EXCESSIVE_MOTION";
                } break;
                
                case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_FEATURES: {
                    failureString = "AR_TRACKING_FAILURE_REASON_INSUFFICIENT_FEATURES";
                } break;
                
                case AR_TRACKING_FAILURE_REASON_INSUFFICIENT_LIGHT: {
                    failureString = "AR_TRACKING_FAILURE_REASON_INSUFFICIENT_LIGHT";
                } break;
                
                case AR_TRACKING_FAILURE_REASON_NONE: {
                    failureString = "AR_TRACKING_FAILURE_REASON_NONE";
                } break;

                default: {
                    failureString = "FAILED_TO_PARSE";
                }
            }

            Warn("Bad tracking state %d - trackingFailureReason %d [%s]", 
                 frameResult.trackingState,
                 frameResult.trackingFailureReason, failureString);
            
        } break;
    }
}

[[noreturn]] void* activityLoop(void* params_) {

    RenderThreadParams* params = (RenderThreadParams*) params_;

    //Initialize EGL on current thread
    glContext.Init(params->androidNativeWindow);
        
    //Set ArCore screen geometry
    ARWrapper::Instance()->UpdateScreenSize(glContext.Width(), glContext.Height());

    //setup GlText
    GlText glText(&glContext, "fonts/xolonium_regular.ttf");
    glText.RenderTexture(GlText::RenderParams {
        .targetGlyphSize = 25,
        .renderStringAttrib = { .rgba = RGBA(1.f, 0, 0) },
    });
    
    InitGlesState();
    
    //GlCamera backCamera(Mat4<float>::Orthogonal(Vec2<float>(glContext.Width(), glContext.Height())*.001f , 0, 2000));
    //GlCamera backCamera(Mat4<float>::Orthogonal(Vec2<float>(glContext.Width(), glContext.Height()), 0, 2000)); //TODO: see if we can replace scaling view with scaling camera so glSkymap draws properly
    //GlCamera backCamera(Mat4<float>::Perspective((float)glContext.Width()/glContext.Height(), ToRadians(85.f), 0.01f, 2000.f), GlTransform(Vec3(0.f, 0.f, 1.f)));
    GlCamera backCamera(glContext.Width(), glContext.Height()), 
             frontCamera(glContext.Width(), glContext.Height());
    
    //TODO: Clean up this initialization when we pull EglTexture out into its own class ... maybe have ArWrapper return an instance of it for us?
    {
        ARWrapper::EglTextureSize eglTextureSize = ARWrapper::Instance()->GetEglTextureSize();

        Log("Using ArCore EglTexture - cpu Size: '%d x %d' | gpu size '%d x %d'",
            eglTextureSize.cpuTextureSize.x, eglTextureSize.cpuTextureSize.y,
            eglTextureSize.gpuTextureSize.x, eglTextureSize.gpuTextureSize.y);

        backCamera.SetEglTextureSize(eglTextureSize.gpuTextureSize.x, eglTextureSize.gpuTextureSize.y);
    }

    //Note: Bind camera texture to ArCore
    //Warn: Order dependent.
    //      We must call SetEglCameraTexture before we update the frame
    //      we also only want to query the ArWrapper ProjectionMatrix after the frame is updated
    ARWrapper::Instance()->SetEglCameraTexture(backCamera.EglTexture());
    UpdateFrameCamera(backCamera);
    backCamera.SetProjectionMatrix(ARWrapper::Instance()->ProjectionMatrix(.01f, 1000.f));
    
    // TODO: Get Camera working with front/back camera and let ARWrapper switch between them so we can update both sides of 
    //      the cubemap in the same frame
    // {
    //     Camera camera(Camera::FRONT_CAMERA);
    // }
       
    const Vec3 omega = ToRadians(Vec3(0.f, 0.f, 0.f));
    const float mirrorOmega = ToRadians( 180.f / 10.f);

    //Setup cubemap skybox    
    GlSkybox skybox(GlSkybox::SkyboxParams {

        .cubemap = {

            // .posX = "textures/skymap/px.png",
            // .negX = "textures/skymap/nx.png",
            // .posY = "textures/skymap/py.png",
            // .negY = "textures/skymap/ny.png",
            // .posZ = "textures/skymap/pz.png",
            // .negZ = "textures/skymap/nz.png",

            .posX = "textures/uvGrid.png",
            .negX = "textures/uvGrid.png",
            .posY = "textures/uvGrid.png",
            .negY = "textures/uvGrid.png",
            .posZ = "textures/uvGrid.png",
            .negZ = "textures/uvGrid.png",
    
            // .posX = "textures/debugTexture.png",
            // .negX = "textures/debugTexture.png",
            // .posY = "textures/debugTexture.png",
            // .negY = "textures/debugTexture.png",
            // .posZ = "textures/debugTexture.png",
            // .negZ = "textures/debugTexture.png",
        },

        .camera = &backCamera,
        .generateMipmaps = true, //Note: used for object roughness parameter
    });

    //Setup object to render
    // GlObject sphere("meshes/cow.obj",
    //                 &backCamera,
    //                 &skybox,
    //                 GlTransform(Vec3(0.f, 0.f, -1.f), Vec3(.03f, .03f, .03f))
    //                 );

    GlObject sphere("meshes/sphere.obj",
                   &backCamera,
                   &skybox,
                   GlTransform(Vec3(0.f, 0.f, -.5f), Vec3(.1f, .1f, .1f))
                   //GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(.1f, .1f, .1f))
                  );
    
    //GlObject sphere("meshes/triangle.obj",
    //                &backCamera,
    //                &skybox,
    //                GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(.2f, .2f, .2f))
    //               );
    
    Timer fpsTimer(true),
          physicsTimer(true);
    
    for(Timer loopTimer(true) ;; loopTimer.SleepLapMs(kTargetMsFrameTime) ) {

        float secElapsed = physicsTimer.LapSec();
        
        // TODO: POLL ANDROID MESSAGE LOOP FOR KEY EVENTS

        //Update camera to match current ArCore position
        UpdateFrameCamera(backCamera);
        
        //clear last frame color and depth buffer
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
        //update skybox
        {
            //rotate camera
            if(false)
            {
                GlTransform transform = backCamera.GetTransform();
                
                static float totalTime = 0.f;
                totalTime+= secElapsed;
                float theta = ToRadians(10.f)*totalTime;
                while(theta >= 2.*Pi()) theta-= 2.*Pi();
                
                transform.position = Vec3(FastCos(theta), 0.f, FastSin(theta)) * .3f;
                
                Quaternion<float> rotation = Quaternion<float>::LookAt(transform.position, sphere.GetTransform().position);
                transform.SetRotation(rotation);
                
                backCamera.SetTransform(transform);
            }
            

            Timer cameraTimer(true);
            int cameraInvocations = 1; // TODO: set to 100 for benchmarking
            for(int i = 0; i < cameraInvocations; ++i) {
                skybox.UpdateTexture(&glContext);
                glFinish();
            }

            float cameraMs = cameraTimer.ElapsedMs();
            glText.PushString(Vec3(10.f, 500.f, 0.f), "CameraMs: %f (%f ms per invocation)", cameraMs, cameraMs/cameraInvocations);

            // skybox.Draw();
        }

        backCamera.Draw();
        
        //Update object
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

        DrawStrings(&glText,
                    loopTimer.ElapsedSec(),
                    fpsTimer.LapSec(),
                    Vec2(50.f, 50.f), //textBaseline
                    Vec2(0.f, 50.f)   //textAdvance
                );
        
        // present back buffer
        if(!glContext.SwapBuffers()) InitGlesState();
    }
}

extern "C" {

    void JFunc(App, NativeStartApp)(JNIEnv* jniEnv, jclass clazz, jobject surface, jobject jAssetManager, jobject jActivity) {
        
        RUNTIME_ASSERT(surface, "Surface Is NULL!");

        FileManager::Init(jniEnv, jAssetManager);
    
        ARWrapper::Instance()->InitializeARWrapper(jniEnv, jActivity);
        
        ANativeWindow* androidNativeWindow = ANativeWindow_fromSurface(jniEnv, surface);
        
        // Spin off render thread
        pthread_t thread;
        pthread_attr_t threadAttribs;
        RUNTIME_ASSERT(!pthread_attr_init(&threadAttribs), "Failed to Init threadAttribs");

        static RenderThreadParams renderThreadParams;
        renderThreadParams = { .androidNativeWindow = androidNativeWindow };
        pthread_create(&thread, &threadAttribs, activityLoop, &renderThreadParams);
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
