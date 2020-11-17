#pragma once

#include "StringLiteral.h"
#include "macros.h"

#define DEBUG_SHADER_VERSION 1

#if DEBUG_SHADER_VERSION
    #define ShaderVersionStr StringLiteral("/*\n" __FILE__  ":" STRINGIFY(__LINE__) "\n*///\n#version 310 es\n")
#else
    const StringLiteral ShaderVersionStr("#version 310 es\n");
#endif

#define ShaderExtension(x) "#extension " x ":require\n"

#define ShaderLocation(x) StringLiteral("layout(location=")+ToString((unsigned int)x)+")"
#define ShaderBinding(x) StringLiteral("layout(binding=")+ToString((unsigned int)x)+")"

#define ShaderIn(x) ShaderLocation(x)+"in "
#define ShaderOut(x) ShaderLocation(x)+"out "

#define ShaderUniform(x) ShaderLocation(x)+"uniform "
#define ShaderSampler(x) ShaderBinding(x)+"uniform "

#define ShaderUniformBlock(x) StringLiteral("layout(std140, binding=")+ToString((unsigned int)x)+") uniform "

const StringLiteral ShaderIncludeConstants(
    STRINGIFY(
        float Pi() { return 3.1415926535897932; }
    )
);

const StringLiteral ShaderIncludeQuaternion(
    ShaderIncludeConstants+
    STRINGIFY(
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
    )
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