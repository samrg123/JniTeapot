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
#include "Camera.h"

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

    //NOTE: just for text blending - this should be turned off otherwise
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepthf(1.f);
    
    //glClearColor(1.f, 1.f, 0.f, 1.f); // yellow
    glClearColor(0.f, 0.f, 0.f, 1.f); //black
    
    // glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    Log("Initialized GLES state");
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
Vec2<float> DrawTransform(GlText* glText, const GlTransform& transform,
                   Vec2<float> textBaseline, Vec2<float> lineAdvance) {

    glText->PushString(textBaseline,
                       "Coordinates: (% 04.3f, % 04.3f, % 04.3f)",
                       transform.position.x, transform.position.y, transform.position.z                       
    );

    textBaseline+= lineAdvance;

    return textBaseline;
}

inline
void DrawStrings(GlText* glText, float renderTime, float frameTime, const GlTransform& transform, 
                 Vec2<float> textBaseline, Vec2<float> lineAdvance) {

    textBaseline = DrawMemoryStats(glText, textBaseline, lineAdvance);
    textBaseline = DrawFPS(glText, renderTime, frameTime, textBaseline, lineAdvance);
    textBaseline = DrawTransform(glText, transform, textBaseline, lineAdvance);
    
    glText->Draw();
    glText->Clear();
}

enum ARInstance {
    AR_INSTANCE_REAR_CAMERA,
    AR_INSTANCE_FRONT_CAMERA,
};

