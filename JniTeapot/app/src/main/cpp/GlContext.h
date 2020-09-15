#pragma once

#include <EGL/egl.h>
#include <GLES3/gl31.h>
#include <android/native_window.h> //C version of android.view.Surface

#include "types.h"
#include "Memory.h"

#define GL_ASSERT_INDENT "\n\t\t\t"
#define GL_ASSERT_END GL_ASSERT_INDENT "}\n\t\t"

#define GlContextAssertNoError_(name, errorFunc, errorMsg, ...) {    \
    int error_;                                                      \
    RUNTIME_ASSERT(!(error_ = errorFunc()),                          \
                    GL_ASSERT_INDENT name " failed {"                \
                    GL_ASSERT_INDENT "\tMSG: " errorMsg              \
                    GL_ASSERT_INDENT "\t" #errorFunc ": %d [0x%08x]" \
                    GL_ASSERT_END,                                   \
                    ##__VA_ARGS__, error_, error_);                  \
}

#define GlContextAssert_(name, errorFunc, condition, errorMsg, ...) { \
    int error_;                                                       \
    RUNTIME_ASSERT((condition) || (error_ = errorFunc(), false),      \
                    GL_ASSERT_INDENT name " failed {"                 \
                    GL_ASSERT_INDENT "\tMSG: " errorMsg               \
                    GL_ASSERT_INDENT "\t" #errorFunc": %d [0x%08x]"   \
                    GL_ASSERT_END,                                    \
                    ##__VA_ARGS__, error_, error_);                   \
}

#define GlContextAssertValue_(name, errorFunc, value, trueValue, errorMsg, ...) {       \
    int error_;                                                                         \
    RUNTIME_ASSERT((value) == (trueValue) || (error_ = errorFunc(), false),             \
                    GL_ASSERT_INDENT name " failed {"                                   \
                    GL_ASSERT_INDENT "\tMSG: " errorMsg                                 \
                    GL_ASSERT_INDENT "\t:" #errorFunc " %d [0x%08x]"                    \
                    GL_ASSERT_INDENT "\tvalue: %d [0x%08x]"                             \
                    GL_ASSERT_INDENT "\ttrueValue: %d [0x%08x]"                         \
                    GL_ASSERT_END,                                                      \
                    ##__VA_ARGS__, error_, error_, value, value, trueValue, trueValue); \
}

#define EglAssertNoError(errorMsg, ...) GlContextAssertNoError_("EglAssertNoError", eglGetError, errorMsg, ##__VA_ARGS__)
#define GlAssertNoError(errorMsg,  ...) GlContextAssertNoError_("GlAssertNoError",  glGetError,  errorMsg, ##__VA_ARGS__)

#define EglAssert(condition, errorMsg, ...) GlContextAssert_("EglAssert", eglGetError, condition, errorMsg, ##__VA_ARGS__)
#define GlAssert(condition,  errorMsg, ...) GlContextAssert_("GlAssert",  glGetError,  condition, errorMsg, ##__VA_ARGS__)

#define EglAssertTrue(val, errorMsg, ...) GlContextAssertValue_("EglAssertTrue", eglGetError, val, EGL_TRUE, errorMsg, ##__VA_ARGS__)
#define GlAssertTrue(val,  errorMsg, ...) GlContextAssertValue_("GlAssertTrue",  glGetError,  val, GL_TRUE,  errorMsg, ##__VA_ARGS__)

class GlContext {
    public:
        static constexpr GLuint kGlesMajorVersion = 3,
                                kGlesMinorVersion = 1;

        static constexpr EGLint kRGBAChanelBitDepth = 4;
        static constexpr EGLint kZBufferBitDepth = 24;
        static constexpr EGLint kStencilBitDepth = 0;
        static constexpr EGLint kMsaaSamples = 0;
        
