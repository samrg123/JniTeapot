#pragma once

#include "StringLiteral.h"
#include "macros.h"

#if OPTIMIZED_BUILD
    #define DEBUG_SHADER 0
#else
    #define DEBUG_SHADER 1
#endif

#define OPEN_PAREN  (   //Map '(' to macro so we defer expansion of expression inside macro


//Macro to assist writting OpenGl shaders
/*Ex: 
    constexpr StringLiteral vertexShader = Shader(
        ShaderVersion("310 es");

        ShaderOut(0) vec3 pos;

        void main() {
            pos = vec3(0, 0, 0);
            gl_Position = vec4(pos, 1.);
        }    
    );
*/
//Note: Shader(...) expands '__VA_ARGS__' first before invocation of Shader_
#if DEBUG_SHADER
    #define Shader(...) StringLiteral("/*\n" __FILE__  ":" STRINGIFY(__LINE__) "\n*/\n") + Shader_(__VA_ARGS__)
#else
    #define Shader(...) Shader_(__VA_ARGS__)
#endif
#define Shader_(...) StringLiteral(#__VA_ARGS__) // Turn '__VA_ARGS__' into StringLiteral

//Macro to pass c expressions into shaders
/* Ex:

    constexpr int threashold = 5;
    constexpr StringLiteral shader = Shader(
        int GetOffset(int x) {
            if(x < ShaderValue(threashold)) return 0;
            return x;
        } 
    );  

    expands to StringLiteral("int GetOffset(int x) {if(x < 5) return 0; return x;}"
*/
#define ShaderValue(cValue)               ShaderValue_(cValue)                             // Expand macros in 'cValue'
#define ShaderValue_(cValue)              ) + ToStringLiteral(cValue) + Shader_ OPEN_PAREN // Terminate current 'Shader' expansion, evaluate 'cValue' and start new expansion of 'Shader_'

//Macro to include the source code to another shader
//Ex: constexpr auto foo1 = Shader( void doStuff(){} );
//    constexpr auto foo2 = Shader( ShaderInclude(foo1); void main() { doStuff(); } );
//Note: included shader must be constexpr
#define ShaderInclude(x)                ShaderValue(x)

// Macro to set the shader version to 'cValue'
//Ex: ShaderVersion("310 es") sets the shader version to '310 es'
#define ShaderVersion(cValue)            ShaderValue(ShaderVersionString(cValue))
#define ShaderVersionString(cValue)      StringLiteral("#version ") + ToStringLiteral(cValue) + StringLiteral("\n")

//Macro to enable Shader extension 'cValue'
//Ex: ShaderExtension(GL_OES_EGL_image_external) enabled the shader extension to 'GL_OES_EGL_image_external'
#define ShaderExtension(cValue)          ShaderValue(ShaderExtensionString(cValue))
#define ShaderExtensionString(cValue)    StringLiteral("#extension ") + ToStringLiteral(cValue) + StringLiteral(":require\n") 

//Macro to specify the layout location 'cValue' of a attribute/uniform
//Ex: ShaderLocation(0) in vec3 foo; binds variable 'foo' to layout location '0'
#define ShaderLocation(cValue)           ShaderValue(ShaderLocationString)
#define ShaderLocationString(cValue)     StringLiteral("layout(location=") + ToStringLiteral(static_cast<int>(cValue))+")"

//Macro to specify the layout binding 'cValue' of a uniform/sampler
//Ex: ShaderBinding(0) uniform sampler2d foo; binds uniform 'foo' to binding point '0'
#define ShaderBinding(cValue)            ShaderValue(ShaderBindingString)
#define ShaderBindingString(cValue)      StringLiteral("layout(binding=") + ToStringLiteral(static_cast<int>(cValue))+")"

//Macro to specify the layout location 'cValue' and direction 'in/out' of global shader variable
//Ex: ShaderIn(0) vec3 foo; binds 'foo' as in input variable at layout location '0'
#define ShaderIn(cValue)                 ShaderValue(ShaderLocationString(cValue) + "in ")
#define ShaderOut(cValue)                ShaderValue(ShaderLocationString(cValue) + "out ")

//Macro to specify the layout location/binding 'cValue' of a uniform/sampler
//Ex: ShaderUniform(0) vec3 foo; binds 'foo' as a uniform at layout location '0'
//Ex: ShaderSampler(0) sampler2d foo; binds 'foo' as a uniform at layout binding point '0'
#define ShaderUniform(cValue)            ShaderValue(ShaderLocationString(cValue)+ "uniform ")
#define ShaderSampler(cValue)            ShaderValue(ShaderBindingString(cValue) + "uniform ")

//Macro to specify the binding 'cValue' of a uniform block
//Ex: ShaderUniformBlock(0) foo { vec3 v1; vec3 v2; }; binds uniform block 'foo' to layout binding point '0' 
#define ShaderUniformBlock(cValue)       ShaderValue(ShaderUniformBlockString(cValue))
#define ShaderUniformBlockString(cValue) StringLiteral("layout(std140, binding=")+ToStringLiteral(static_cast<int>(cValue))+") uniform "

//Macro to specify the binding 'cValue' of a buffer block
//Ex: ShaderBufferBlock(0) foo { readonly vec3[]; } binds buffer block 'foo' to layout binding point '0'
#define ShaderBufferBlock(cValue)       ShaderValue(ShaderBufferBlockString(cValue))
#define ShaderBufferBlockString(cValue) StringLiteral("layout(std140, binding=")+ToStringLiteral(static_cast<int>(cValue))+") buffer "

