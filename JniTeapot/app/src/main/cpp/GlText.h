#pragma once

#include <unistd.h>
#include <alloca.h>
#include "Ft.h"

#include "GlContext.h"
#include "types.h"
#include "customAssert.h"
#include "FileManager.h"

#include "vec.h"
#include "Rectangle.h"

class GlText {

    public:

        static constexpr Vec2<float> kDefaultStringAttribScale = Vec2(1.f, 1.f);
        static constexpr uint32 kDefaultStringAttribRGBA = RGBA(0u, 0, 0);
        static constexpr float kDefaultStringAttribDepth = -1.f;
        
        struct alignas(8) StringAttrib {
            Vec2<float> scale = kDefaultStringAttribScale;
            uint32 rgba = kDefaultStringAttribRGBA;
            float depth = kDefaultStringAttribDepth; //Note: must be in range [-1, 1]
        };

    private:

        static constexpr uchar kDefaultStartChar = 32; //space
        static constexpr uchar kDefaultEndChar = 126;  //tilde
        static constexpr uchar kDefaultUnknownChar = '?';
        
        static constexpr StringAttrib kDefaultRenderStringAttrib = {
            .scale = kDefaultStringAttribScale,
            .rgba = kDefaultStringAttribRGBA,
            .depth = kDefaultStringAttribDepth,
        };
        
        static constexpr uint32 kDefaultExtraStringAttrib = 100;                // number of string attributes per draw call
        static constexpr uint32 kMaxStringAttribSize = (MaxUint32() >> 8) + 1;  // lower 8 bits of stringAttribBufferIndex is used for glyphIndex in vertexShader
        
        static constexpr uint8 kDpi = 72; //freetypes default font resolution
        static constexpr float kPixelsPerAdvance = 1.f/64; //freetype reports advance in 26:6 fixed point format
        static constexpr float kPixelsPerBearing = 1.f/64; //freetype reports bearing in 26:6 fixed point format
        
        // in pixels
        static constexpr uint16 kDefaultGlyphSize = 50;
        static constexpr uint32 kMinTextureSize = 128;
        static constexpr uint8 kTexturePadding = 1; // gutter so texture scaling doesn't have artifacts
        static constexpr short kMaxTargetSizeDelta = 10;
        
        
        //Warn: VertexAttribs values must match the locations in the vertex shader
        enum Attribs { AttribPosition,  AttribSCI };
        enum Uniforms { UScreenSize, UInverseTextureSize };
        enum SharedBlocks { SBlockVertexGlyphData, SBlockStringAttribData };
        enum TextureUnits { TUnitFont };
    
        static constexpr const char* kVertexShaderSource =  "#version 310 es\n"
                                                            "struct VertexGlyphData {"
                                                            "   highp vec2 textureCoordinates;"
                                                            "   highp vec2 size;"
                                                            "};"
                                                            ""
                                                            "struct StringAttrib {"
                                                            "   highp vec2 scale;"
                                                            "   highp uint rgba;"
                                                            "   highp float depth;"
                                                            "};"
                                                            ""
                                                            //Note: std430 ensures that alignment is 8 bytes
                                                            "layout(std430, binding = 0) buffer VertexGlyphDataBuffer {"
                                                            "   readonly VertexGlyphData glyphData[];"
                                                            "};"
                                                            ""
                                                            //Note: std430 ensures ordering and that alignment is 8 bytes
                                                            "layout(std430, binding = 1) buffer StringAttribBuffer {"
                                                            "   readonly StringAttrib stringAttribData[];"
                                                            "};"
                                                            ""
                                                            "layout(location = 0) uniform vec2 screenSize;"
                                                            "layout(location = 1) uniform vec2 inverseTextureSize;"
                                                            ""
                                                            "layout(location = 0) in highp vec2 position;"
                                                            "layout(location = 1) in highp uint SCI;" //stringAttribIndex[24]:glyphDataIndex[8]
                                                            ""
                                                            "smooth out vec2 textureCoordinates;"
                                                            "flat   out vec4 fontColor;" //Note: don't waste time interpolating color - its the same across the whole quad
                                                            ""
                                                            "void main() {"
                                                            ""
                                                            "" // unpack indexes
                                                            "   uint glyphIndex        =  SCI & 0xFFU;"
                                                            "   uint stringAttribIndex =  SCI >> 8;"
                                                            ""
                                                            "" // fetch scale/color/depth attributes
                                                            "   VertexGlyphData gData = glyphData[glyphIndex];"
                                                            "   StringAttrib stringAttrib = stringAttribData[stringAttribIndex];"
                                                            ""
                                                            "" // set font color
                                                            "   fontColor = (1.f/255.f) * "
                                                            "               vec4(float(stringAttrib.rgba>>24), "
                                                            "                    float((stringAttrib.rgba>>16) & 0xFFU), "
                                                            "                    float((stringAttrib.rgba>>8) & 0xFFU), "
                                                            "                    float(stringAttrib.rgba & 0xFFU)"
                                                            "               );"
                                                            ""
                                                            ""  // compute vertex coordinates - note 'position' is upper left of glyph
                                                            "   gl_Position = vec4(position.x, position.y, stringAttrib.depth, 1);"
                                                            "   textureCoordinates = gData.textureCoordinates;"
                                                            ""
                                                            "   if((gl_VertexID&1) != 0) {"
                                                            "       gl_Position.x+= stringAttrib.scale.x * gData.size.x;"
                                                            "       textureCoordinates.x+= gData.size.x;"
                                                            "   }"
                                                            ""
                                                            "   if((gl_VertexID&2) != 0) {"
                                                            "       gl_Position.y+= stringAttrib.scale.y * gData.size.y;"
                                                            "       textureCoordinates.y+= gData.size.y;"
                                                            "   }"
                                                            ""
                                                            ""  // normalize texture coordinates
                                                            "   textureCoordinates*= inverseTextureSize;"
                                                            ""
                                                            "" // map screen space to clip space - Note: screen space origin is upper left
                                                            "   gl_Position.x =  2.f*gl_Position.x / screenSize.x - 1.f;"
                                                            "   gl_Position.y = -2.f*gl_Position.y / screenSize.y + 1.f;"
                                                            ""
                                                            "}";

