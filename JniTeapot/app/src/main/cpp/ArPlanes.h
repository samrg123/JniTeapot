#pragma once

#include "util.h"
#include "GlContext.h"

#include ARCORE_HEADER

class ArPlanes {
protected:

    static inline constexpr StringLiteral kShaderVersion = "310 es";

    struct GlIndirectData {
        uint vertexCount;
        uint instanceCount;
        uint firstVertex;
        uint reserved;        
    };

    enum Blocks  { BLOCK_VERTICIES };
    enum Uniform { UNIFORM_MVP_MATRIX };

    static inline constexpr StringLiteral kVertexShader = Shader(

        ShaderVersion(kShaderVersion)

        ShaderBufferBlock(BLOCK_VERTICIES) VerticiesBuffer {
            readonly float verticies[];
        };

        ShaderUniform(UNIFORM_MVP_MATRIX) mat4 mvpMatrix;

        void main() {
            vec2 vertex = vec2(verticies[gl_VertexID], verticies[gl_VertexID+1]);
            // vec3 normal = vec3(verticies[gl_InstanceID], verticies[gl_InstanceID+1], verticies[gl_InstanceID+2]);

            // TODO: use normal?
            // gl_Position = mvpMatrix * vec4(vertex, 0, 1.f);
            gl_Position = vec4(vertex, 1.f, 1.f);
        }
    );

    static inline constexpr StringLiteral kFragmentShader = Shader(

        ShaderVersion(kShaderVersion)

        precision highp float;
                
        ShaderOut(0) vec4 fragColor;

        void main() {
            fragColor = vec4(1.f, 0.f, 0.f, .5f);
        }
    ); 

    Memory::Arena vertexArena;
    Memory::Arena indirectAreana;

    const ArSession* arSession;

    ArPose* arPose;
    ArTrackableList* trackableList;

    uint32 vertexIndex = 0;
    uint32 indirectIndex  = 0;

    GLuint shaderProgram;
    GLuint vao;
    union {
        GLuint glBuffers[2];
        struct {
            GLuint glVertexBuffer;
            GLuint glIndirectBuffer;
        };
    };

    uint32 glVertexBufferBytes = 0;
    uint32 glIndirectBufferBytes = 0;

    uint32 glVertexBufferPosition = 0;
    uint32 glIndirectBufferPosition  = 0;


    void InsertPlane(const ArPlane* arPlane) {
        
        //get number of plane polygon indicies
        //Note: polygon is in format {[x1,z1], [x2, z2], ...} where polygonIndicies is
        //      the number of indicies NOT verticies
        //Note: Polygons have implicit z=0 coordinate and must be transformed with
        //      ArPlane_getCenterPos to be placed in world coordinates
        int32 polygonIndicies;
        ArPlane_getPolygonSize(arSession, arPlane, &polygonIndicies);

        //allocate verticies
        int32 vertexIndicies = 2 +                 //origin vector indicies
                               polygonIndicies +   //polygon indicies
                               3;                  //normal vector indicies

        int32 vertexBytes = sizeof(float)*vertexIndicies;
        void* verticies = vertexArena.PushBytes(vertexBytes);
        Vec2<float>* origin       = reinterpret_cast<Vec2<float>*>(verticies);
        Vec2<float>* polygon      = reinterpret_cast<Vec2<float>*>(ByteOffset(verticies, sizeof(Vec2<float>)));
        Vec3<float>* normalVector = reinterpret_cast<Vec3<float>*>(ByteOffset(verticies, vertexBytes-sizeof(Vec3<float>)));

        //fill origin verticies
        *origin = Vec2<float>(0, 0);

        //fill polygon verticies
        ArPlane_getPolygon(arSession, arPlane, polygon->component);
        
        //fill normal vector
        //TODO: TMP CODE TURN CETNER POSE INTO normal VECTOR
        ArPlane_getCenterPose(arSession, arPlane, arPose);
        *normalVector = Vec3<float>(0, 0, 1);

        //allocate new indirectData
        GlIndirectData* indirectData = indirectAreana.PushType<GlIndirectData>();
        indirectData->vertexCount   = polygonIndicies/2 + 1; //polygon verts + origin vert
        indirectData->instanceCount = 1;
        indirectData->firstVertex   = vertexIndex;
        indirectData->reserved = 0;

        //advance postion counters
        indirectIndex+=  1;
        vertexIndex+= vertexIndicies;
    }

