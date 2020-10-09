#pragma once

#include "GlContext.h"
#include "GlTransform.h"
#include "FileManager.h"
#include "Memory.h"
#include "panic.h"

#include "util.h"
#include "vec.h"
#include "mat.h"

class GlObject {
    private:
        
        static constexpr const char* kVertexSource = "#version 310 es\n"
                                                     ""
                                                     //Note: blocks must be defined in the order of their binding (gles 3.1 doesn't let use explicitly set location=binding)
                                                     "layout(std140, binding = 0) uniform UniformBlock {"
                                                     "  mat4 mvpMatrix;"
                                                     //"  vec3 camera;"
                                                     "};"
                                                     ""
                                                     "layout(location = 0) in vec3 position;"
                                                     "layout(location = 1) in vec3 normal;"
                                                     "layout(location = 2) in vec2 uv;"
                                                     ""
                                                     //"smooth out uvOut;"
                                                     //"smooth out reflectOut;"
                                                     ""
                                                     "void main() {"
                                                     "  vec4 v4Position = vec4(position.x, position.y, position.z, 1.);"
                                                     "  gl_Position = mvpMatrix*v4Position;"
                                                     //"  gl_Position.xyz = position;"
                                                     ""
                                                     "  gl_Position.w = 1.;"
                                                     "}";
        
        static constexpr const char* kFragmentSource = "#version 310 es\n"
                                                       "precision mediump float;"
                                                       ""
                                                       "layout(location = 0) out vec4 fragColor;"
                                                       ""
                                                       "void main() {"
                                                       "    fragColor = vec4(0., 1., 0., 1.);"
                                                       "}";
                
        enum Attribs { ATTRIB_GEO_VERT, ATTRIB_NORMAL_VERT, ATTRIB_UV_VERT };
        enum UBlocks { UBLOCK_UNIFORM_BLOCK };
        
        enum Flag {
            FLAG_NONE, FLAG_NORMAL = 1<<0, FLAG_UV = 1<<1,
            FLAG_TRANSFORM_UPDATED = 1<<2
        };
        
        struct alignas(16) UniformBlock {
            Mat4<float> mvpMatrix;
        };
        
        GlTransform transform;
        
        union {
            struct {
                GLuint vbo, elementBuffer, uniformBlockBuffer;
            };
            GLuint glBuffers[3];
        };
        
        GLuint vao;
        GLuint glProgram;
        
        uint32 flags;
        uint32 numElements;
        GLenum elementType;
        
        //Note: indices are Vec3<int> where x = vertexIndex, y = normalIndex, z = uvIndex
        template<typename T>
        inline void UploadElements(uint32 numIndices, const Memory::Arena& indicesArena, const Memory::Region& indicesRegion) {
            
            //set the elementType
            elementType = GlType<T>();
    
            //Allocate element Buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
            uint32 elementBufferBytes = numIndices*sizeof(T);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementBufferBytes, nullptr, GL_STATIC_DRAW);
            GlAssertNoError("Failed to allocate element buffer { numIndices: %u, elementBufferBytes: %u }", numIndices, elementBufferBytes);
    
