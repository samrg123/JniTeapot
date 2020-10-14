#pragma once

#include "GlContext.h"
#include "GlTransform.h"
#include "GlCamera.h"

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
                                                     "  mat4 mvMatrix;"
                                                     "};"
                                                     ""
                                                     "layout(location = 0) uniform float mirrorConstant;"
                                                     "layout(location = 1) uniform vec3 cameraPosition;"
                                                     "layout(location = 2) uniform vec3 lightPosition;"
                                                     ""
                                                     "layout(location = 0) in vec3 position;"
                                                     "layout(location = 1) in vec3 normal;"
                                                     "layout(location = 2) in vec2 uv;"
                                                     ""
                                                     "layout(location = 0) out vec3 lightDirection;"
                                                     "layout(location = 1) out vec3 fragNormal;"
                                                     "layout(location = 2) out vec3 cameraDirection;"
                                                     "layout(location = 3) out vec4 lightColor;"
                                                     "layout(location = 4) out vec3 worldPosition;"
                                                     "layout(location = 5) out vec3 fragLightPosition;"
                                                     ""
                                                     ""
                                                     "void main() {"
                                                     "  vec4 v4Position = vec4(position, 1.);"
                                                     "  gl_Position = mvpMatrix*v4Position;"
                                                     ""
                                                     "  fragNormal = normalize((mvMatrix * vec4(normal, 0.)).xyz);"
                                                     "  worldPosition = (mvMatrix * v4Position).xyz;"
                                                     "  fragLightPosition = lightPosition;"
                                                     ""
                                                     //"  lightColor = vec4(0., 1., 0., .3);"
                                                     //"  lightColor = vec4(1., 1., 1., 300000.);"
                                                     //"  lightColor = vec4(0.85, .95, 1., 200000.);"
                                                     "  lightColor = vec4(0.6784, .7255, .698, 1000000.);"
                                                     "  lightDirection = normalize(lightPosition - worldPosition);"
                                                     "  cameraDirection = normalize(cameraPosition - worldPosition);"
                                                     "}";
        
        static constexpr const char* kFragmentSource = "#version 310 es\n"
                                                       "precision highp float;"
                                                       ""
                                                       "layout(binding = 0) uniform samplerCube cubemapSampler;"
                                                       "layout(location = 0) uniform float mirrorConstant;"
                                                       ""
                                                       "layout(location = 0) in vec3 lightDirection;"
                                                       "layout(location = 1) in vec3 fragNormal;"
                                                       "layout(location = 2) in vec3 cameraDirection;"
                                                       "layout(location = 3) in vec4 lightColor;"
                                                       "layout(location = 4) in vec3 worldPosition;"
                                                       "layout(location = 5) in vec3 fragLightPosition;"
                                                       ""
                                                       "layout(location = 0) out vec4 fragColor;"
                                                       ""
                                                       "void main() {"
                                                       " "
                                                       "    vec4 diffuseColor = vec4(.9, .9, .9, 1.);"
                                                       "    vec4 ambientColor = vec4(.4471, .4486, .3464, 1.);"
                                                       ""
                                                       "    float reflectivity = mirrorConstant;"
                                                       "    float diffuseness = 1. - mirrorConstant;"
                                                       //"    float specularPower  = 16.;" //Note: smaller numbers = more reflective
                                                       "    float specularPower  = 8.;" //Note: smaller numbers = more reflective
                                                       "    "
                                                       ""
                                                       //TODO: this is phong - see if we should do blin-phong instead
                                                       //"    vec3 lightReflection = -reflect(lightDirection, fragNormal);"
                                                       "    vec3 lightReflection = normalize( ((2.*dot(fragNormal, lightDirection)) * fragNormal) - lightDirection);"
                                                       "    vec3 cubeReflection = (2.*fragNormal) - cameraDirection;"
                                                       "    vec4 cubeColor = texture(cubemapSampler, normalize(cubeReflection));"
                                                       ""
                                                       "    float lightDistance = fragLightPosition.z - worldPosition.z;"
                                                       "    float invLightDistanceSqaured = 1./(lightDistance*lightDistance);"
                                                       ""
                                                       "    vec3 ambientTerm = ambientColor.w * diffuseColor.rgb * ((diffuseness*ambientColor.rgb) + (reflectivity*cubeColor.rgb));"
                                                       //"    vec3 ambientTerm = ambientColor.w * diffuseColor.rgb * ((.999*ambientColor.rgb) + (.001*cubeColor.rgb));"
                                                       "    vec3 diffuseTerm =   diffuseness * diffuseColor.rgb * max(0., dot(fragNormal, lightDirection));"
                                                       "    float specularTerm = reflectivity * pow(max(0., dot(cameraDirection, lightReflection)), specularPower);"
                                                       "    vec3 lightTerm = lightColor.w * lightColor.rgb * invLightDistanceSqaured*(diffuseTerm + specularTerm);"
                                                       " "
                                                       "    fragColor.rgb = ambientTerm + lightTerm;"
                                                       "    fragColor.a = diffuseColor.a;"
                                                       ""
                                                       //"    fragColor.rgb = (fragColor.rgb*.001) + (.5*(lightDirection + vec3(1.)));"
                                                       ""
                                                       //Prevenets optimized out uniform error
                                                       //"    fragColor.rgb = fragColor.rgb + mirrorConstant*.01*cubeColor.rgb;"

                                                       ////NormalColor
                                                       //"    fragColor.rgb = (.001*fragColor.rgb) + .5*(fragNormal + vec3(1.));"
                                                       "}";
                
        enum Attribs  { ATTRIB_GEO_VERT, ATTRIB_NORMAL_VERT, ATTRIB_UV_VERT };
        enum Uniforms { UNIFORM_MIRROR_CONSTANT, UNIFORM_CAMERA_POSITION, UNIFORM_LIGHT_POSITION };
        enum UBlocks  { UBLOCK_UNIFORM_BLOCK };
        
        enum Flag {
            FLAG_NORMAL = 1<<0, FLAG_UV = 1<<1,
            FLAG_OBJ_TRANSFORM_UPDATED = 1<<2
        };
        
        struct alignas(16) UniformBlock {
            Mat4<float> mvpMatrix,
                        mvMatrix;
        };
        
        GlTransform transform;
        Mat4<float> transformMatrix;
        GlCamera* camera;
        uint32 cameraMatrixId;
        
        union {
            struct {
                GLuint vbo, elementBuffer, uniformBlockBuffer;
            };
            GLuint glBuffers[3];
        };
        
        GLuint vao;
        GLuint glProgram;
        GLuint cubeSampler, cubemapTexture;
        
        uint32 flags;
        uint32 numIndices;
        GLenum elementType;
        
        inline uint32 AllocateVBO(uint32 numVerts, uint32 vboStride) {
            uint32 vboBytes = numVerts*vboStride;
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            
            glBufferData(GL_ARRAY_BUFFER, vboBytes, nullptr, GL_STATIC_DRAW);
            GlAssertNoError("Failed to allocate vbo. { numVerts: %u, vboBytes: %u, vboStride: %u }",
                            numVerts, vboBytes, vboStride);
            
            return vboBytes;
        }

        inline uint32 AllocateElementsBuffer(uint32 numIndices, uint32 indexStride) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
            uint32 elementBufferBytes = numIndices*indexStride;

            glBufferData(GL_ELEMENT_ARRAY_BUFFER, elementBufferBytes, nullptr, GL_STATIC_DRAW);
            GlAssertNoError("Failed to allocate element buffer { numIndices: %u, elementBufferBytes: %u, indexStride: %u }",
                            numIndices, elementBufferBytes, indexStride);
            
            return elementBufferBytes;
        }
        
        template<uint32 kAttribute, typename ArenaT>
        static inline uint32 InterleaveVboWithArena(const Memory::Arena* arena, uint32 numElements, void* vboPtr, uint32 vboStride, uint32 vboOffset) {
            
            glEnableVertexAttribArray(kAttribute);
            glVertexAttribPointer(kAttribute, GlAttributeSize<ArenaT>(),
                                  GlAttributeType<ArenaT>(), GL_FALSE, vboStride, (void*)vboOffset);
    
            arena->CopyToBuffer<ArenaT>(numElements, ByteOffset(vboPtr, vboOffset), sizeof(ArenaT), vboStride);
            return vboOffset + sizeof(Vec3<float>);
        }
        
        struct UploadBufferParams {
            
            Memory::Arena geoVertArena, normalVertArena, uvVertArena;
            
            Memory::Arena* indicesArena;
            Memory::Region indicesStartRegion, indicesStopRegion;
            
            uint32 numIndices,
                   numGeoVerts, numNormalVerts, numUvVerts;
        };
        
        template<typename ElementT>
        void UploadBuffers(const UploadBufferParams* params) {
    
            numIndices = params->numIndices;
            elementType = GlAttributeType<ElementT>();
            
            uint32 numVerts = params->numGeoVerts;
            RUNTIME_ASSERT(!params->numNormalVerts || params->numNormalVerts == numVerts,
                           "numNormalVerts[%u] != numVerts[%u]", params->numNormalVerts, numVerts);
            
            RUNTIME_ASSERT(!params->numUvVerts || params->numUvVerts == numVerts,
                           "numUvVerts[%u] != numVerts[%u]", params->numUvVerts, numVerts);
            
            // compute vbo stride
            // Note: vbo's always contain geoVerts & normals (we compute them if not provided). UV is optional
            uint32 vboStride = 2*sizeof(Vec3<float>);
            if(flags&FLAG_UV) vboStride+= sizeof(Vec2<float>);
            
            // allocate buffers
            glBindVertexArray(vao);
            uint32 vboBytes = AllocateVBO(numVerts, vboStride);
            uint32 elementBufferBytes = AllocateElementsBuffer(params->numIndices, sizeof(ElementT));
            
            void* vboPtr = glMapBufferRange(GL_ARRAY_BUFFER, 0, vboBytes, GL_MAP_WRITE_BIT);
            GlAssert(vboPtr, "Failed to map vbo buffer");
    
            void* elementPtr = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, elementBufferBytes, GL_MAP_WRITE_BIT) ;
            GlAssert(elementPtr, "Failed to map element buffer");
    
            
            //upload geoVerts
            uint32 vboOffset = InterleaveVboWithArena<ATTRIB_GEO_VERT, Vec3<float>>(&params->geoVertArena, numVerts, vboPtr, vboStride, 0);
            
            if(flags&FLAG_NORMAL) {
    
                //upload normalVerts - TODO: TEST THIS WITH FILE
                vboOffset = InterleaveVboWithArena<ATTRIB_NORMAL_VERT, Vec3<float>>(&params->normalVertArena, numVerts, vboPtr, vboStride, vboOffset);
    
                //Translate indices to type T and copy to GL_ELEMENT_ARRAY_BUFFER
                Memory::TranslateRegionsToBuffer(params->indicesStartRegion, params->indicesStopRegion,
                                                 params->numIndices, elementPtr,
                                                 sizeof(int), sizeof(ElementT),
                                                 [](void* regionElement, void* bufferElement) {
                                                    *(ElementT*)bufferElement = ElementT(*(int*)regionElement);
                                                 });
                
            } else  {
                
                //TODO: clean this up and break out to function
                
                Log("Normals not provided in obj file. Computing normals.");
                
                //create a tempory block of vectors filled with 0's to compute intermediate math on
                Memory::Region tmpRegion = Memory::temporaryArena.CreateRegion();
                Vec3<float>* tmpNormals = (Vec3<float>*)Memory::temporaryArena.PushBytes(numVerts*sizeof(Vec3<float>), true);
                
                auto computeNormalsAndTranslateIndices = [&](void* regionElements, void* bufferElements) {
                    int* indices = (int*)regionElements;
                    int index1 = indices[0],
                        index2 = indices[1],
                        index3 = indices[2];
    
                    //get corresponding vertices from vbo
                    Vec3<float> *v1 = (Vec3<float>*)ByteOffset(vboPtr, vboStride*index1),
                                *v2 = (Vec3<float>*)ByteOffset(vboPtr, vboStride*index2),
                                *v3 = (Vec3<float>*)ByteOffset(vboPtr, vboStride*index3);
    
                    //compute the add normal vector
                    Vec3<float> crossProduct = (*v2 - *v1).Cross(*v3 - *v1).Normalize();
                    tmpNormals[index1]+= crossProduct;
                    tmpNormals[index2]+= crossProduct;
                    tmpNormals[index3]+= crossProduct;
    
                    //translate indices to T
                    ElementT* translatedIndices = (ElementT*)bufferElements;
                    translatedIndices[0] = (ElementT)index1;
                    translatedIndices[1] = (ElementT)index2;
                    translatedIndices[2] = (ElementT)index3;
                };
                
                //compute smooth normals while translating indices to type T and copying to GL_ELEMENT_ARRAY_BUFFER
                Memory::TranslateRegionsToBuffer(params->indicesStartRegion, params->indicesStopRegion,
                                                 (params->numIndices)/3, elementPtr,
                                                 3*sizeof(int), 3*sizeof(ElementT),
                                                 computeNormalsAndTranslateIndices);

                //average normals and interleave in vbo
                glEnableVertexAttribArray(ATTRIB_NORMAL_VERT);
                glVertexAttribPointer(ATTRIB_NORMAL_VERT, 3, GL_FLOAT, GL_FALSE, vboStride, (void*)vboOffset);
                
                Memory::TranslateRegionsToBuffer(tmpRegion, Memory::temporaryArena.CreateRegion(),
                                                 numVerts, ByteOffset(vboPtr, vboOffset),
                                                 sizeof(Vec3<float>), vboStride,
                                                 [&](void* regionElement, void* bufferElement){ *(Vec3<float>*)bufferElement = ((Vec3<float>*)regionElement)->Normalize(); });
    
                Memory::temporaryArena.FreeBaseRegion(tmpRegion);
                vboOffset+= sizeof(Vec3<float>);
            }
            
            //upload uvVerts - TODO: TEST THIS WITH FILE
            if(flags&FLAG_UV) {
                vboOffset = InterleaveVboWithArena<ATTRIB_UV_VERT, Vec2<float>>(&params->uvVertArena, numVerts, vboPtr, vboStride, vboOffset);
            }
            
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            
            glBindVertexArray(0);
        }
        
        char* LoadVertex(char* strPtr, UploadBufferParams* params) {
            char mode = *++strPtr;
            switch(mode) {
        
                //geometry vertex
                case ' ':
                case '\t': {
                    ++params->numGeoVerts;
            
                    Vec3<float>* v = params->geoVertArena.PushStruct<Vec3<float>>();
                    v->x = StrToFloat(++strPtr, &strPtr);
                    v->y = StrToFloat(++strPtr, &strPtr);
                    v->z = StrToFloat(++strPtr, &strPtr);
            
                } break;
            
                //texture vertex
                case 't': {
                    ++params->numUvVerts;
            
                    Vec2<float>* v = params->uvVertArena.PushStruct<Vec2<float>>();
                    v->x = StrToFloat(++strPtr, &strPtr);
                    v->y = StrToFloat(++strPtr, &strPtr);
            
                } break;

                //normal vertex
                case 'n': {
                    ++params->numNormalVerts;
            
                    Vec3<float>* v = params->normalVertArena.PushStruct<Vec3<float>>();
                    v->x = StrToFloat(++strPtr, &strPtr);
                    v->y = StrToFloat(++strPtr, &strPtr);
                    v->z = StrToFloat(++strPtr, &strPtr);
            
                } break;
        
                default: {
                    Panic("Unsupported Vertex mode: %d[%c]", mode, mode);
                }
            }
            return strPtr;
        }
        
        char* LoadFace(char* strPtr, UploadBufferParams* params) {
            
            params->numIndices+= 3;
    
            int32 numGeoVerts = params->numGeoVerts;
            RUNTIME_ASSERT(numGeoVerts > 0, "Overflowed maximum allowed number of vertices [%d]", MaxInt32());
            
            int* vertIndices = (int*)params->indicesArena->PushBytes(3*sizeof(int));
            for(int i = 0; i < 3; ++i) {
        
                //get vertIndex
                int geoIndex = StrToInt(++strPtr, &strPtr);
                vertIndices[i] = (geoIndex <= 0) ? geoIndex + numGeoVerts : geoIndex - 1;
                RUNTIME_ASSERT((uint32)vertIndices[i] < numGeoVerts, "Geometry Vertex not defined { geoIndex: %d, numGeoVerts: %d }", geoIndex, numGeoVerts);
                
                //get uvIndex
                if(*strPtr == '/') {
                    flags|= FLAG_UV;
            
                    int uvIndex = StrToInt(++strPtr, &strPtr);
                    RUNTIME_ASSERT(uvIndex == geoIndex, "uvIndex [%d] != geoIndex [%d]. Shuffled Indices not supported!", uvIndex, geoIndex);
            
                    //get normalIndex
                    if(*strPtr == '/') {
                        flags|= FLAG_NORMAL;
                
                        int nIndex = StrToInt(++strPtr, &strPtr);
                        RUNTIME_ASSERT(nIndex == geoIndex, "uvIndex [%d] != geoIndex [%d]. Shuffled Indices not supported!", uvIndex, geoIndex);
                    }
                }
            }
            
            return strPtr;
        }
        
        void LoadObject(const FileManager::AssetBuffer* buffer) {
    
            UploadBufferParams uploadParams = {
                .indicesArena = &Memory::temporaryArena,
                .indicesStartRegion = Memory::temporaryArena.CreateRegion()
            };
        
            char* ptr = (char*)buffer->data;
            for(;;) {
                ptr = SkipWhiteSpace(ptr);
                
                char c = *ptr;
                switch(c) {

                    //eof - Finish processing and return
                    case 0: {
                        
                        uploadParams.indicesStopRegion = Memory::temporaryArena.CreateRegion();
                        
                             if(!LargerThan8Bit(uploadParams.numGeoVerts))  UploadBuffers<uint8>(&uploadParams);
                        else if(!LargerThan16Bit(uploadParams.numGeoVerts)) UploadBuffers<uint16>(&uploadParams);
                        else UploadBuffers<uint32>(&uploadParams);
    
                        uploadParams.indicesArena->FreeBaseRegion(uploadParams.indicesStartRegion);
                        return;
                    }
                    
                    //ignore comments
                    case '#': break;
    
                    //handle vertices
                    case 'v': ptr = LoadVertex(ptr, &uploadParams);
                    break;
        
                    
                    //Note: faces - defined by indices in the form 'v/vt/vn' where vt and vn are optional
                    case 'f': ptr = LoadFace(ptr, &uploadParams);
                    break;
                    
                    default: {
                        Panic("Unknown object command: %d[%c]", c, c);
                    }
                }

                //advance to next line
                ptr = SkipLine(ptr);
            }
        }

    public:

        inline void SetCamera(GlCamera* c) {
            RUNTIME_ASSERT(c, "Camera cannot be null");
            camera = c;

            //set the matrixId to an stale one so mvp matrix gets updated next draw
            cameraMatrixId = c->MatrixId()-1;
        }
        
        GlObject(const char* objPath, GlCamera* camera, const GlTransform& transform = GlTransform()):
                    transform(transform),
                    camera(camera),
                    flags(FLAG_OBJ_TRANSFORM_UPDATED) {
    
            SetCamera(camera);
            
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
    
            Memory::temporaryArena.FreeBaseRegion(tmpRegion);
            
            //TODO: MOVE THIS OUT!!
            {
                const char* images[] = {
                    "textures/skymap/px.png",
                    "textures/skymap/nx.png",
                    "textures/skymap/py.png",
                    "textures/skymap/ny.png",
                    "textures/skymap/pz.png",
                    "textures/skymap/nz.png"
                };
                
                cubemapTexture = GlContext::LoadCubemap(images);
                Log("Cubemap Texture: %d", cubemapTexture);
    
                glGenSamplers(1, &cubeSampler);
                glSamplerParameteri(cubeSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glSamplerParameteri(cubeSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glSamplerParameteri(cubeSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glSamplerParameteri(cubeSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                GlAssertNoError("Failed to create cubemap sampler: %d", cubeSampler);
            }
            
            
        }
        
        ~GlObject() {
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(ArrayCount(glBuffers), glBuffers);
            glDeleteProgram(glProgram);
        }
        
        inline GlTransform GetTransform() const { return transform; }
        inline void SetTransform(const GlTransform& t) { transform = t; flags|= FLAG_OBJ_TRANSFORM_UPDATED; }
        
        void Draw(float mirrorConstant) {
        
            //check if camera matrix updated
            // Warn: cameraMatrixId repeats every 2^32 iterations so its possible that this check fails if the cameraMatrixId differs by a multiple of 2^32.
            //       This has a failure probability of 4.6566129e-10, way smaller than the chance of the earth being destroyed massive meteor (1e-8)
            //       Failure mode is using the previous projection matrix until the next update to camera or object matrix (most likely will occur on the next frame)
            //       If this failure rate/mode is still concerning use a uint64 for the camera matrixId instead
            bool updateUniformBlock =cameraMatrixId != camera->MatrixId();
            
            //check if the object matrix updated
            if(flags&FLAG_OBJ_TRANSFORM_UPDATED) {
                flags^= FLAG_OBJ_TRANSFORM_UPDATED;
                transformMatrix = transform.Matrix();
                updateUniformBlock = true;
            }
    
            //update mvpMatrix
            if(updateUniformBlock) {
                glBindBuffer(GL_UNIFORM_BUFFER, uniformBlockBuffer);
                UniformBlock* uniformBlock = (UniformBlock*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, sizeof(UniformBlock), GL_MAP_WRITE_BIT);
                GlAssert(uniformBlock, "Failed to map uniformBlock");
    
                uniformBlock->mvpMatrix = camera->Matrix() * transformMatrix;
                uniformBlock->mvMatrix = transformMatrix;

                glUnmapBuffer(GL_UNIFORM_BUFFER);
            }
            
            //TODO: This
            //      Bind sampler
            //      Bind texture
            
            glUseProgram(glProgram);
            
            
            //TODO: DEBUG CODE
            {
                glUniform1f(UNIFORM_MIRROR_CONSTANT, mirrorConstant);
    
                Vec3<float> cameraPosition = camera->GetTransform().position;
                glUniform3fv(UNIFORM_CAMERA_POSITION, 1, cameraPosition.component);
    
                Vec3<float> lightPosition = Vec3(1500.f, 1000.f, -1500.f);
                glUniform3fv(UNIFORM_LIGHT_POSITION, 1, lightPosition.component);
            }
            
            //Note: no need to bind 'GL_ELEMENT_ARRAY_BUFFER', its part of vao state
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
            //TODO: make TuUniform instead of passing 0
            glBindSampler(0, cubeSampler);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);

            glDrawElements(GL_TRIANGLES, numIndices, elementType, 0);

            GlAssertNoError("Failed to Draw");
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