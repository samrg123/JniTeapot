cmake_minimum_required(VERSION 3.4.1)

project(FreeType2.9 LANGUAGES C CXX)

# setup freetype to compile library
add_definitions(-DFT2_BUILD_LIBRARY)
add_definitions("-DFT_CONFIG_MODULES_H=<ftmodule.h>")

# Set the library sources - Note: taken from the original Freetype CMakeLists.txt
set(BASE_SRCS
        src/autofit/autofit.c
        src/base/ftbase.c
        src/base/ftbbox.c
        src/base/ftbdf.c
        src/base/ftbitmap.c
        src/base/ftcid.c
        src/base/ftfntfmt.c
        src/base/ftfstype.c
        src/base/ftgasp.c
        src/base/ftglyph.c
        src/base/ftgxval.c
        src/base/ftinit.c
        src/base/ftlcdfil.c
        src/base/ftmm.c
        src/base/ftotval.c
        src/base/ftpatent.c
        src/base/ftpfr.c
        src/base/ftstroke.c
        src/base/ftsynth.c
        src/base/ftsystem.c
        src/base/fttype1.c
        src/base/ftwinfnt.c
        src/bdf/bdf.c
        src/bzip2/ftbzip2.c
        src/cache/ftcache.c
        src/cff/cff.c
        src/cid/type1cid.c
        src/gzip/ftgzip.c
        src/lzw/ftlzw.c
        src/pcf/pcf.c
        src/pfr/pfr.c
        src/psaux/psaux.c
        src/pshinter/pshinter.c
        src/psnames/psnames.c
        src/raster/raster.c
        src/sfnt/sfnt.c
        src/smooth/smooth.c
        src/truetype/truetype.c
        src/type1/type1.c
        src/type42/type42.c
        src/winfonts/winfnt.c
        src/base/ftdebug.c
)

# build library
set(FREETYPE_LIB freetype)
set(FREETYPE_LIB ${FREETYPE_LIB} PARENT_SCOPE)
add_library(${FREETYPE_LIB} SHARED "${BASE_SRCS}")

# Specify library include directories - Warn must be done after we set library target
set(FREETYPE_INC "${PROJECT_SOURCE_DIR}/include")
set(FREETYPE_INC "${FREETYPE_INC}" PARENT_SCOPE)
target_include_directories(${FREETYPE_LIB} PRIVATE ${FREETYPE_INC})
target_include_directories(${FREETYPE_LIB} PRIVATE ${FREETYPE_INC}/freetype/config)
