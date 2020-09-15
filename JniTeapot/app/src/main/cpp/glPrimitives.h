#pragma once

#include "GlContext.h"
#include "vec.h"

#include "types.h"

template<typename VertexType>
class GlUnitQuad {

	protected:
		static inline GLuint vBuffer;
		
		static void Init() {
			glGenBuffers(1, &vBuffer);
			GlAssertNoError("Failed to allocate vertex buffer");
			
			glBindBuffer(GL_ARRAY_BUFFER, vBuffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(kVerts), kVerts, GL_STATIC_DRAW);
			
			Log("Initialized GlUnitQuad");
		}
		
	public:
		static constexpr GLuint kGlDrawMode = GL_TRIANGLE_STRIP;
		static inline const VertexType kVerts[] = { (VertexType::up   + VertexType::left),
											        (VertexType::up   + VertexType::right),
                                                    (VertexType::down + VertexType::left),
										   	        (VertexType::down + VertexType::right)};
	
		static void Free() {
			RUNTIME_ASSERT(vBuffer, "GlUnitQuad not Initialized")
			
			glDeleteBuffers(1, &vBuffer);
            GlAssertNoError("Failed to delete buffers");
			
            vBuffer = 0;
            Log("Freed GlUnitQuad");
		}

		static inline GLuint VertexBuffer() {

			//TODO: see how costly this becomes in practice
			if(!vBuffer) Init();
			return vBuffer;
		}
};
