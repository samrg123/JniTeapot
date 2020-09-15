#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include "types.h"
#include "panic.h"

FT_Library ftlib;

static CrtGlobalInitFunc FtInit() {
    FT_Error error = FT_Init_FreeType(&ftlib);
    if(error) Panic("Failed to Initialize FreeType library - error: %d", error);
    Log("Initialed FreeType library at address: %p | ftLib handle: %p", ftlib, &ftlib);
}