        static constexpr const char* kFragmentShaderSource =    "#version 310 es\n"
                                                                "precision mediump float;"
                                                                ""
                                                                "layout(binding = 0) uniform sampler2D fontSampler;"
                                                                ""
                                                                "smooth in vec2 textureCoordinates;"
                                                                "flat   in vec4 fontColor;"
                                                                ""
                                                                "layout(location = 0) out vec4 fragColor;"
                                                                ""
                                                                "void main() {"
                                                                "   float tColor = texture(fontSampler, textureCoordinates).r;"
                                                                //Warn: this is expensive!!! (halves performance on moto G6) - acts as a hack to enable z-testing on transparent pixels, but prevents fragment culling [really should sort things with painters algorithm!]
                                                                "   if(tColor <= .5f) discard;"
                                                                ""
                                                                "   fragColor.rgb = (tColor > 0.f) ? fontColor.rgb : vec3(0,0,0);"
                                                                "   fragColor.a = fontColor.a*tColor;"
                                                                "}";
           
        struct GlyphData {
            Vec2<float> advance;  //stride between glyphs
            Vec2<float> hBearing; //negative offset from baseline to left/top while walking horizontal
            Vec2<float> vBearing; //offset from baseline to left/top while walking down
        };
        
        struct GlyphSortData {
            Rectangle<uint32> paddedTextureCoords;
            uint32 area;
            uint8 character;
            
            static void Insert(GlyphSortData* arry, uint8 arryLength, const GlyphSortData& value) {
                
                // sort glyphs by decreasing area
                uint8 insertIndex = BinarySearchUpper(value.area, arry, arryLength,
                                                      [](uint32 area, const GlyphSortData &data) { return area > data.area; }
                                                     );
    
                //shift over existing values and insert
                for(uint8 i = arryLength; i > insertIndex; --i) arry[i] = arry[i - 1];
                arry[insertIndex] = value;
            }
        };
        
        struct alignas(8) VertexGlyphData {
            Vec2<float> texCoords;
            Vec2<float> size;
        };
        
        struct VertexAttributeData {
            Vec2<float> position;
            uint32 SCI;
        };
        
        FT_Face face;
        
        Memory::Arena memoryArena;
        Memory::Region baseRegion, stringRegion;

        FileManager::AssetBuffer* fontAssetBuffer;
        GlyphData* offsetGlyphData;
        StringAttrib* stringAttribData;
        
        GLint maxTextureSize, maxSharedBlockSize;
        GLuint glProgram, fontSampler;
        
        union {
            GLuint glBuffers[3];
            struct {
                GLuint vertexGlyphDataBuffer,
                       stringAttribBuffer,
                       vertexAttributeBuffer;
            };
        };

        union {
            GLuint glTextures[1];
            struct {
                GLuint fontTexture;
            };
        };
        
        GLuint vao;
        uint32 vertexAttributeBufferBytes, pushedBytes;
        uint32 stringAttribSize, stringAttribBitIndex, uploadStringAttribBitIndex;
        
        uchar startChar, endChar, unknownChar;
        
