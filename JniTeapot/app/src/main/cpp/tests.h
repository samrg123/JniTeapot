#pragma once

#include "util.h"

#if OPTIMIZED_BUILD
    #define TESTS_ENABLED 0
#else
    #define TESTS_ENABLED 1
#endif

#if TESTS_ENABLED

//Disable unused functions warnings in ide (ide doesn't know about crt init table)
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#define TEST_CONDITION(x) { RUNTIME_ASSERT(x, "TEST CONDITION FAILED for: '%s'", __PRETTY_FUNCTION__); }
#define TEST_FUNC(name) static CrtGlobalTestFunc Test_##name()

#include "mat.h"
TEST_FUNC(Mat4) {
    
    //Test mul
    {
        Mat4<float> m1({
                           1, 5, 9, 13,
                           2, 6, 10, 14,
                           3, 7, 11, 15,
                           4, 8, 12, 16
                       }
                      );
        
        Mat4<float> m2({
                           11, 13, 2, 3,
                           15, 5, 5, 1,
                           16, 7, 4, 8,
                           12, 8, 6, 6
                       }
                      );
        
        Mat4 m3=m1*m2;
        TEST_CONDITION(m3.a1 == 55);
        TEST_CONDITION(m3.a2 == 44);
        TEST_CONDITION(m3.a3 == 74);
        TEST_CONDITION(m3.a4 == 70);
        
        TEST_CONDITION(m3.b1 == 171);
        TEST_CONDITION(m3.b2 == 148);
        TEST_CONDITION(m3.b3 == 214);
        TEST_CONDITION(m3.b4 == 198);
        
        TEST_CONDITION(m3.c1 == 287);
        TEST_CONDITION(m3.c2 == 252);
        TEST_CONDITION(m3.c3 == 354);
        TEST_CONDITION(m3.c4 == 326);
        
        TEST_CONDITION(m3.d1 == 403);
        TEST_CONDITION(m3.d2 == 356);
        TEST_CONDITION(m3.d3 == 494);
        TEST_CONDITION(m3.d4 == 454);
    }
}

#include "GlTransform.h"
TEST_FUNC(GlTransform) {

    //Test NormalMatrix
    {
        GlTransform transform(Vec3(150.f, 767.f, 361.f),
                              Vec3(755.f, 373.f, 442.f),
                              Vec3(25.f,  433.f, 496.f));
        
        Mat4<float> nMatrix = transform.NormalMatrix();
        Mat4<float> nMatrixAns = transform.Matrix().Inverse().Transpose();

        //Note: NormalMatrix() ignores translation so we skip the check of d row
        
        TEST_CONDITION(Approx(nMatrix.a1, nMatrixAns.a1));
        TEST_CONDITION(Approx(nMatrix.b1, nMatrixAns.b1));
        TEST_CONDITION(Approx(nMatrix.c1, nMatrixAns.c1));
        //TEST_CONDITION(Approx(nMatrix.d1, nMatrixAns.d1));
        
        TEST_CONDITION(Approx(nMatrix.a2, nMatrixAns.a2));
        TEST_CONDITION(Approx(nMatrix.b2, nMatrixAns.b2));
        TEST_CONDITION(Approx(nMatrix.c2, nMatrixAns.c2));
        //TEST_CONDITION(Approx(nMatrix.d2, nMatrixAns.d2));
        
        TEST_CONDITION(Approx(nMatrix.a3, nMatrixAns.a3));
        TEST_CONDITION(Approx(nMatrix.b3, nMatrixAns.b3));
        TEST_CONDITION(Approx(nMatrix.c3, nMatrixAns.c3));
        //TEST_CONDITION(Approx(nMatrix.d3, nMatrixAns.d3));
    
        TEST_CONDITION(Approx(nMatrix.a4, nMatrixAns.a4));
        TEST_CONDITION(Approx(nMatrix.b4, nMatrixAns.b4));
        TEST_CONDITION(Approx(nMatrix.c4, nMatrixAns.c4));
        //TEST_CONDITION(Approx(nMatrix.d4, nMatrixAns.d4));
    }
}

#include "Quaternion.h"
TEST_FUNC(Quaternion) {
    
    //test multiplication
    {
        //Note: quaternions are (i,j,k, real)
        Quaternion<float> q1(9, 7, 8, 3),
                          q2(39, 36, 29, 61);

        Quaternion r = q1*q2;
        TEST_CONDITION(r.x == 581);
        TEST_CONDITION(r.y == 586);
        TEST_CONDITION(r.z == 626);
        TEST_CONDITION(r.w == -652);
    }

    //test rotation
    {
        Quaternion<float> r(581, 586, 626, -652);
        r.Normalize();
        
        Vec3<float> v(7.f, 3.f, 2.f);
        Quaternion<float> q(7.f, 3.f, 2.f, 0.f);
    
        Vec3 vPrime = r.ApplyRotation(v);
        Quaternion qPrime = r*q*r.Inverse();
        TEST_CONDITION(Approx(vPrime.x, qPrime.x));
        TEST_CONDITION(Approx(vPrime.y, qPrime.y));
        TEST_CONDITION(Approx(vPrime.z, qPrime.z));
        TEST_CONDITION(qPrime.w == 0); //just a sanity check, not part of test
    }
    
    //test to matrix func
    {
        Quaternion<float> r(581, 586, 626, -652);
        r.Normalize();

        Mat4<float> rMatrix = r.Matrix();
        TEST_CONDITION(Approx(rMatrix.a1,  0.0182872f));
        TEST_CONDITION(Approx(rMatrix.a2,  0.9995320f));
        TEST_CONDITION(Approx(rMatrix.a3, -0.0245217f));
        TEST_CONDITION(rMatrix.a4 == 0.0000000f);
    
        TEST_CONDITION(Approx(rMatrix.b1, -0.0903723f));
        TEST_CONDITION(Approx(rMatrix.b2,  0.0260779f));
        TEST_CONDITION(Approx(rMatrix.b3,  0.9955665f));
        TEST_CONDITION(rMatrix.b4 == 0.0000000f);
    
        TEST_CONDITION(Approx(rMatrix.c1,  0.9957401f));
        TEST_CONDITION(Approx(rMatrix.c2, -0.0159900f));
        TEST_CONDITION(Approx(rMatrix.c3,  0.0908069f));
        TEST_CONDITION(rMatrix.c4 == 0.0000000f);
    
        TEST_CONDITION(rMatrix.d1 == 0.f);
        TEST_CONDITION(rMatrix.d2 == 0.f);
        TEST_CONDITION(rMatrix.d3 == 0.f);
        TEST_CONDITION(rMatrix.d4 == 1.f);
    }
}


static CrtGlobalPreTestFunc InitTests() {
    Log("Testing code...");
}

static CrtGlobalPostTestFunc TestsCompleted() {
    Log("Finished Testing! all tests passed!");
}

#pragma clang diagnostic pop

#endif //TESTS_ENABLED