        static constexpr EGLint kSwapInterval = 0; // 0 for no-vsync, 1-for vsync, n-for vsync buffing
        
        
        //TODO: do we even want a render queue like this? maybe just have people handle things
        //      themselves like GLText?
        template<typename T>
        using RenderCommand = FuncPtr<void, T*>;
        
        static constexpr auto kMaxRenderQueueItems = 1024;
        
    private:
        
        template<typename T>
        struct RenderQueue {
            RenderCommand<T> command;
            T* data;
        };
        
        RenderQueue<void> renderQueue[kMaxRenderQueueItems];
		uint32 renderQueuePosition = 0;
		
		ANativeWindow* nativeWindow = nullptr;

        EGLDisplay eglDisplay = EGL_NO_DISPLAY;
        EGLConfig eglConfig = nullptr;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        EGLContext eglContext = EGL_NO_CONTEXT;

        EGLint eglWidth, eglHeight;
        
        static inline
        EGLDisplay GetEglDisplay() {
            EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

            EGLint majorVersion, minorVersion;
            EglAssertTrue(eglInitialize(display, &majorVersion, &minorVersion), "Failed to initialize egl display");

            Log("Initialized EGL Display { display: %p, version: %d.%d }", display, majorVersion, minorVersion);
            return display;
        }

        inline
        EGLSurface CreateEglSurface(const EGLDisplay& display, const EGLConfig& config, int* outWidth, int* outHeight) {

            EGLSurface surface = eglCreateWindowSurface(display, config, nativeWindow, NULL);
            EglAssert(surface != EGL_NO_SURFACE, "Failed to create eglSurface { window: %p, display: %d, config: %d }", nativeWindow, display, config);

            EglAssertTrue(eglSurfaceAttrib(display, surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED),
                          "Failed to set EGL_BUFFER_DESTROYED surface attribute { window %p, display: %d, config: %d, surface: %d }",
                          nativeWindow, display, config, surface);

            eglQuerySurface(display, surface, EGL_WIDTH, outWidth);
            eglQuerySurface(display, surface, EGL_HEIGHT, outHeight);

            Log("Created EGL surface { window: %p, display: %d, config: %p, surface: %p, width: %u, height: %u}",
                nativeWindow, display, config, surface, *outWidth, *outHeight);

            return surface;
        }
        
        static inline
        EGLConfig GetEglConfig(const EGLDisplay& display) {
            static const EGLint attribs[] = {
                EGL_RENDERABLE_TYPE, (kGlesMajorVersion >= 3 ? EGL_OPENGL_ES3_BIT : kGlesMajorVersion >= 2 ? EGL_OPENGL_ES2_BIT : EGL_OPENGL_ES_BIT),
                EGL_RED_SIZE,   kRGBAChanelBitDepth,
                EGL_GREEN_SIZE, kRGBAChanelBitDepth,
                EGL_BLUE_SIZE,  kRGBAChanelBitDepth,
                EGL_ALPHA_SIZE, kRGBAChanelBitDepth,
                EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
                EGL_DEPTH_SIZE, kZBufferBitDepth,
                EGL_STENCIL_SIZE, kStencilBitDepth,
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_SAMPLE_BUFFERS, (kMsaaSamples ? 1 : 0), //Max of 1 msaa buffer allowed
                EGL_SAMPLES, kMsaaSamples,
                EGL_NONE
            };
            
            EGLConfig config;
            EGLint numConfigs;
            EglAssertTrue(eglChooseConfig(display, attribs, &config, 1, &numConfigs), "Failed to get compatible egl configuration { display: %d }", display);
            EglAssert(numConfigs, "No compatible egl configuration { display: %d }", display);
            
            Log("Selected EGL config { config: %p, display: %d, kRGBAChanelBitDepth: %d, kZBufferBitDepth: %d, kStencilBitDepth: %d, kMsaaSamples: %d }",
                config, display, kRGBAChanelBitDepth, kZBufferBitDepth, kStencilBitDepth, kMsaaSamples );
            
            return config;
        }