        inline
        bool SetFontFixedSize(uint16 targetGlyphSize) {
    
            //TODO: see if fixedBitmaps are sorted so we can search faster...
    
            //scan for closest fixed bitmap size larger than our target size
            FT_Bitmap_Size* fixedBitmaps = face->available_sizes;
            for(FT_Int i = 0, length = face->num_fixed_sizes; i < length; ++i) {
        
                //Log("BmpWidth: %d", fixedBitmaps[i].width);
        
                short bmpWidth = fixedBitmaps[i].width;
                if(bmpWidth > targetGlyphSize) {
            
                    short delta = bmpWidth - targetGlyphSize;
                    if(delta <= kMaxTargetSizeDelta) {
                
                        //use these bmps for the glyph

                        FT_Error ftError;
                        RUNTIME_ASSERT(!(ftError = FT_Select_Size(face, i)),
                                       "Failed to select glyph size { ftError %d, face: %p, faceName: %s, targetSize: %d, fixedSizeDelta: %d, fixedSizeIndex: %d }",
                                       ftError, face, face->family_name, targetGlyphSize, delta, i);
                
                        Log("Using Fixed bitmap { face: %p, faceName: %s, bmpSize: %d, targetSize: %d, kMaxTargetSizeDelta: %d }",
                            face, face->family_name, bmpWidth, targetGlyphSize, kMaxTargetSizeDelta);
                
                        return true;
                    }
                }
            }
            return false;
        }
        
        inline
        void SetFontSize(uint16 targetGlyphSize) {
    
            if(!SetFontFixedSize(targetGlyphSize)) {

                // Render new bitmaps at the requested scale
                FT_Error ftError;
                RUNTIME_ASSERT(!(ftError = FT_Set_Pixel_Sizes(face, targetGlyphSize, targetGlyphSize)),
                               "Failed to set font glyph size { ftError %d, face: %p, faceName: %s, targetSize: %d }",
                               ftError, face, face->family_name, targetGlyphSize);
        
                Log("Rendering font bitmap { face: %p, faceName: %s, fixedSizes: %p, targetSize: %d, kMaxTargetSizeDelta: %d }",
                    face, face->family_name, face->available_sizes, targetGlyphSize, kMaxTargetSizeDelta);
            }
        }
        
        inline
        void SetupMemoryArena(uint32 stringAttribCount, uint8 numGlyphs) {

            //Note: this is more than just sanity - makes sure 'Clear()' was called before this so user knows everything will be wiped
            //Warn: if this isn't enforced zero 'pushedBytes' and 'stringAttribBitIndex' before preceding
            RUNTIME_ASSERT(memoryArena.IsEmptyRegion(stringRegion),
                           "Nonempty string Region. GlText must be cleared before rendering a new texture { memoryArena: %p, stringRegion: %p } ",
                           &memoryArena, &stringRegion);
                
            uint32 glyphDataBytes    = sizeof(GlyphData)  * numGlyphs,
                   stringAttribBytes = sizeof(StringAttrib) * stringAttribCount,
                   fixedMemoryBytes  = glyphDataBytes + stringAttribBytes;
    
            memoryArena.FreeRegion(baseRegion);
    
            GlyphData* glyphData = (GlyphData*)memoryArena.PushBytes(fixedMemoryBytes);
            stringAttribData = (StringAttrib*)ByteOffset(glyphData, glyphDataBytes);
            offsetGlyphData = glyphData - startChar;
        
            stringRegion = memoryArena.CreateRegion();
        }

        inline
        uint32 TargetTextureSize(uint32 numGlyphs) {
        
            //approximate a square bbox texture
            FT_UShort unitsPerEm = face->units_per_EM;
            uint32 paddedBBoxWidth  = CeilFraction((face->bbox.xMax - face->bbox.xMin)*kDpi, unitsPerEm) + kTexturePadding,
                   paddedBBoxHeight = CeilFraction((face->bbox.yMax - face->bbox.yMin)*kDpi, unitsPerEm) + kTexturePadding;
    
            uint32  bboxTextureBytes = numGlyphs*paddedBBoxWidth*paddedBBoxHeight,
                    bboxTextureSize  = Pow2RoundDown(FastSqrt(bboxTextureBytes));
    
            bboxTextureSize = (bboxTextureSize <= kMinTextureSize) ? kMinTextureSize : Min(bboxTextureSize, maxTextureSize);
            RUNTIME_ASSERT(bboxTextureSize >= paddedBBoxWidth,
                           "Computed bbox textureSize is smaller bbox character size { bboxTextureSize: %u, paddedBBoxWidth: %u, paddedBBoxHeight: %u }",
                           bboxTextureSize, paddedBBoxWidth, paddedBBoxHeight);
    
            return bboxTextureSize;
        }
        
        struct TextureMetrics {
            Vec2<uint32> maxSize;
            Vec2<uint32> size;
            uint32 minBytes; //Note: this is just for logging the packing efficiency - TODO: strip this out if logging is disabled!
        };
        
