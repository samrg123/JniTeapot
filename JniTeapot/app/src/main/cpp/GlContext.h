#pragma once


#include "glUtil.h"
#include <android/native_window.h> //C version of android.view.Surface

#include "types.h"
#include "Memory.h"
#include "lodepng/lodepng.h"

#include "stdlib.h"
#include "lodepng/lodepng.h"

class GlContext {
    public:
        static constexpr GLuint kGlesMajorVersion = 3,
                                kGlesMinorVersion = 1;

        static constexpr EGLint kRGBAChanelBitDepth = 4;
        static constexpr EGLint kZBufferBitDepth = 24;
        static constexpr EGLint kStencilBitDepth = 0;
        static constexpr EGLint kMsaaSamples = 0;
        
        static constexpr EGLint kSwapInterval = 0; // 0 for no-vsync, 1-for vsync, n-for vsync buffing
        
    private:
    
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

        inline int Width()  const { return eglWidth; }
        inline int Height() const { return eglHeight; }
        
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
                             GL_ASSERT_INDENT "\ttype: %s [%d]"
                             GL_ASSERT_INDENT "\tsource: ["
                             "\n%.512s..." //Note: android logging caps out at 4k so 512 limit prevents chopping off info string
                             GL_ASSERT_INDENT "\t]"
                             GL_ASSERT_INDENT "\tInfo: %s"
                             GL_ASSERT_INDENT "}",
                             
                             (type == GL_VERTEX_SHADER ? "VertexShader" : type == GL_FRAGMENT_SHADER ? "FragmentShader" : "Unknown"), type,
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
                             GL_ASSERT_INDENT "\tGL_LINK_STATUS: %d"
                             GL_ASSERT_INDENT "\tVertex Source ["
                             "\n%.256s...\n"
                             GL_ASSERT_INDENT "\t]"
                             GL_ASSERT_INDENT "\tFragment Source ["
                             "\n%.256s...\n"
                             GL_ASSERT_INDENT "\t]"
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
        
            static GLuint LoadCubemap(const char* (&images)[6], int &size) {
    
                GLuint cubemapTexture;
                glGenTextures(1, &cubemapTexture);
                GlAssertNoError("Failed to create cubemap texture");
                
                glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
                
                //TODO: generate cubemap mipmaps!
                size = -1;
                
                for(int i = 0; i < 6; ++i) {
    
                    Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
                    FileManager::AssetBuffer* pngBuffer = FileManager::OpenAsset(images[i], &Memory::temporaryArena);
    
                    uchar* decodedPng;
                    uint width, height;
                    lodepng_decode_memory(&decodedPng, &width, &height, pngBuffer->data, pngBuffer->size, LodePNGColorType::LCT_RGBA, 8);
                   
                    if (width != height || (size != -1 && width != size)) {
                        Log("Invalid cubemap texture size\n");
                    } 
                    size = width;
    
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                                 0,               //mipmap level
                                 GL_RGBA,         //internal format
                                 width,
                                 height,
                                 0,                //border must be 0
                                 GL_RGBA,          //input format
                                 GL_UNSIGNED_BYTE,
                                 decodedPng);

                    GlAssertNoError("Failed to set cubemap image: '%s' side: %d", images[i], i);
                    
                    
                    free(decodedPng);
                    Memory::temporaryArena.FreeBaseRegion(tmpRegion);
                }

                return cubemapTexture;
            }
};

