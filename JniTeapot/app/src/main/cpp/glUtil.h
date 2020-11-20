#pragma once

#include <EGL/egl.h>
#include <GLES3/gl31.h>

#include "customAssert.h"
#include "types.h"

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

template<typename T> struct GlAttributeInfo;

template<typename T> constexpr GLenum GlAttributeSize() { return GlAttributeInfo<T>::kSize; };
template<typename T> constexpr GLenum GlAttributeType() { return GlAttributeInfo<T>::kType; };

template<> struct GlAttributeInfo<float>  { static constexpr uint32 kSize = 1; static constexpr GLenum kType = GL_FLOAT; };
template<> struct GlAttributeInfo<int8>   { static constexpr uint32 kSize = 1; static constexpr GLenum kType = GL_BYTE; };
template<> struct GlAttributeInfo<int16>  { static constexpr uint32 kSize = 1; static constexpr GLenum kType = GL_SHORT; };
template<> struct GlAttributeInfo<int32>  { static constexpr uint32 kSize = 1; static constexpr GLenum kType = GL_INT; };
template<> struct GlAttributeInfo<uint8>  { static constexpr uint32 kSize = 1; static constexpr GLenum kType = GL_UNSIGNED_BYTE; };
template<> struct GlAttributeInfo<uint16> { static constexpr uint32 kSize = 1; static constexpr GLenum kType = GL_UNSIGNED_SHORT; };
template<> struct GlAttributeInfo<uint32> { static constexpr uint32 kSize = 1; static constexpr GLenum kType = GL_UNSIGNED_INT; };

//Note: specialization for Vec types
template<typename T, template<typename, typename...> class VecTemplate>
struct GlAttributeInfo<VecTemplate<T>> { static constexpr uint32 kSize = VecTemplate<T>::kNumDims; static constexpr GLenum kType = GlAttributeInfo<T>::kType; };