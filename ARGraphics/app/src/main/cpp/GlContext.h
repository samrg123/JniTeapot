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

    private:
        int eglWidth, eglHeight;

    public:

        void UpdateScreenSize(int width, int height) {
            eglWidth = width;
            eglHeight = height;
        }


        inline int Width() { return eglWidth; }

        inline int Height() { return eglHeight; }

        inline
        void static PrintVariables(GLuint glProgram) {

            GLint count, size;
            GLenum type;
            char nameBuffer[KB(1)];

            glGetProgramiv(glProgram, GL_ACTIVE_ATTRIBUTES, &count);
            Log("Active Attributes { count: %d, glProgram: %u }:", count, glProgram);
            for (GLuint i = 0; i < count; ++i) {

                glGetActiveAttrib(glProgram, i, ArrayCount(nameBuffer), NULL, &size, &type, nameBuffer);
                GLint location = glGetAttribLocation(glProgram, nameBuffer);
                Log("\t-> Attribute %d { Type: %u, location: %d, Size: %d, Name: %s }", i, type,
                    location, size, nameBuffer);
            }

            glGetProgramiv(glProgram, GL_ACTIVE_UNIFORMS, &count);
            Log("Active Uniforms { count: %d, glProgram: %u }:", count, glProgram);
            for (GLuint i = 0; i < count; ++i) {
                glGetActiveUniform(glProgram, i, ArrayCount(nameBuffer), NULL, &size, &type,
                                   nameBuffer);
                GLint location = glGetUniformLocation(glProgram, nameBuffer);
                Log("\t-> Uniform %d { Type: %u, location: %d, Size: %d, Name: %s }", i, type, location,
                    size, nameBuffer);
            }

            glGetProgramiv(glProgram, GL_ACTIVE_UNIFORM_BLOCKS, &count);
            Log("Active Uniform Blocks { count: %d, glProgram: %u }:", count, glProgram);
            for (GLuint i = 0; i < count; ++i) {
                glGetActiveUniformBlockName(glProgram, i, ArrayCount(nameBuffer), NULL, nameBuffer);
                GLint index = glGetUniformBlockIndex(glProgram, nameBuffer);
                Log("\t-> Uniform Block %d { index: %d, Name: %s }", i, index, nameBuffer);
            }
        }


        static inline
        GLuint CreateShader(GLuint type, const char *source) {

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
            GlAssertTrue((glGetShaderiv(shader, GL_COMPILE_STATUS, &status), status),
                         "Failed to compile gl shader {"
                                 GL_ASSERT_INDENT "\ttype: %d"
                                 GL_ASSERT_INDENT "\tsource: ["
                                                  "\n%.100s..." //Note: android logging caps out at 4k so 100 limit prevents chopping off info string
                                 GL_ASSERT_INDENT "\t]"
                                 GL_ASSERT_INDENT "\tInfo: %s"
                                 GL_ASSERT_INDENT "}",

                         type,
                         source,
                         (glGetShaderInfoLog(shader, sizeof(glCompileErrorStr), NULL,
                                             glCompileErrorStr), glCompileErrorStr)
            );

            return shader;
        }

        static
        GLuint CreateGlProgram(const char *vertexSource, const char *fragmentSource) {

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
                         (glGetProgramInfoLog(glProgram, sizeof(linkInfoStr), NULL,
                                              linkInfoStr), linkInfoStr)
            );

            // clean up shader objects - already compiled into executable, no longer need the intermediate code sticking around
            // Note: shaders must be detached first or else delete won't free them from memory
            glDetachShader(glProgram, glVertexShader);
            glDetachShader(glProgram, glFragmentShader);

            glDeleteShader(glVertexShader);
            glDeleteShader(glFragmentShader);

            return glProgram;
        }
};