            void* bufferPtr = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, elementBufferBytes, GL_MAP_WRITE_BIT) ;
            GlAssert(bufferPtr, "Failed to map elementBuffer { numIndices: %u, elementBufferBytes: %u }", numIndices, elementBufferBytes);
    
            //Translate indices to type T and copy to element Buffer
            indicesArena.TranslateRegionToBuffer(indicesRegion, numIndices, bufferPtr, sizeof(int), [&](const void* element){
                return (T)(*(int*)element);
            });

            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        }
        
        void LoadObject(const FileManager::AssetBuffer* buffer) {
    
            Memory::Arena &tmpIndicesArena = Memory::temporaryArena,
                          tmpGeoVertArena,
                          tmpNormalArena,
                          tmpUvArena;
            
            Memory::Region tmpIndicesRegion = tmpIndicesArena.CreateRegion();

            numElements = 0;
            uint32 numGeoVerts = 0,
                   numUvVerts = 0,
                   numNormalVerts = 0;
    
            char* ptr = (char*)buffer->data;
            for(;;) {
                ptr = SkipWhiteSpace(ptr);
                
                char c = *ptr;
                switch(c) {

                    //eof - Finish processing and return
                    case 0: {

                        //TODO: turn triangles into triangle strips to reduce memory overhead of draw call
                        //TODO: compute normals if they aren't available
    
                        bool hasNormal  = flags&FLAG_NORMAL,
                             hasTexture = flags&FLAG_UV;
                        
                        //TODO: add support for non-interlaced buffer by packing buffers back to back - EX:  vbo = {allGeo, allNormal, allTexture}
                        //      Note: figuring out how we should pack vs interlace is a little tricky. Ex vbo = {allGeo, inerlacedNormalTexture} might be more efficient!
                        RUNTIME_ASSERT(!hasNormal  || numGeoVerts == numNormalVerts, "numGeoVerts[%u] != numNormalVerts[%u]. Non-interlaced buffers not supported yet!", numGeoVerts, numNormalVerts);
                        RUNTIME_ASSERT(!hasTexture || numGeoVerts == numUvVerts, "numGeoVerts[%u] != numUvVerts[%u]. Non-interlaced buffers not supported yet!", numGeoVerts, numUvVerts);
                        
                        uint32 numVerts = numGeoVerts;
    
                        //Bind VAO - Note: UploadElemnets requires Vao to be bound so it can bind GL_ELEMENT_ARRAY_BUFFER (which is part of vao)
                        glBindVertexArray(vao);
    
                        //Upload elementBuffer
                        if(!LargerThan8Bit(numVerts))       UploadElements<uint8>(numElements, tmpIndicesArena, tmpIndicesRegion);
                        else if(!LargerThan16Bit(numVerts)) UploadElements<uint16>(numElements, tmpIndicesArena, tmpIndicesRegion);
                        else                                UploadElements<uint32>(numElements, tmpIndicesArena, tmpIndicesRegion);
                        
                        //compute vbo stride
                        uint32 vboStride = sizeof(Vec3<float>);
                        if(hasNormal)  vboStride+= sizeof(Vec3<float>);
                        if(hasTexture) vboStride+= sizeof(Vec2<float>);
    
                        uint32 vboBytes = numVerts*vboStride;
    
                        //Allocate VBO
                        glBindBuffer(GL_ARRAY_BUFFER, vbo);
                        glBufferData(GL_ARRAY_BUFFER, vboBytes, nullptr, GL_STATIC_DRAW);
                        GlAssertNoError("Failed to allocate vbo. { numVerts: %u, vboBytes: %u, hasNormal: %d, hasTexture: %d }", numVerts, vboBytes, hasNormal, hasTexture);
                        
                        //interleave vbo Data
                        {
                            uint32 vboOffset = 0;
                            
                            void* vboPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, vboBytes, GL_MAP_WRITE_BIT);
                            GlAssert(vboPtr, "Failed to map vbo { numVerts: %u, vboBytes: %u, hasNormal: %d, hasTexture: %d }", numVerts, vboBytes, hasNormal, hasTexture);
                            
                            glEnableVertexAttribArray(ATTRIB_GEO_VERT);
                            glVertexAttribPointer(ATTRIB_GEO_VERT, 3, GL_FLOAT, GL_FALSE, vboStride, (void*)vboOffset);
                            
                            //interleave geoVerts
                            tmpGeoVertArena.CopyToBuffer<Vec3<float>>(numGeoVerts, ByteOffset(vboPtr, vboOffset), vboStride);
                            vboOffset+= sizeof(Vec3<float>);
    
                            //interleave normalVerts and align vboPtr to start of nextElement
                            if(hasNormal) {
                                glEnableVertexAttribArray(ATTRIB_NORMAL_VERT);
                                glVertexAttribPointer(ATTRIB_NORMAL_VERT, 3, GL_FLOAT, GL_FALSE, vboStride, (void*)vboOffset);
    
                                tmpNormalArena.CopyToBuffer<Vec3<float>>(numNormalVerts, ByteOffset(vboPtr, vboOffset), vboStride);
                                vboOffset+= sizeof(Vec3<float>);
                            }
    
                            //interleave uvVerts
                            if(hasTexture) {
                                glEnableVertexAttribArray(ATTRIB_UV_VERT);
                                glVertexAttribPointer(ATTRIB_UV_VERT, 2, GL_FLOAT, GL_FALSE, vboStride, (void*)vboOffset);
                                
                                tmpUvArena.CopyToBuffer<Vec2<float>>(numUvVerts, ByteOffset(vboPtr, vboOffset), vboStride);
                                vboOffset+= sizeof(Vec2<float>);
                            }

                            glUnmapBuffer(GL_ARRAY_BUFFER);
                        }
    
                        glBindVertexArray(0);
                        tmpIndicesArena.FreeRegion(tmpIndicesRegion);
                        return;
                    }
                    
                    //ignore comments
                    case '#': break;
    
                    //handle vertices
                    case 'v': {

                        char mode = *++ptr;
                        switch(mode) {
                            
                            //geometry vertex
                            case ' ':
                            case '\t': {
                                ++numGeoVerts;
                                
                                Vec3<float>* v = tmpGeoVertArena.PushStruct<Vec3<float>>();
                                v->x = StrToFloat(++ptr, &ptr);
                                v->y = StrToFloat(++ptr, &ptr);
                                v->z = StrToFloat(++ptr, &ptr);

                            } break;
                            
                            
                            //texture vertex
                            case 't': {
                                ++numUvVerts;
                                
                                Vec2<float>* v = tmpUvArena.PushStruct<Vec2<float>>();
                                v->x = StrToFloat(++ptr, &ptr);
                                v->y = StrToFloat(++ptr, &ptr);

                            } break;
                            
                            //normal vertex
                            case 'n': {
                                ++numNormalVerts;
    
                                Vec3<float>* v = tmpNormalArena.PushStruct<Vec3<float>>();
                                v->x = StrToFloat(++ptr, &ptr);
                                v->y = StrToFloat(++ptr, &ptr);
                                v->z = StrToFloat(++ptr, &ptr);
                                
                            } break;
                            
                            default: {
                                Panic("Unsupported Vertex mode: %d[%c]", mode, mode);
                            }
                        }
                    } break;
        
                    //Note: faces - defined by indices in the form 'v/vt/vn' where vt and vn are optional
                    case 'f': {
                        numElements+= 3;
                        
                        //TODO: VertIndices has a lot of waste (only .33 space effiecient if no normal or uv)
                        //      throw away the Normal and UV index and map everything to the same geo index after inforcing they are
                        //      the same
                        int* vertIndices = (int*)tmpIndicesArena.PushBytes(3*sizeof(int));
                        
                        for(int i = 0; i < 3; ++i) {
    
                            //get vertIndex
                            int geoIndex = StrToInt(++ptr, &ptr);
                            RUNTIME_ASSERT(geoIndex && geoIndex <= numGeoVerts, "Geometry Vertex not defined { geoIndex: %d, numGeoVerts: %d }", geoIndex, numGeoVerts);
                            vertIndices[i] = (geoIndex < 0) ? geoIndex + numGeoVerts : geoIndex - 1;
    
                            //get uvIndex
                            if(*ptr == '/') {
                                flags|= FLAG_UV;
                            
                                int uvIndex = StrToInt(++ptr, &ptr);
                                RUNTIME_ASSERT(uvIndex == geoIndex, "uvIndex [%d] != geoIndex [%d]. Shuffled Indices not supported yet!", uvIndex, geoIndex);
                            
                                //get normalIndex
                                if(*ptr == '/') {
                                    flags|= FLAG_NORMAL;
                                    
                                    int nIndex = StrToInt(++ptr, &ptr);
                                    RUNTIME_ASSERT(nIndex == geoIndex, "uvIndex [%d] != geoIndex [%d]. Shuffled Indices not supported yet!", uvIndex, geoIndex);
                                }
                            }
                        }
                    } break;
                    
                    default: {
                        Panic("Unknown object command: %d[%c]", c, c);
                    }
                }

                //advance to next line
                ptr = SkipLine(ptr);
            }
        }

    public:

        GlObject(const char* objPath, GlTransform transform = GlTransform()): transform(transform), flags(FLAG_TRANSFORM_UPDATED) {
    
            glGenVertexArrays(1, &vao);
            GlAssertNoError("Failed to create vao");
            
            glGenBuffers(ArrayCount(glBuffers), glBuffers);
            GlAssertNoError("Failed to create glBuffers");

            glProgram = GlContext::CreateGlProgram(kVertexSource, kFragmentSource);
    
            //Debugging
            GlContext::PrintVariables(glProgram);
            
            //bind uniform block to program
            glBindBuffer(GL_UNIFORM_BUFFER, uniformBlockBuffer);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformBlock), nullptr, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_UNIFORM_BLOCK, uniformBlockBuffer);
            GlAssertNoError("Failed to bind UniformBlock [%d] to uniformBlockBuffer [%d]", UBLOCK_UNIFORM_BLOCK, uniformBlockBuffer);
            
            // load obj
            Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
            FileManager::AssetBuffer* buffer = FileManager::OpenAsset(objPath, &Memory::temporaryArena);
            LoadObject(buffer);
            
            Memory::temporaryArena.FreeRegion(tmpRegion);
        }
        
        ~GlObject() {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(ArrayCount(glBuffers), glBuffers);
            glDeleteProgram(glProgram);
        }
        
        inline GlTransform GetTransform() const { return transform; }
        inline void SetTransform(const GlTransform& t) { transform = t; flags|= FLAG_TRANSFORM_UPDATED; }
        
        //TODO: pass in GlCamera transform
        void Draw() {

            if(flags&FLAG_TRANSFORM_UPDATED) {
                flags^= FLAG_TRANSFORM_UPDATED;
             
                glBindBuffer(GL_UNIFORM_BUFFER, uniformBlockBuffer);
                Mat4<float>* mvpMatrix = (Mat4<float>*)glMapBufferRange(GL_UNIFORM_BUFFER, offsetof(UniformBlock, mvpMatrix), sizeof(Mat4<float>), GL_MAP_WRITE_BIT);
                GlAssert(mvpMatrix, "Failed to map mvpMatrix");
    
                //TODO: replace Identity with glCamera.ViewProjectionMatrix();
                *mvpMatrix = transform.Matrix() * Mat4<float>::Identity;
                
                glUnmapBuffer(GL_UNIFORM_BUFFER);
            }
            
            //TODO: This
            //      Bind sampler
            //      Bind texture
            
            glUseProgram(glProgram);
    
            //Note: no need to bind 'GL_ELEMENT_ARRAY_BUFFER', its part of vao state
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);

            glDrawElements(GL_TRIANGLES, numElements, elementType, 0);
        }
        
        //TODO: create GENERIC!!! VBO
        //TODO: create VBI's
        //TODO: create FBO pipeline: vbo queue -> render with their included shaders & optional VBIs to backBuffer quad texture
        //      - VBOs render pipeline takes in a camera to render to!
        //      - Camera system for handling MVP matrix transforms
        
        //TODO: create FBO
        //TODO: create COMPOSITE pipeline: fbo queue -> render with their included shaders (or none) -> stencil regions & z-ordering
        
        //TODO: have text render unit quad vertices for each character
        //      - write shader that positions text on
        //      - GlText is a type of renderable VBO - doesn't have world matrix though (default camera?)
        //      - GlText gets rendered to a hub FBO - can still add post processing like shake, and distortion!

};