void UpdateFrameCamera(ARInstance arInstance, GlCamera& camera) {
    
    ARWrapper* updateArWrapper;
    ARWrapper* pauseArWrapper;
    
    if(arInstance == AR_INSTANCE_FRONT_CAMERA) {

        updateArWrapper = ARWrapper::FrontInstance();
        pauseArWrapper  = ARWrapper::Instance();

    } else if(arInstance == AR_INSTANCE_REAR_CAMERA) {

        updateArWrapper = ARWrapper::Instance();
        pauseArWrapper  = ARWrapper::FrontInstance();

    } else {
        Panic("Invalid ArInstance [%d]", arInstance);
    }

    ArStatus status = pauseArWrapper->Pause();
    if(status != AR_SUCCESS) {
        Warn("Failed to pause back ARWrapper [%p]. Status [%d]", pauseArWrapper, status);
    } 

    updateArWrapper->Resume();
    ARWrapper::UpdateFrameResult frameResult = updateArWrapper->UpdateFrame();

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
        .renderStringAttrib = { .rgba = RGBAf(1.f, 0, 0) },
    });
    
    InitGlesState();
    
    //GlCamera backCamera(Mat4<float>::OrthogonalProjection(Vec2<float>(glContext.Width(), glContext.Height())*.001f , 0, 2000));
    //GlCamera backCamera(Mat4<float>::OrthogonalProjection(Vec2<float>(glContext.Width(), glContext.Height()), 0, 2000)); //TODO: see if we can replace scaling view with scaling camera so glSkymap draws properly
    //GlCamera backCamera(Mat4<float>::PerspectiveProjection((float)glContext.Width()/glContext.Height(), ToRadians(85.f), 0.01f, 2000.f), GlTransform(Vec3(0.f, 0.f, 1.f)));
    GlCamera backCamera(glContext.Width(), glContext.Height()), 
             frontCamera(glContext.Width(), glContext.Height());
    
    //TODO: Clean up this initialization when we pull EglTexture out into its own class ... maybe have ArWrapper return an instance of it for us?
    {
        ARWrapper::EglTextureSize eglTextureSize = ARWrapper::Instance()->GetEglTextureSize();

        Log("Using ArCore::Instance EglTexture - cpu Size: '%d x %d' | gpu size '%d x %d'",
            eglTextureSize.cpuTextureSize.x, eglTextureSize.cpuTextureSize.y,
            eglTextureSize.gpuTextureSize.x, eglTextureSize.gpuTextureSize.y);

        backCamera.SetEglTextureSize(eglTextureSize.gpuTextureSize.x, eglTextureSize.gpuTextureSize.y);
    }

    // {
    //     ARWrapper::EglTextureSize eglTextureSize = ARWrapper::FrontInstance()->GetEglTextureSize();

    //     Log("Using ArCore::FrontInstance EglTexture - cpu Size: '%d x %d' | gpu size '%d x %d'",
    //         eglTextureSize.cpuTextureSize.x, eglTextureSize.cpuTextureSize.y,
    //         eglTextureSize.gpuTextureSize.x, eglTextureSize.gpuTextureSize.y);

    //     frontCamera.SetEglTextureSize(eglTextureSize.gpuTextureSize.x, eglTextureSize.gpuTextureSize.y);
    // }

    //Note: Bind camera texture to ArCore
    //Warn: Order dependent.
    //      We must call SetEglCameraTexture before we update the frame
    //      we also only want to query the ArWrapper ProjectionMatrix after the frame is updated
    ARWrapper::Instance()->SetEglCameraTexture(backCamera.EglTexture());
    UpdateFrameCamera(AR_INSTANCE_REAR_CAMERA, backCamera);
    backCamera.SetProjectionMatrix(ARWrapper::Instance()->ProjectionMatrix(.01f, 1000.f));

    Camera testCamera(Camera::FRONT_CAMERA);

    // //Note: Bind camera texture to ArCore
    // //Warn: Order dependent.
    // //      We must call SetEglCameraTexture before we update the frame
    // //      we also only want to query the ArWrapper ProjectionMatrix after the frame is updated
    // ARWrapper::FrontInstance()->SetEglCameraTexture(backCamera.EglTexture());
    // UpdateFrameCamera(AR_INSTANCE_FRONT_CAMERA, frontCamera);
    // frontCamera.SetProjectionMatrix(ARWrapper::FrontInstance()->ProjectionMatrix(.01f, 1000.f));    
    
    //TODO: place in ArWrapper... clean up ArWrapper singleton design
    // ArPlanes arPlanes(ARWrapper::Instance()->ArSession());

    // TODO: Get Camera working with front/back camera and let ARWrapper switch between them so we can update both sides of 
    //      the cubemap in the same frame
    // {
    //     Camera camera(Camera::FRONT_CAMERA);
    // }

    //Setup cubemap skybox    
    GlSkybox skybox(GlSkybox::SkyboxParams {

        .cubemap = {

            // .posX = "textures/skymap/px.png",
            // .negX = "textures/skymap/nx.png",
            // .posY = "textures/skymap/py.png",
            // .negY = "textures/skymap/ny.png",
            // .posZ = "textures/skymap/pz.png",
            // .negZ = "textures/skymap/nz.png",

            .posX = "textures/roomCubemap/+X.png",
            .negX = "textures/roomCubemap/-X.png",
            .posY = "textures/roomCubemap/+Y.png",
            .negY = "textures/roomCubemap/-Y.png",
            .posZ = "textures/roomCubemap/+Z.png",
            .negZ = "textures/roomCubemap/-Z.png",            

            // .posX = "textures/uvGrid.png",
            // .negX = "textures/uvGrid.png",
            // .posY = "textures/uvGrid.png",
            // .negY = "textures/uvGrid.png",
            // .posZ = "textures/uvGrid.png",
            // .negZ = "textures/uvGrid.png",

            // .posX = "textures/pink.png",
            // .negX = "textures/pink.png",
            // .posY = "textures/pink.png",
            // .negY = "textures/pink.png",
            // .posZ = "textures/pink.png",
            // .negZ = "textures/pink.png",

            // .posX = "textures/green.png",
            // .negX = "textures/red.png",
            // .posY = "textures/white.png",
            // .negY = "textures/white.png",
            // .posZ = "textures/white.png",
            // .negZ = "textures/white.png",

            // .posX = "textures/white_4096.png",
            // .negX = "textures/white_4096.png",
            // .posY = "textures/white_4096.png",
            // .negY = "textures/white_4096.png",
            // .posZ = "textures/white_4096.png",
            // .negZ = "textures/white_4096.png",

            // .posX = "textures/black.png",
            // .negX = "textures/black.png",
            // .posY = "textures/black.png",
            // .negY = "textures/black.png",
            // .posZ = "textures/black.png",
            // .negZ = "textures/black.png",
    
            // .posX = "textures/debugTexture.png",
            // .negX = "textures/debugTexture.png",
            // .posY = "textures/debugTexture.png",
            // .negY = "textures/debugTexture.png",
            // .posZ = "textures/debugTexture.png",
            // .negZ = "textures/debugTexture.png",
        },

        // .camera = &backCamera,
        .camera = &frontCamera,
        .generateMipmaps = true, //Note: used for object roughness parameter
    });

    //Setup object to render

    GlObject objects[] = {
        GlObject("meshes/cow.obj",
                 &backCamera,
                 &skybox,
                 GlTransform(Vec3(0.f, -.1f, -1.f), Vec3(.03f, .03f, .03f))
        ),

        GlObject("meshes/blenderUmbrella.obj",
                &backCamera,
                &skybox,
                GlTransform(
                    Vec3(0.f, 0.f, -1.f),   //position
                    Vec3(.01f, .01f, .01f), //scale
                    Vec3<float>(ToRadians(-90.f), ToRadians(0.f), ToRadians(25.f)) //rotation
                )
        )
    };

    //TODO: computed normals are incorrect this.. need to add verticies when normals change direction!    
    // GlObject obj("meshes/cube.obj",
    //              &backCamera,
    //              &skybox,
    //              // GlTransform(Vec3(0.f, 0.f, -1.f), Vec3(.05f, .05f, .05f))
    //              GlTransform(Vec3(0.f, 0.f, -1.f), Vec3(.25f, .25f, .25f))
    //             );

    // GlObject obj("meshes/sphere.obj",
    //              &backCamera,
    //              &skybox,
    //              GlTransform(Vec3(0.f, 0.f, -.5f), Vec3(.1f, .1f, .1f))
    //              //GlTransform(Vec3(0.f, 0.f, 0.f), Vec3(.1f, .1f, .1f))
    //             );
    
    // GlObject obj("meshes/triangle.obj",
    //              &backCamera,
    //              &skybox,
    //              GlTransform(
    //                  Vec3<float>(0.f, 0.f, 0.f), 
    //                  Vec3<float>(.2f, .2f, .2f), 
    //                  Vec3<float>(ToRadians(45.f), ToRadians(45.f), 0.f)
    //              )
    //             );

    
    Timer fpsTimer(true);
    Timer physicsTimer(true);
    Timer frontCameraTimer(true);

    constexpr float kFrontCameraUpdateInterval = .5;

    for(Timer loopTimer(true) ;; loopTimer.SleepLapMs(kTargetMsFrameTime) ) {

        float secElapsed = physicsTimer.LapSec();
        
        // TODO: POLL ANDROID MESSAGE LOOP FOR KEY EVENTS

        //Update cameras to match current ArCore position
        UpdateFrameCamera(AR_INSTANCE_REAR_CAMERA, backCamera);
        // UpdateFrameCamera(AR_INSTANCE_FRONT_CAMERA, frontCamera);

        // // TODO: This is really slopply written and really just a hack to get things working
        // //       update Timer to allow you to get ElapsedSec and then apply old-timeref so
        // //       we skip time
        // if(frontCameraTimer.ElapsedSec() >= kFrontCameraUpdateInterval) {
        //     frontCameraTimer.LapNs();
        //     UpdateFrameCamera(AR_INSTANCE_FRONT_CAMERA, frontCamera);
        // }

        //Update arPlanes
        // arPlanes.UpdatePlanes();

        //clear last frame color and depth buffer

        glClearDepthf(1.f);
        // glClearColor(1.f, 1.f, 1.f, 0.f); //white
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
        // backCamera.SetProjectionMatrix(Mat4<float>::PerspectiveProjection(1.f, ToRadians(90.f), .01f, 1000.f));

        backCamera.Draw();

        glViewport(0, 0, 500, 500);
        frontCamera.Draw();

        glViewport(0, 0, glContext.Width(), glContext.Height());

        //update skybox
        //TODO: only update texture if camera updates 'ArFrame_getTimestamp'
        {
            Timer cameraTimer(true);
            int cameraInvocations = 1; // TODO: set to 100 for benchmarking
            for(int i = 0; i < cameraInvocations; ++i) {
                skybox.UpdateTexture(&glContext);
                // glFinish();
            }

            float cameraMs = cameraTimer.ElapsedMs();
            glText.PushString(Vec3(10.f, 500.f, 0.f), "CameraMs: %f (%f ms per invocation)", cameraMs, cameraMs/cameraInvocations);
        }
        
        // arPlanes.Draw();

        skybox.ClearDepthTexture(&glContext);
        for(int i = 0; i < ArrayCount(objects); ++i) {

            GlObject& obj = objects[i];

            // Rotate object
            Vec3 omega = ToRadians(i ? Vec3<float>(0.f, 0.f, 10.f) : Vec3<float>::zero);
            GlTransform transform = obj.GetTransform();
            transform.Rotate(omega*secElapsed);
            obj.SetTransform(transform);
            
            obj.RenderToDepthTexture(&glContext);
        }

        bool gaussianBlur = false;
        skybox.AntiAlisDepthBuffer(&glContext, gaussianBlur);

        for(int i = 0; i < ArrayCount(objects); ++i) {
            GlObject& obj = objects[i];   
            obj.Draw(0);
        }

        // Note: For Debugging
        // skybox.DrawDepthTexture();
        // skybox.Draw();

        DrawStrings(&glText,
                    loopTimer.ElapsedSec(),
                    fpsTimer.LapSec(),
                    backCamera.GetTransform(),
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
        ARWrapper::FrontInstance()->InitializeARWrapper(jniEnv, jActivity);
        
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