        static inline
        EGLContext CreateAndBindEglContext(const EGLDisplay& display, const EGLConfig& config, const EGLSurface& surface) {
            const EGLint contexAttribs[] = {
                EGL_CONTEXT_MAJOR_VERSION, kGlesMajorVersion,
                EGL_CONTEXT_MINOR_VERSION, kGlesMinorVersion,
                EGL_NONE
            };

            EGLContext context = eglCreateContext(display, config, NULL, contexAttribs);
            EglAssert(context != EGL_NO_CONTEXT, "Failed to make eglContext { display: %d, config: %p }", display, config);
    
            // bind egl to our thread
            EglAssertTrue(eglMakeCurrent(display, surface, surface, context), "Failed to bind egl surface to thread");
            
            // set swap interval
            EglAssertTrue(eglSwapInterval(display, kSwapInterval), "Failed to set swap interval { display: %p, context: %p, inverval: %d }", display, context, kSwapInterval);
            
            Log("Created EGL context { display: %d, config: %p, context: %p }", display, config, context);
            return context;
        }

        static inline
        void FreeContext(const EGLDisplay& display, const EGLContext& context) {
            EglAssertTrue(eglDestroyContext(display, context), "Failed to destroy EGL Context { display: %d, context: %p }", display, context);
        }
        static inline
        void FreeSurface(const EGLDisplay& display, const EGLSurface& surface) {
            EglAssertTrue(eglDestroySurface(display, surface), "Failed to destroy EGL Surface { display: %d, surface: %p }", display, surface);
        }

        static inline
        void FreeDisplay(const EGLDisplay& display) {
            EglAssertTrue(eglTerminate(display), "Failed to destroy Terminate EGL Display { display: %d }", display);
        }


    public:

        inline int Width() { return eglWidth; }
        inline int Height() { return eglHeight; }
        
        inline
        void static PrintVariables(GLuint glProgram) {
            
            GLint count, size;
            GLenum type;
            char nameBuffer[KB(1)];
    
            glGetProgramiv(glProgram, GL_ACTIVE_ATTRIBUTES, &count);
            Log("Active Attributes { count: %d, glProgram: %u }:", count, glProgram);
            for(GLuint i = 0; i < count; ++i) {

                glGetActiveAttrib(glProgram, i, ArrayCount(nameBuffer), NULL, &size, &type, nameBuffer);
                GLint location = glGetAttribLocation(glProgram, nameBuffer);
                Log("\t-> Attribute %d { Type: %u, location: %d, Size: %d, Name: %s }", i, type, location, size, nameBuffer);
            }
    
            glGetProgramiv(glProgram, GL_ACTIVE_UNIFORMS, &count);
            Log("Active Uniforms { count: %d, glProgram: %u }:", count, glProgram);
            for(GLuint i = 0; i < count; ++i) {
                glGetActiveUniform(glProgram, i, ArrayCount(nameBuffer), NULL, &size, &type, nameBuffer);
                GLint location = glGetUniformLocation(glProgram, nameBuffer);
                Log("\t-> Uniform %d { Type: %u, location: %d, Size: %d, Name: %s }", i, type, location, size, nameBuffer);            }
    
            glGetProgramiv(glProgram, GL_ACTIVE_UNIFORM_BLOCKS, &count);
            Log("Active Uniform Blocks { count: %d, glProgram: %u }:", count, glProgram);
            for(GLuint i = 0; i < count; ++i) {
                glGetActiveUniformBlockName(glProgram, i, ArrayCount(nameBuffer), NULL, nameBuffer);
                GLint index = glGetUniformBlockIndex(glProgram, nameBuffer);
                Log("\t-> Uniform Block %d { index: %d, Name: %s }", i, index, nameBuffer);
            }
        }
        