    inline void UpdateBuffer(const Memory::Arena& arena, 
                             GLuint glBufferTarget, uint32 glBufferBytes, 
                             uint32 newGlBufferPosition, uint32 oldGlBufferPosition) {
        
        //check to see if we need to allocate more bytes
        if(glBufferBytes < newGlBufferPosition) {
            
            //allocate new buffer. Note: this deletes existing content
            glBufferData(glBufferTarget, newGlBufferPosition, nullptr, GL_DYNAMIC_DRAW);

            Log("Allocating new glBuffer: { glBufferTarget: %d, oldBufferBytes: %d, newBufferBytes: %d }",
                glBufferTarget, glBufferBytes, newGlBufferPosition);

            //Copy over whole arena
            void* glBufferData = glMapBufferRange(glBufferTarget, 0, newGlBufferPosition, GL_MAP_WRITE_BIT);
            arena.CopyToBuffer<uint8>(newGlBufferPosition, glBufferData);
            glUnmapBuffer(glBufferTarget);

            
        } else {
            
            //copy over only new parts of the arena
            GLuint newBytes = newGlBufferPosition-oldGlBufferPosition;
            void* glBufferData = glMapBufferRange(glBufferTarget, oldGlBufferPosition, newBytes, GL_MAP_WRITE_BIT);
            arena.CopyToBuffer<uint8>(newBytes, glBufferData);
            glUnmapBuffer(glBufferTarget);
        }
    }

    inline void BindBuffers() {

        //bind buffers
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER,  glIndirectBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BLOCK_VERTICIES, glVertexBuffer);

        //check if we have new planes
        uint32 vertexBytes = sizeof(float)*vertexIndex;
        if(glVertexBufferBytes < vertexBytes) {

            //update vertex buffer
            UpdateBuffer(vertexArena, 
                        GL_SHADER_STORAGE_BUFFER, glVertexBufferBytes, 
                        vertexBytes, glVertexBufferPosition);

            GlAssertNoError("Failed to update glVertexBuffer");

            //update indirect buffer
            uint32 indirectBytes = sizeof(GlIndirectData)*indirectIndex;
            UpdateBuffer(indirectAreana,
                         GL_DRAW_INDIRECT_BUFFER, glIndirectBufferBytes,
                         indirectBytes, glIndirectBufferPosition);

            GlAssertNoError("Failed to update glIndirectBuffer");

            //update buffer positions
            glVertexBufferPosition = vertexBytes;
            glIndirectBufferPosition = indirectBytes;
        }
    }

public:

    ArPlanes(const ArSession* arSession): arSession(arSession) {
        shaderProgram = GlContext::CreateGlProgram(kVertexShader, kFragmentShader);
    
        ArPose_create(arSession, nullptr, &arPose);
        ArTrackableList_create(arSession, &trackableList);

        glGenBuffers(ArrayCount(glBuffers), glBuffers);

        //Note: Indirect drawing requires VAO
        glGenVertexArrays(1, &vao);

        GlAssertNoError("Failed to initialize");    
    }

    ~ArPlanes() {

        ArPose_destroy(arPose);
        ArTrackableList_destroy(trackableList);

        glDeleteBuffers(ArrayCount(glBuffers), glBuffers);
        glDeleteVertexArrays(1, &vao);
    }

    void Draw() {

        if(!vertexIndex) return;

        glUseProgram(shaderProgram);
        
        BindBuffers();
        glBindVertexArray(vao);

        glDrawArraysIndirect(GL_TRIANGLE_FAN, 0);

        GlAssertNoError("Failed to draw");
    }

    void ClearPlanes() {

        //Note: we preserve gpu buffer size... might want to have a Purge function that clears that buffer as well  

        indirectIndex = 0;
        vertexIndex = 0;
        
        vertexArena.FreeAll();
        indirectAreana.FreeAll();
    }


    void UpdatePlanes() {

        //clear out old verticies
        //TODO: see if we can track the planes with an id? 
        ClearPlanes();

        //grab arplaces planes
        ArSession_getAllTrackables(arSession, AR_TRACKABLE_PLANE, trackableList);

        //TODO: only do this at init and then use ArFrame_getUpdatedTrackables
        //      to only update the planes that changed each frame
        //      maybe pass through a arPlane list to updatePlanes?
        int32 trackableListSize;
        ArTrackableList_getSize(arSession, trackableList, &trackableListSize);

        for(int32 i = 0; i < trackableListSize; ++i) {
            
            //grab arPlane
            ArTrackable* trackableItem = nullptr;
            ArTrackableList_acquireItem(arSession, trackableList, i, &trackableItem);
            ArPlane* arPlane = reinterpret_cast<ArPlane*>(trackableItem); 

            //check if this plane is merged with another plane in trackableList
            ArPlane* subsumedByPlane;
            ArPlane_acquireSubsumedBy(arSession, arPlane, &subsumedByPlane);
            if(subsumedByPlane) {

                //skip this plane, we'll just draw the subsumbed one
                ArTrackable_release(trackableItem);
                ArTrackable_release(reinterpret_cast<ArTrackable*>(subsumedByPlane));
                continue;
            }
 
            //TODO: consider allocating all planes in batch? could get time savings by allocating 1 large chunk on arenas
            InsertPlane(arPlane);
            ArTrackable_release(trackableItem);
        }
    }
};
