#pragma once

#include "customAssert.h"
#include "log.h"

#include "types.h"

#define TESTS_ENABLED 1
#if TESTS_ENABLED

//Disable unused functions warnings in ide (ide doesn't know about crt init table)
#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#define TEST_CONDITION(x) { RUNTIME_ASSERT(x, "TEST CONDITION FAILED for: '%s'", __PRETTY_FUNCTION__); }
#define TEST_FUNC(name) static CrtGlobalTestFunc Test_##name()

#include "mat.h"
TEST_FUNC(Mat4Mul) {
    
    Mat4<float> m1({
                       1, 5, 9,  13,
                       2, 6, 10, 14,
                       3, 7, 11, 15,
                       4, 8, 12, 16
                   });
    
    Mat4<float> m2({
                       11, 13, 2, 3,
                       15, 5,  5, 1,
                       16, 7,  4, 8,
                       12, 8,  6, 6
                   });
    
    Mat4 m3 = m1*m2;
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



static CrtGlobalPreTestFunc InitTests() {
    Log("Testing code...");
}

static CrtGlobalPostTestFunc TestsCompleted() {
    Log("Finished Testing! all tests passed!");
}

#pragma clang diagnostic pop

#endif //TESTS_ENABLED