        void Init(ANativeWindow* window) {
                nativeWindow = window;
                
                // Initialize EGL
                eglDisplay = GetEglDisplay();
                eglConfig = GetEglConfig(eglDisplay);
                eglSurface = CreateEglSurface(eglDisplay, eglConfig, &eglWidth, &eglHeight);
                eglContext = CreateAndBindEglContext(eglDisplay, eglConfig, eglSurface);
                
                Log("Finished Initializing GLES version: %s", (const char*)glGetString(GL_VERSION));
            }
    
            inline
            void Shutdown() {
                eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                FreeContext(eglDisplay, eglContext);
                FreeSurface(eglDisplay, eglSurface);
                FreeDisplay(eglDisplay);
            }
    
            static inline
            GLuint CreateShader(GLuint type, const char* source) {

                GLuint shader = glCreateShader(type);
                GlAssert(shader,
                        "Failed to create gl shader { "
                        GL_ASSERT_INDENT "\ttype: %d"
                        GL_ASSERT_INDENT "\tsource: ["
                        "\n%s\n"
                        GL_ASSERT_INDENT
                        GL_ASSERT_INDENT "\t]"
                        GL_ASSERT_INDENT "}",
                
                        type, source
                );
                
                glShaderSource(shader, 1, &source, NULL);
                glCompileShader(shader);

                GLint status;
                static char glCompileErrorStr[KB(1)];
                GlAssertTrue( (glGetShaderiv(shader, GL_COMPILE_STATUS, &status), status),
                             "Failed to compile gl shader {"
                             GL_ASSERT_INDENT "\ttype: %d"
                             GL_ASSERT_INDENT "\tsource: ["
                             "\n%.100s..." //Note: android logging caps out at 4k so 100 limit prevents chopping off info string
                             GL_ASSERT_INDENT "\t]"
                             GL_ASSERT_INDENT "\tInfo: %s"
                             GL_ASSERT_INDENT "}",
                             
                             type,
                             source,
                             (glGetShaderInfoLog(shader, sizeof(glCompileErrorStr), NULL, glCompileErrorStr), glCompileErrorStr)
                );
                
                return shader;
            }
            
            static
            GLuint CreateGlProgram(const char* vertexSource, const char* fragmentSource) {
            
                GLuint glProgram = glCreateProgram();
                GlAssert(glProgram, "Failed to Create gl program");
                
                GLuint glVertexShader = CreateShader(GL_VERTEX_SHADER, vertexSource),
                       glFragmentShader = CreateShader(GL_FRAGMENT_SHADER, fragmentSource);
    
                glAttachShader(glProgram, glVertexShader);
                glAttachShader(glProgram, glFragmentShader);
    
                glLinkProgram(glProgram);
    
                int status;
                static char linkInfoStr[KB(1)];
                GlAssertTrue((glGetProgramiv(glProgram, GL_LINK_STATUS, &status), status),
                             "Failed to Link glProgram {"
                             GL_ASSERT_INDENT "\tglProgram: %d"
                             GL_ASSERT_INDENT "\tGL_LINK_STATUS: %d",
                             GL_ASSERT_INDENT "\tVertex Source [",
                             "\n%s\n"
                             GL_ASSERT_INDENT "\t]",
                             GL_ASSERT_INDENT "\tFragment Source ["
                             "\n%s\n",
                             GL_ASSERT_INDENT "\t]",
                             GL_ASSERT_INDENT "\tLinkInfo: %s"
                             GL_ASSERT_INDENT "}",
                             
                             
                             glProgram,
                             status,
                             vertexSource,
                             fragmentSource,
                             (glGetProgramInfoLog(glProgram, sizeof(linkInfoStr), NULL, linkInfoStr), linkInfoStr)
                );
    
                // clean up shader objects - already compiled into executable, no longer need the intermediate code sticking around
                // Note: shaders must be detached first or else delete won't free them from memory
                glDetachShader(glProgram, glVertexShader);
                glDetachShader(glProgram, glFragmentShader);
    
                glDeleteShader(glVertexShader);
                glDeleteShader(glFragmentShader);
                
                return glProgram;
            }
    