//Shader Libraries are listed below. You can include them in source code using 'ShaderInclude'
//Ex: constexpr auto foo = Shader( ShaderInclude(ShaderConstants); )
// --------

constexpr StringLiteral ShaderConstants = Shader(
    float Pi() { return 3.1415926535897932; }
);

constexpr StringLiteral ShaderQuaternion = Shader( 
    
    ShaderInclude(ShaderConstants);

    vec4 qIdentity() { return vec4(0, 0, 0, 1); }
    vec4 qConjugate(vec4 q) { return vec4(-q.xyz, q.w); }
    
    vec4 qMul(vec4 p, vec4 q) {
        return vec4(p.w*q.x + p.x*q.w + p.y*q.z - p.z*q.y,
                    p.w*q.y + p.y*q.w + p.z*q.x - p.x*q.z,
                    p.w*q.z + p.z*q.w + p.x*q.y - p.y*q.x,
                    p.w*q.w - p.x*q.x - p.y*q.y - p.z*q.z);
    }
    
    //vec3 qRotate(vec3 v, vec4 q) { return qMul( qMul(q, vec4(v, 0)), qConjugate(q) ).xyz; }
    vec3 qRotate(vec3 v, vec4 q) {
        float x  = q.x * 2.;
        float y  = q.y * 2.;
        float z  = q.z * 2.;
        float xx = q.x * x;
        float yy = q.y * y;
        float zz = q.z * z;
        float xy = q.x * y;
        float xz = q.x * z;
        float yz = q.y * z;
        float wx = q.w * x;
        float wy = q.w * y;
        float wz = q.w * z;

        vec3 res;
        res.x = (1. - (yy + zz)) * v.x + (xy - wz) * v.y + (xz + wy) * v.z;
        res.y = (xy + wz) * v.x + (1. - (xx + zz)) * v.y + (yz - wx) * v.z;
        res.z = (xz - wy) * v.x + (yz + wx) * v.y + (1. - (xx + yy)) * v.z;
        return res;
    }

    //Warn: requires axis to be normalized
    vec4 qFromAngle(float angle, vec3 axis) {
        float theta = angle * .5;
        return vec4(sin(theta)*axis, cos(theta));
    }

    //Warn: requires destination and source to be normalized
    //Note: this returns identity vector if destination = zero or if destination = source
    vec4 qRotateTo(vec3 source, vec3 destination) {
    
        float cosTheta = dot(source, destination);
        
        //check to see if we rotated 180 degrees
        float epsilon = 0.001;
        if(cosTheta < (-1. + epsilon)) {

            vec3 perpendicular = cross(vec3(1, 0, 0), destination);
            if(dot(perpendicular.yz, perpendicular.yz) < epsilon) {
                perpendicular = cross(vec3(0, 1, 0), destination);
            }
            return qFromAngle(.5*Pi(), normalize(perpendicular));
        }
    
        //Note: using double angle trig identities
        //      the resulting quaternion = <vec4(sin(theta/2)*normalize(cross(source, destination)), cos(theta/2))>
    
        //Note: quaternion = identity if source and destination are colinear
        
        vec3 perpendicular = cross(source, destination);
        return normalize(vec4(perpendicular, 1. + cosTheta));
    }
    
    vec4 qLookAt(vec3 origin, vec3 target, vec3 eyeDirection) { return qRotateTo(eyeDirection,   normalize(target-origin)); }
    vec4 qLookAt(vec3 origin, vec3 target)                    { return qRotateTo(vec3(0, 0, -1), normalize(target-origin)); }
    
    vec4 qLerp(vec4 q1, vec4 q2, float t) { return normalize(((1. - t)*q1) + (t*q2)); }
    
    vec4 qSlerp(vec4 q1, vec4 q2, float t) {
    
        // Note: if q1 and q2 are 90+c > 90 degrees apart from each other
        //       we can get a shorter arc that represents the same
        //       rotation by using -q1 and q2 which are 90-c < 90 degrees apart
        // Note: q * v * q^-1 = (s*q) * v * (s*q)^-1
        float cosTheta = dot(q1, q2);
        if(cosTheta < 0.) {
            q1 = -q1;
            cosTheta = -cosTheta;
        }
    
        // q1 and q2 are ~colinear and acos is unstable so just lerp instead
        float epsilon = 0.001;
        if(cosTheta >= (1. - epsilon)) return qLerp(q1, q2, t);

        float theta = acos(cosTheta);
        float inverseSinTheta = inversesqrt(1. - (cosTheta*cosTheta));

        return ( (sin((1. - t)*theta)*q1) + (sin(t*theta)*q2) ) * inverseSinTheta;
    }
);

//TODO: this is math to convert ndc to world space -- might be useful sometime later
//"   float tz2 = projectionMatrix[2][2];"
//"   float tz3 = projectionMatrix[3][2];"
//"   float tw2 = projectionMatrix[2][3];"
//"   float tw3 = projectionMatrix[3][3];"
//
//    //ndc coords
//"   float nx = 1.;"
//"   float ny = 1.;"
//"   float nz = 1.;"
//"   vec4 clipCoords;"
////if w = 1 after coords are translated into clipspace
//"   clipCoords.w = (tw2*( (tz3 - (nz*tw3))/((nz*tw2) - tz2) )) + tz3;"
//"   clipCoords.xyz = vec3(nx, ny, nz) * clipCoords.w;"
//"   mat4 inverseProjectionMatrix = inverse(projectionMatrix);"
//"   vec4 unprojected = inverseProjectionMatrix * clipCoords;"
//"   vec3 worldSpace = transpose(inverse(mat3(viewMatrix))) * unprojected.xyz;"