        void LoadGlyphData(uint32 targetTextureSize, GlyphSortData* glyphSortData, TextureMetrics* textureMetrics) {

            FT_Error ftError;
            FT_GlyphSlot glyphSlot = face->glyph;

            Vec2<uint32> maxTextureSize(0, 0), rowSize(0, 0);
            uint32 minTextureBytes = 0;
                
            //get font metrics and compute maximum texture size by naive packing characters in rows
            for(uchar i = startChar; i <= endChar; ++i) {
        
                RUNTIME_ASSERT(!(ftError = FT_Load_Char(face, i, FT_LOAD_BITMAP_METRICS_ONLY)),
                               "Failed to load glyph metrics { ftError: %d, face: %p, faceName: %s, char: '%c'[%d] }",
                               ftError, face, face->family_name, i, i);
        
                int32 width  = glyphSlot->bitmap.width,
                      height = glyphSlot->bitmap.rows;
        
                GlyphData* data = offsetGlyphData + i;
        
                data->advance.x = glyphSlot->metrics.horiAdvance * kPixelsPerAdvance;
                data->advance.y = glyphSlot->metrics.vertAdvance * kPixelsPerAdvance;
    
                //Note: we flip negate hBearing.y so that we can simd FMADD to get coordinates
                data->hBearing.x = glyphSlot->metrics.horiBearingX * kPixelsPerBearing;
                data->hBearing.y = -(glyphSlot->metrics.horiBearingY * kPixelsPerBearing);
        
                data->vBearing.x = glyphSlot->metrics.vertBearingX * kPixelsPerBearing;
                data->vBearing.y = glyphSlot->metrics.vertBearingY * kPixelsPerBearing;
        
                uint32  paddedWidth  = width  + kTexturePadding,
                        paddedHeight = height + kTexturePadding,
                        paddedArea   = paddedWidth*paddedHeight;
    
                //insert character in sortData
                GlyphSortData::Insert(glyphSortData, i - startChar, { .paddedTextureCoords = Rectangle(0u, 0u, paddedWidth, paddedHeight), .area = paddedArea, .character = i });
                
                //advanced to new row
                if((rowSize.x+= paddedWidth) > targetTextureSize) {
            
                    maxTextureSize.y+= rowSize.y;
    
                    rowSize.x = paddedWidth;
                    rowSize.y = paddedHeight;
                
                } else if(rowSize.y < paddedHeight) rowSize.y = paddedHeight;

                if(maxTextureSize.x < rowSize.x) maxTextureSize.x = rowSize.x;
                minTextureBytes+= paddedArea;
            }
            maxTextureSize.y+= rowSize.y; // add on last row
            
            textureMetrics->minBytes = minTextureBytes;
            textureMetrics->maxSize = maxTextureSize;
        }
        
        void PackGlyphs(void* texture, TextureMetrics* textureMetrics, GlyphSortData* glyphSortData, uint8 numGlyphs) {
        
            // map VertexGlyphData into memory
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexGlyphDataBuffer);
            GlAssertNoError("Failed to bind vertexGlyphDataBuffer: %u ", vertexGlyphDataBuffer);
    
            GLuint vgBytes = numGlyphs*sizeof(VertexGlyphData);
            glBufferData(GL_SHADER_STORAGE_BUFFER, vgBytes, nullptr, GL_STATIC_DRAW);
            GlAssertNoError("Failed to allocate vertexGlyphDataBuffer { vgBytes: %u [%u glyphs] } ", vgBytes, numGlyphs);
    
            VertexGlyphData* offsetVgData = (VertexGlyphData*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, vgBytes, GL_MAP_WRITE_BIT) - startChar;
            

            FT_Error ftError;
            FT_GlyphSlot glyphSlot = face->glyph;
    
            uint32 textureWidth = textureMetrics->size.x,
                   textureHeight = 0;
    
            //TODO: MAKE THIS BETTER!
            uint32 minRow0Height = MaxUint32();
            
            for(uchar i = 0; i < numGlyphs; ++i) {
        
                GlyphSortData* sData = glyphSortData + i;
                Rectangle<uint32>& sRect = sData->paddedTextureCoords;
        
                int32 sWidth  = sRect.Width();
        
                // get open texture spot
                int32 rowAdvanceHeight = minRow0Height;
                for(uchar j = 0; j < i;) {
            
                    Rectangle<uint32>& jRect = glyphSortData[j].paddedTextureCoords;
                    if(sRect.IntersectsArea(jRect)) {
                
                        //advance along line
                        sRect.right = jRect.right + sWidth;
                        if(sRect.right <= textureWidth) sRect.left = jRect.right;
                        else {
                    
                            //advance to nexRow
                            sRect.left = 0;
                            sRect.right = sWidth;
                            sRect.top+= rowAdvanceHeight;
                            sRect.bottom+= rowAdvanceHeight;
                    
                            // TODO: THINK OF FASTER WAY FOR SCANNING DOWN!
                            // if we get to then end of this row and can't fit
                            // anything keep advancing 1px down until we can
                            rowAdvanceHeight = 1;
                        }
                
                        j = 0; // restart scan
                
                    } else ++j; // scan next
                }
        
                // TODO: introduce valve distance field fonts for less blury scaling
    
                //set vgData
                offsetVgData[sData->character] = {
                    .texCoords = Vec2(sRect.left, sRect.top),
                    .size      = Vec2(sRect.Width(), sRect.Height()) - kTexturePadding
                };
                
                //draw the glyph - Note: the default for FT_LOAD_RENDER which uses FT_RENDER_MODE_NORMAL which is 8bit anti-aliased grayscale
                RUNTIME_ASSERT(!(ftError = FT_Load_Char(face, sData->character, FT_LOAD_RENDER)),
                               "Failed to render font glyph bitmap { ftError: %d, face: %p, faceName: %s, char: '%c'[%d], area: %d }",
                               ftError, face, face->family_name, sData->character, sData->character, sData->area);
        
                //Note: pitch changes sign to match bitmap y-flipping
                int glyphPitch  = glyphSlot->bitmap.pitch,
                    glyphWidth  = glyphSlot->bitmap.width,
                    glyphHeight = glyphSlot->bitmap.rows;

                uint8 *glyphMem  = glyphSlot->bitmap.buffer,
                      *packedMem = (uint8*)ByteOffset(texture, textureWidth*sRect.top + sRect.left);
        
                //Copy glyph over to texture
                // TODO: look into providing our own RasterParams with custom raster callback to avoid doing this copy
                for(uint32 r = 0; r < glyphHeight; ++r) {
                    memcpy(packedMem, glyphMem, glyphWidth);
                    glyphMem+= glyphPitch;
                    packedMem+= textureWidth;
                }
                
                //Note: texture doesn't need padding on the bottom, but intermediate rows do
                if(textureHeight < sRect.bottom) textureHeight = sRect.bottom;
    
                //TODO: this only helps with first row - find way to make this better!
                if(sRect.bottom < minRow0Height) minRow0Height = sRect.bottom;
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            
            textureMetrics->size.y = textureHeight;
        }
        