			template<typename T>
            inline void PushRenderQueue(RenderCommand<T> cmd, T* data = nullptr) {
				RUNTIME_ASSERT(renderQueuePosition < kMaxRenderQueueItems-1, "Exceeded maximum number of Render commands: %d", kMaxRenderQueueItems);
				renderQueue[renderQueuePosition++] = RenderQueue<void>{ .command = (RenderCommand<void>)cmd, .data = data };
			}

            //TODO: create GENERIC!!! VBO
            //TODO: create VBI's
            //TODO: create FBO pipeline: vbo queue -> render with their included shaders & optional VBIs to backBuffer quad texture
            //      - VBOs render pipeline takes in a camera to render to!
            //      - Camera system for handling MVP matrix transforms
    
            //TODO: create FBO
            //TODO: create COMPOSITE pipeline: fbo queue -> render with their included shaders (or none) -> stencil regions & z-ordering
    
            //TODO: have text render unit quad vertices for each character
            //      - write shader that positions text on
            //      - GlText is a type of renderable VBO - doesn't have world matrix though (default camera?)
            //      - GlText gets rendered to a hub FBO - can still add post processing like shake, and distortion!
    
    
            static inline void VertexAttribPointerArray(GLuint attribIndex, GLuint dataType, uint32 bytes, const void* pointer) {
    
                //Note: gl spec makes all attributes aligned to 4 bytes
                //      and allows us to set 4 attributes in a single call
                constexpr uint8 kAttributeBytes = 4;
                constexpr uint8 kMaxAttribUploadsPerCall = 4;
    
                uint8 stride = kMaxAttribUploadsPerCall*kAttributeBytes;
                while(bytes) {
                    
                    if(stride > bytes) stride = bytes;
                    glVertexAttribPointer(attribIndex, CeilFraction(stride, kAttributeBytes), dataType, false, 0, pointer);
    
                    bytes-= stride;
                    pointer = ByteOffset(pointer, stride);
                }
            }
            
            
            inline
            void Draw() {
				// Note: using function commands that call into opengl instead of an enumeration because we want
				//       students to program the opengl calls
				for(uint32 i = 0; i < renderQueuePosition; ++i) {
				    RenderQueue<void>& queue = renderQueue[i];
					queue.command(queue.data);
				}
				
				renderQueuePosition = 0;
			}
            
            // Returns false if context was recreated
            bool SwapBuffers() {
                if(eglSwapBuffers(eglDisplay, eglSurface) == EGL_TRUE) return true;
    
                GLint error = eglGetError();
                switch(error) {
                    case EGL_CONTEXT_LOST: //lost context due to memory purge
                        Warn("eglContext was purged from memory - Recreating")
                        FreeContext(eglDisplay, eglContext);
                        eglContext = CreateAndBindEglContext(eglDisplay, eglConfig, eglDisplay);
                    break;
    
                    case EGL_BAD_SURFACE:
    
                        // TODO: FIX MINIMIZE MAXIMIZE BUG HERE (window is destroyed so we can't just recreate surface)
                        //      may be be able to fix by relasing window and suspending condtext?
                        //      maybe 'configChanges' setting in manifest
    
                        Warn("eglSurface is invalid! - Recreating");
                        FreeSurface(eglDisplay, eglSurface);
                        eglSurface = CreateEglSurface(eglDisplay, eglConfig, &eglWidth, &eglHeight);
                    break;
    
                    // EGL_BAD_DISPLAY or EGL_NOT_INITIALIZED
                    default:
                        RUNTIME_ASSERT(error == EGL_BAD_DISPLAY || error == EGL_NOT_INITIALIZED, "Unknown EGL SwapBuffers error: %d", error);
    
                        Warn("eglDisplay is invalid! - Recreating");
                        FreeContext(eglDisplay, eglContext);
                        FreeSurface(eglDisplay, eglSurface);
                        FreeDisplay(eglDisplay);
                        Init(nativeWindow);
                    break;
                }
    
                eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
                return false;
            }
};

