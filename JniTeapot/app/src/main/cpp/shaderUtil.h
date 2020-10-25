#pragma once

#include "StringLiteral.h"

const StringLiteral kGlesVersionStr("#version 310 es\n");

#define ShaderLocation(x) StringLiteral("layout(location=")+ToString((unsigned int)x)+")"
#define ShaderBinding(x) StringLiteral("layout(binding=")+ToString((unsigned int)x)+")"

#define ShaderIn(x) ShaderLocation(x)+"in "
#define ShaderOut(x) ShaderLocation(x)+"out "

#define ShaderUniform(x) ShaderLocation(x)+"uniform "
#define ShaderSampler(x) ShaderBinding(x)+"uniform "

#define ShaderUniformBlock(x) "layout(std140, binding="+ToString((unsigned int)x)+") uniform "