        inline
        void UploadRenderedTexture(void* fontTextureData, Vec2<uint32> fontTextureSize) {
    
            glUseProgram(glProgram);
            Vec2 inverseFontTextureSize = ((Vec2<float>)fontTextureSize).Inverse();
            glUniform2fv(UInverseTextureSize, 1, &inverseFontTextureSize);
        
            glDeleteTextures(ArrayCount(glTextures), glTextures);
            GlAssertNoError("Failed to Delete old textures");
    
            glGenTextures(ArrayCount(glTextures), glTextures);
            GlAssertNoError("Failed to Generate new textures");
    
            glBindTexture(GL_TEXTURE_2D, fontTexture);
            GlAssertNoError("Failed to bind fontTexture");
    
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, fontTextureSize.x, fontTextureSize.y, 0, GL_RED, GL_UNSIGNED_BYTE, fontTextureData);
            GlAssertNoError("Failed to upload fontTexture { texture: %d, textureWidth: %u, textureHeight: %u }",
                            fontTexture, fontTextureSize.x, fontTextureSize.y);
            
        }
      
        void AllocateStringAttribBuffer(uint32 size) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, stringAttribBuffer);
            GlAssertNoError("Failed to bind stringAttribBuffer: %u", stringAttribBuffer);
    
            //Note: index 0 is default scale color so gpu buffer needs to be params.stringAttribSize+1 elements
            GLuint scBytes = size * sizeof(StringAttrib);
            glBufferData(GL_SHADER_STORAGE_BUFFER, scBytes, nullptr, GL_DYNAMIC_DRAW);
            GlAssertNoError("Failed to allocate stringAttribBuffer { scBytes: %u } ", scBytes);
        }
        
        void BindAndAllocateVertexAttributeBufferBytes(uint32 bytes) {
            
            glBindBuffer(GL_ARRAY_BUFFER, vertexAttributeBuffer);
            glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_DYNAMIC_DRAW);

            GlAssertNoError("Failed to allocate vertexAttributeBuffer { buffer: %u, oldBytes: %u, oldNumChars: %u, newBytes: %u, newNumChars: %u } ",
                             vertexAttributeBuffer, vertexAttributeBufferBytes, vertexAttributeBufferBytes/sizeof(VertexAttributeData), bytes, bytes/sizeof(VertexAttributeData));
    
            Log("Allocated vertexAttributeBuffer { oldBytes: %u, oldNumChars: %u, newBytes: %u, newNumChars: %u }",
                vertexAttributeBufferBytes, vertexAttributeBufferBytes/sizeof(VertexAttributeData), bytes, bytes/sizeof(VertexAttributeData));
            
            vertexAttributeBufferBytes = bytes;
        }
        
        //Note: lower 8 bits are reserved for glyphIndex
        inline constexpr uint32 StringAttribBitIndexIncrement() { return 1 << 8; }
        inline uint32 StringAttribBitIndex(uint32 saBits) { return saBits >> 8; }
        inline uint32 StringAttribIndex() { return StringAttribBitIndex(stringAttribBitIndex); }
        
    public:
        
        inline
        void UpdateScreenSize(int32 width, int32 height) {
            glUseProgram(glProgram);
            glUniform2f(UScreenSize, width, height);
            GlAssertNoError("Failed to set screenSize uniform { glProgram: %u, width: %d, height: %d }",
                            glProgram, width, height);
        }
        
        inline void SetStringAttrib(const StringAttrib& stringAttrib) {

            // add one to current index
            stringAttribBitIndex+= StringAttribBitIndexIncrement();
            uint32 scdIndex = StringAttribIndex();
            RUNTIME_ASSERT(scdIndex < stringAttribSize, "stringAttrib buffer overflow { stringAttribSize: %u }", stringAttribSize);
    
            stringAttribData[scdIndex] = stringAttrib;
        }
        
        inline void SetColor(uint32 rgba) {
            SetStringAttrib({
                .scale = stringAttribData[StringAttribIndex()].scale,
                .rgba = rgba,
                .depth = stringAttribData[StringAttribIndex()].depth
            });
        }

        inline void SetDepth(float depth) {
            SetStringAttrib({
                .scale = stringAttribData[StringAttribIndex()].scale,
                .rgba = stringAttribData[StringAttribIndex()].rgba,
                .depth = depth
            });
        }

        inline void SetScale(Vec2<float> scale) {
            SetStringAttrib({
                .scale = scale,
                .rgba = stringAttribData[StringAttribIndex()].rgba,
                .depth = stringAttribData[StringAttribIndex()].depth
            });
        }
        
        GlText(GlContext* context, const char* assetPath, int fontIndex = 0): glTextures{}, pushedBytes{0}, stringAttribBitIndex{0}, uploadStringAttribBitIndex{0} {
            
            //load font into memory
            fontAssetBuffer = FileManager::OpenAsset(assetPath, &memoryArena);
    
            baseRegion = memoryArena.CreateRegion();
            stringRegion = memoryArena.CreateRegion();
            
            // WARNING: This requires fontBuffer to stay valid across lifetime of text object
            FT_Error ftError;
            RUNTIME_ASSERT(!(ftError = FT_New_Memory_Face(ftlib, fontAssetBuffer->data, fontAssetBuffer->size, fontIndex, &face)),
                           "Failed to load font face { ftError: %d, assetPath: %s, fontIndex: %d }",
                           ftError, assetPath, fontIndex);
            
            
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
            RUNTIME_ASSERT(maxTextureSize >= kMinTextureSize,
                           "Max texture size: %d, is smaller than min texture size: %d",
                           maxTextureSize, kMinTextureSize);
    
            glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSharedBlockSize);
    
            glProgram = GlContext::CreateGlProgram(kVertexShaderSource, kFragmentShaderSource);
        
            ////NOTE: DEBUGGING
            //GlContext::PrintVariables(glProgram);

            //Note: must be called after we compile program
            UpdateScreenSize(context->Width(), context->Height());
    
            //setup sampler
            glGenSamplers(1, &fontSampler);
            glSamplerParameteri(fontSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(fontSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(fontSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(fontSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    
            //generate buffers
            glGenBuffers(ArrayCount(glBuffers), glBuffers);
            GlAssertNoError("Failed to generate glBuffers");
    
            //setup glyphData buffer
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexGlyphDataBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SBlockVertexGlyphData, vertexGlyphDataBuffer);
            
            //setup stringAttrib buffer
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, stringAttribBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SBlockStringAttribData, stringAttribBuffer);
            
            //setup attribute buffer
            vertexAttributeBufferBytes = 0;
            
            //setup vao
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
    
            glEnableVertexAttribArray(AttribSCI);
            glEnableVertexAttribArray(AttribPosition);
            
            glBindBuffer(GL_ARRAY_BUFFER, vertexAttributeBuffer); //Note: this doesn't modify vao state so we still need to rebind it on draw!
            glVertexAttribPointer(AttribPosition, 2, GL_FLOAT,        GL_FALSE, sizeof(VertexAttributeData), (void*)offsetof(VertexAttributeData, position));
            glVertexAttribIPointer(AttribSCI,     1, GL_UNSIGNED_INT,           sizeof(VertexAttributeData), (void*)offsetof(VertexAttributeData, SCI));
            
            //advance each attribute once per instance [character]
            glVertexAttribDivisor(AttribSCI, 1);
            glVertexAttribDivisor(AttribPosition, 1);
    
            glBindVertexArray(0);
    
            Log("Initialized GlText { screenWidth: %d, screenHeight: %d, maxTextureSize: %u, glProgram: %u, fontSampler: %u, vertexGlyphDataBuffer: %u, vertexAttributeBuffer: %u }",
                context->Width(), context->Height(), maxTextureSize, glProgram, fontSampler, vertexGlyphDataBuffer, vertexAttributeBuffer);
        }
        
        inline
        ~GlText() {
            FT_Done_Face(face); //Warn: Must be called before we free memory - uses font stored in memoryArena
            
            glDeleteTextures(ArrayCount(glTextures), glTextures);
            glDeleteBuffers(ArrayCount(glBuffers), glBuffers);
    
            // TODO: make this static so that they persist across the whole program
            //      and initilize them with some GlText::Init call
            glDeleteProgram(glProgram);
            glDeleteVertexArrays(1, &vao);
            glDeleteSamplers(1, &fontSampler);
        }

        // Note: vertexAttributeBuffer grows to fit largest draw call so we don't have to keep allocating a new one each draw call
        //       this can be called with numChars=0 to purge the buffer from memory and recreate a new one or preallocate a large buffer up front
        //       to prevent dynamically resizing
        void AllocateBuffer(uint32 numChars) { BindAndAllocateVertexAttributeBufferBytes(numChars * sizeof(vertexAttributeBuffer)); }

        struct RenderParams {
            StringAttrib renderStringAttrib = kDefaultRenderStringAttrib;
            uint32 extraStringAttrib = kDefaultExtraStringAttrib;
            uint16 targetGlyphSize = kDefaultGlyphSize;
    
            uchar startChar   = kDefaultStartChar,
                  endChar     = kDefaultEndChar,
                  unknownChar = kDefaultUnknownChar;
        };
        
        template<typename... ArgT>
        void PushString(Vec2<float> position, const char* fmt, ArgT... args) {
            RUNTIME_ASSERT(fontTexture, "No font texture - Call RenderTexture first");
            
            int strChars = snprintf(nullptr, 0, fmt , args...),
                strBytes = strChars+1;
        
            Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
            char* str = (char*)Memory::temporaryArena.PushBytes(strBytes);
            sprintf(str, fmt, args...);
    
            uint32 attributeBytes = strChars*sizeof(VertexAttributeData);
            VertexAttributeData* attributeData = (VertexAttributeData*)memoryArena.PushBytes(attributeBytes);
            pushedBytes+= attributeBytes;
            
            Vec2 scale = stringAttribData[StringAttribIndex()].scale;
            
            for(int i = 0; i < strChars; ++i) {

                uint8 c = str[i];
                if(c < startChar || c > endChar) {
                    Warn("Character: '%c'[%d] is not a rendered character { startChar: '%c'[%d], endChar: '%c'[%d] }",
                        c,c, startChar,startChar, endChar,endChar);
    
                    c = unknownChar;
                }
                
                //do some metric magic;
                GlyphData& gData = offsetGlyphData[c];
    
                attributeData[i] = {
                    .position = scale*gData.hBearing + Vec2(position.x, position.y),
                    .SCI = stringAttribBitIndex | (c - startChar)
                };
                
                position.x+= scale.x * gData.advance.x;
            }
            
            Memory::temporaryArena.FreeRegion(tmpRegion);
        }
        
        void RenderTexture(const RenderParams& params) {
            
            startChar = params.startChar;
            endChar = params.endChar;
            RUNTIME_ASSERT( startChar <= endChar,
                            "startChar: '%c'[%d] must be less than or equal to endChar: '%c'[%d] { face: %s }",
                            startChar, startChar, endChar, endChar, face->family_name);
            
            unknownChar = params.unknownChar;
            RUNTIME_ASSERT(InRange(unknownChar, startChar, endChar),
                           "unknownChar: '%c'[%d] must be between startChar: '%c'[%d] and endChar: '%c'[%d] { face: %s } ",
                           unknownChar, unknownChar, startChar,startChar, endChar,endChar, face->family_name);
    
            stringAttribSize = params.extraStringAttrib + 1;
            RUNTIME_ASSERT(stringAttribSize <= kMaxStringAttribSize,
                           "stringAttribSize exceeds maximum { stringAttribSize: %u, kMaxStringAttribSize: %u }",
                           stringAttribSize, kMaxStringAttribSize);
    
            uint32 stringAttribBytes = sizeof(StringAttrib)*stringAttribSize;
            RUNTIME_ASSERT(stringAttribBytes < maxSharedBlockSize,
                           "stringAttribBytes exceeds maxSharedBlockSize { stringAttribBytes: %u, maxSharedBlockSize: %u }",
                           stringAttribBytes, maxSharedBlockSize);
            
    
            // prepare fixed memory
            uint8 numGlyphs = endChar - startChar + 1; //+1 to include the end char
            SetupMemoryArena(stringAttribSize, numGlyphs);
            AllocateStringAttribBuffer(stringAttribSize);
    
            // prepare tmp memory
            Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
            GlyphSortData* glyphSortData = (GlyphSortData*)Memory::temporaryArena.PushBytes(sizeof(GlyphSortData) * numGlyphs);
            
            // set default stringAttrib
            *stringAttribData = params.renderStringAttrib;
            uploadStringAttribBitIndex = 0; // Note: upload default scale next draw call
    
            
            // get glyph texture size s
            SetFontSize(params.targetGlyphSize);
            uint32 targetTextureSize = TargetTextureSize(numGlyphs); //Note: variable so we can log it
    
            // Get glyph metrics - Note: 'LoadGlyphData' sets textureMetrics.maxSize
            TextureMetrics textureMetrics;
            LoadGlyphData(targetTextureSize, glyphSortData, &textureMetrics);
            
            // set the texture width to a power of 2 if possible to improve performance
            uint32 pow2MaxTextureWidth = Pow2RoundUp(textureMetrics.maxSize.x);
            textureMetrics.size.x = (pow2MaxTextureWidth <= maxTextureSize) ? pow2MaxTextureWidth : maxTextureSize;

            // pack the glyph bitmaps into tmpTexture
            // Note: 'PackGlyphs' requires 'textureMetrics.size.x' and sets 'textureMetrics.size.y'
            uint32 maxTextureBytes = textureMetrics.size.x * textureMetrics.maxSize.y;
            void* tmpTexture = Memory::temporaryArena.PushBytes(maxTextureBytes);
            PackGlyphs(tmpTexture, &textureMetrics, glyphSortData, numGlyphs);
            
            // set the texture height to a power of 2 if possible to improve performance
            uint32 minTextureHeight = textureMetrics.size.y, //Note: var polled out so we can log it
            pow2TextureHeight = Pow2RoundUp(minTextureHeight);
            if(pow2TextureHeight <= maxTextureSize) textureMetrics.size.y = pow2TextureHeight;
    
            // upload to GPU
            UploadRenderedTexture(tmpTexture, textureMetrics.size);
            Memory::temporaryArena.FreeRegion(tmpRegion);
    
            
            uint32 textureBytes = textureMetrics.size.Area(),
                   nonPow2Bytes = minTextureHeight * textureMetrics.size.x;
            Log("\nRendered Font {\n"
                "\tface: %p, faceName: %s, targetGlyphSize: %d\n"
                "\tstartChar: '%c'[%d], endChar: '%c'[%d], unknownChar: '%c'[%d]\n"
                "\ttargetTextureSize: %u, textureWidth: %u, textureHeight: %u\n"
                "\ttextureBytes: %u [nonPow2: %u]\n"
                "\tpackingRatio: %f [nonPow2: %f]\n"
                "}",
            
                face, face->family_name, params.targetGlyphSize,
                startChar,startChar,  endChar,endChar, unknownChar, unknownChar,
                targetTextureSize, textureMetrics.size.x, textureMetrics.size.y,
                textureBytes, nonPow2Bytes,
                ((float)textureBytes/textureMetrics.minBytes), ((float)nonPow2Bytes/textureMetrics.minBytes)
               );
        }
        
        void Draw() {
    
            //Note: glMapBufferRange fails with 0 size
            if(!pushedBytes) return;
            
            glUseProgram(glProgram);
            glBindSampler(TUnitFont, fontSampler);
            glBindVertexArray(vao);
            
            glActiveTexture(GL_TEXTURE0 + TUnitFont);
            glBindTexture(GL_TEXTURE_2D, fontTexture);
    
            if(stringAttribBitIndex >= uploadStringAttribBitIndex) {

                // get inclusive distance from last uploaded stringAttribIndex to current stringAttribIndex
                uint32 nextUploadStringAttribBitIndex = stringAttribBitIndex + StringAttribBitIndexIncrement();
            
                uint32 uploadCount = StringAttribBitIndex(nextUploadStringAttribBitIndex - uploadStringAttribBitIndex);

                uint32 uscIndex = StringAttribBitIndex(uploadStringAttribBitIndex);
                uploadStringAttribBitIndex = nextUploadStringAttribBitIndex;

                //upload the new stringAttrib bits
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, stringAttribBuffer);
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, uscIndex*sizeof(StringAttrib), uploadCount * sizeof(StringAttrib), stringAttribData + uscIndex);
            }
            
            if(vertexAttributeBufferBytes < pushedBytes) BindAndAllocateVertexAttributeBufferBytes(pushedBytes);
            else                                         glBindBuffer(GL_ARRAY_BUFFER, vertexAttributeBuffer);
            
            // copy over the attributeData to glBuffer
            // TODO: make this similar to stringAttrib - only push new bytes!
            char* attributeBuffer = (char*)glMapBufferRange(GL_ARRAY_BUFFER, 0, pushedBytes, GL_MAP_WRITE_BIT);
            GlAssert(attributeBuffer, "Failed to map vertexAttributeBuffer");
            memoryArena.ForEachRegion(stringRegion, [&](void* chunk, uint32 chunkBytes) {
                memcpy(attributeBuffer, chunk, chunkBytes);
                attributeBuffer+= chunkBytes;
            });
            glUnmapBuffer(GL_ARRAY_BUFFER);
            
            //Note: 4 vertices per quad
            uint32 numChars = pushedBytes/sizeof(VertexAttributeData);
            glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, numChars);
        }
        
        inline void Clear() {
            
            //Note: without this check 'uploadStringAttribBitIndex' needs to be cleared to 0 and we'll reupload the default stringAttrib each time
            //      Clear is called - which might be every frame!
            RUNTIME_ASSERT(uploadStringAttribBitIndex,
                           "Clear called before Draw. Default stringAttrib not uploaded! { uploadStringAttribBitIndex: %u } ", uploadStringAttribBitIndex);
        
            //Note: index 0 is the default color and only needs to be uploaded once after 'RenderTexture' so we reset upload index to 1
            uploadStringAttribBitIndex = StringAttribBitIndexIncrement();
            stringAttribBitIndex = 0;
    
            pushedBytes = 0;
            memoryArena.FreeRegion(stringRegion);
        }
};