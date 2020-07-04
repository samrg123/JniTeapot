package com.eecs487.lab0;

import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.util.Log;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

//Note: use: https://developer.android.com/reference/android/opengl/GLES20
//      and: https://www.khronos.org/registry/OpenGL-Refpages/es2.0/
//      and: https://www.khronos.org/opengles/sdk/docs/reference_cards/OpenGL-ES-2_0-Reference-card.pdf
//      as openGLES references
//      learn more about openGLES2.0 specification at: https://www.khronos.org/registry/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf

public class Lab0Renderer implements GLSurfaceView.Renderer {

    // TASK 1: set the global variable "glRenderMode" to the openGL drawing mode to draw lines
    // Note: these are marked volatile(disables caching) because they are set by MainAcivity which runs on a separate thread.
    //       they don't need to be synchronized because they are only read from in the renderThread
    public volatile int glRenderMode = GLES20.GL_LINES;
    public volatile int glLineWidth = 1; // width of line in pixels - Note: opengles defaults to 1
    public volatile boolean driftEnabled;
    public volatile boolean drawWireframe;

    // Handle for render program executed on the GPU
    //Note: OpenGL uses an int handles to reference different objects
    private int glProgram;

    // Code for vertex shader - this gets run on the GPU per vertex and tells the GPU how to render each vertices in clipSpace
    // Note: opengl translates the vertex shader output vertex 'gl_Position' into normalized device coordinates 'NDC by first clipping vertices
    //       that lie out side of the range [-gl_Position.w, gl_Position.w] and then dividing each component of gl_Position by gl_Position.w (The division accounts for perspective).
    //       Finally opengl rasterizes each vertex into pixel fragments these fragments are then passed to the fragment shader
    private final String kGlVertextShaderStr = "uniform mat4 mat4Projection;" +  //uniform means the value is shared across all vertices of primitive
                                               "attribute vec4 v4Position;" +    //attribute means the value is set per vertex of primitive
                                               "attribute vec3 v3PrimitiveVertex;" + //used to draw wireframe around triangles - set to non-negative constant to disable wireframe
                                               "varying vec3 v3PrimitivePosition;" + //varying means the variable is output from vertex shader and input to the fragment shader
                                               "void main() {" +
                                               "    gl_Position = mat4Projection*v4Position;" + //gl_Position is the opengl clipSpaceCoordinate of vertex - Note: opengl may not render GL_POINT if any part of the point is outside of clipSpace
                                               "    gl_PointSize = 20.;" +                      //gl_Point size is the size of GL_POINT vertices in pixels
                                                "   v3PrimitivePosition = v3PrimitiveVertex;" +
                                               "}";
    private int glUniformMat4Projection,
                glAttributeV4Position,
                glAttributeV3PrimitiveVertex;

    // Code for fragment(pixel) shader - this gets run on the GPU per pixel and tells the GPU how to color each pixel of a primitive
    private final String kGlFragmentShaderStr = "#extension GL_OES_standard_derivatives : enable\n" +   //tell opengl to include extensions for dFdx and dFdy - Note: this must be specified before any shader code
                                                "precision mediump float;" + //sets the floating point math precision to medium - Note: This is required for fragment shader (defaults to highp in vertex shader)
                                                "uniform vec4 v4Color;" +
                                                "varying vec3 v3PrimitivePosition;" +
                                                "void main() {" +
                                                "   gl_FragColor = v4Color;" + //gl_FragColor is the opengl color of the primitive pixel fragment

                                                //get the window stepsize of v3PrimitivePosition in pixels
                                                "   vec3 dx = abs(dFdx(v3PrimitivePosition));" +
                                                "   vec3 dy = abs(dFdy(v3PrimitivePosition));" +

                                                // compute the v3PrimitivePosition that is withing 3 pixels of the edge -
                                                // Note: this is exact form, but sqrt becomes expensive with a large number of vertices.
                                                //       Value can be approximated with: 'width*(abs(dx) + abs(dy))' = width*fwidth(v3PrimitivePosition)'
                                                "   vec3 edgeWidth = 3.*sqrt(dx*dx + dy*dy);" +

                                                // check if the current fragment is within 3 pixels of the edge
                                                // Note: This can introduce alising when object is rotated in 3d space. Aliasing can be reduced by using:
                                                //          vec3 smoothedPosition = smoothstep(.5*edgeWidth, edgeWidth, v3PrimitivePosition);
                                                //          gl_fragColor*= min(min(smoothedPosition.x, smoothedPosition.y), smoothedPosition.z);
                                                //      which blends the inside half of the wireframe with the primitive color
                                                "   if(any(lessThan(v3PrimitivePosition, edgeWidth))) {" +
                                                "       gl_FragColor.rgb = vec3(0, 0, 0);" + //set the pixel color to black
                                                "   }" +
                                                "}";
    private int glUniformVec4Color;

    //projection matrix for translating worldCoordinate vectors into clipSpaceCoordinate vectors
    private volatile float[] mat4Projection = new float[4*4];
    //Note: 3D Matrix matrix multiplication doesn't support 3D translation so matrices are 4D with translation packed in the last column (vectors should have a 1 packed in their w-component). Ex:
    //          | . . . dx |   | x |
    //          | . . . dy | * | y | = ( x'+dx, y'+dy, z'+dz, w' ) <- Vector is translated by (dx, dy, dz)
    //          | . . . dz |   | z |
    //          | . . . .  |   | 1 |

    public final float kEndHWidth = 30.f; // in pixels
    private final float kDriftSpeedX = 100f; //pixels/sec
    private final float kDriftSpeedY = 100f; //pixels/sec

    private volatile FloatBuffer ends; //Note: We use a FloatBuffer for vertices because we read from it frequently and update it rarely

    private long lastNanoTime;
    private int width, height;
    private volatile float worldX, worldY;

    private int[] glStatus = new int[1];
    private GLSurfaceView view;

    public Lab0Renderer(GLSurfaceView v) {
        view = v;
    }

    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {

        // Init time stamp
        lastNanoTime = System.nanoTime();

        width = view.getWidth();
        height = view.getHeight();

        // Setup initial points
        ends = AllocateFloatBuffer(new float[]{
                .25f*width, .75f*height,
                .75f*width, .25f*height,
        });

        // Tell opengl to clear backbuffer to white
        GLES20.glClearColor(1.f, 1.f, 1.f, 1.f);

        // Tell opengl to create a GPU shader program
        glProgram = GLES20.glCreateProgram();

        // Compile shaders into GPU code
        int glVertexShader = loadShader(GLES20.GL_VERTEX_SHADER, kGlVertextShaderStr),
            glFragmentShader = loadShader(GLES20.GL_FRAGMENT_SHADER, kGlFragmentShaderStr);

        // Bind our shaders to the GPU program
        GLES20.glAttachShader(glProgram, glVertexShader);
        GLES20.glAttachShader(glProgram, glFragmentShader);

        // Link our program into an executable that can run on the GPU
        GLES20.glLinkProgram(glProgram);

        GLES20.glGetProgramiv(glProgram, GLES20.GL_LINK_STATUS, glStatus, 0);
        if(glStatus[0] == GLES20.GL_FALSE) Panic("Failed to link shader | INFO: [ " + GLES20.glGetProgramInfoLog(glProgram) + " ]");

        // clean up shader objects - already compiled into executable, no longer need the intermediate code sticking around
        // Note: shaders must be detached first or else delete won't free them from memory
        GLES20.glDetachShader(glProgram, glVertexShader);
        GLES20.glDetachShader(glProgram, glFragmentShader);

        GLES20.glDeleteShader(glVertexShader);
        GLES20.glDeleteShader(glFragmentShader);

        // Get handles to shader variables
        // Note: this must be done after we link the program - They refer to locations in the binary
        glUniformMat4Projection = GlUniformLocation(glProgram, "mat4Projection");
        glAttributeV4Position = GLAttributeLocation(glProgram, "v4Position");
        glAttributeV3PrimitiveVertex = GLAttributeLocation(glProgram, "v3PrimitiveVertex");

        glUniformVec4Color = GlUniformLocation(glProgram, "v4Color");

        EnableShaderProgram();

        Log("Created Renderer: [w: "+width+", h: "+height+"]");
    }

    @Override
    public void onSurfaceChanged(GL10 gl10, int w, int h) {
        Log("Surface Changed [w: "+w+", h: "+h+"]");

        width = w;
        height = h;

        // Set the viewport to the full screen
        // Note: opengl uses this to transform normalized device coordinates into screen space coordinates (pixel coordinates starting from the lower left)
        GLES20.glViewport(0, 0, width, height);

        // acquire lock to modify projection matrix
        synchronized(this) {
            // Setup an orthogonal projection matrix matrix
            //Note: Opengl uses column-major matrices so matrices are stored in memory as their transpose
            //      projection matrix is shown below for completeness. It linearly transforms world coordinates into clip space coordinates. See: https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/orthographic-projection-matrix
            //      This matrix can be generated with: Matrix.orthoM(mat4WorldProjection, 0, 0, width, 0, height, 1, -1);
            mat4Projection = new float[] {(2.f / width),    0,              0,      0,
                                          0,                (2.f/height),   0,      0,
                                          0,                0,              1,      0,
                                          -1,                -1,            0,      1};

            // apply current translation to new projection matrix
            mat4Projection[3*4 + 0] = XToClipSpace(worldX, worldY);
            mat4Projection[3*4 + 1] = YToClipSpace(worldX, worldY);
        }
    }

    @Override
    public void onDrawFrame(GL10 gl10) {
        // Clear the backbuffer
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);

        //upload projection matrix to GPU
        GLES20.glUniformMatrix4fv(glUniformMat4Projection,
                                  1,      //upload 1 matrix
                                  false,  //Transposed - MUST be set to false in opengles or else glUniformMatrix4fv will fail - see https://www.khronos.org/registry/OpenGL/specs/es/2.0/es_full_spec_2.0.pdf pg:38
                                  mat4Projection,
                                  0       //offset into array
                                 );

        //TASK 2 - Set line thickness to 'glLineWidth'
        GLES20.glLineWidth(glLineWidth);

        // Set primitive color to Black
        GLES20.glUniform4f(glUniformVec4Color, 0, 0, 0, 1.f);

        //TASK 1 - Observe how glRenderMode is used
        // Upload vertex buffer to the GPU
        GLES20.glVertexAttribPointer(glAttributeV4Position,
                                     2,                  //number of values specified in each array attribute element - Note: if this is less than 4 opengl fills the missing y and/or z with 0 and sets w to 1
                                     GLES20.GL_FLOAT,    //data type stored in attribute
                                     false,              //normalize fixpoint data (we're using float so it doesn't matter),
                                     0,                  //stride between elemets (0 means data is packed in an array)
                                     ends
                                    );

        // Start Drawing Vertex Buffer
        GLES20.glDrawArrays(glRenderMode,
                            0,  //start offset into vertex array
                            2   //number of vertices to draw
                           );


        // TASK 4 - draw a red square of width 2*kEndHWidth pixels
        //          around each ends[] to highlight the end points
        // Hint: use FloatBufferToArray to get a float[] from a FloatBuffer
        //       and AllocateFloatBuffer(float[]) to create a FloatBuffer
        {
            GLES20.glUniform4f(glUniformVec4Color, 1.f, 0, 0, 1.f);

            float endsArry[] = FloatBufferToArray(ends); //57543 cycles
            for(int i = 0; i < endsArry.length; i += 2) {

                FloatBuffer verts = AllocateFloatBuffer(new float[] {
                        endsArry[i] - kEndHWidth, endsArry[i + 1] - kEndHWidth,
                        endsArry[i] + kEndHWidth, endsArry[i + 1] - kEndHWidth,
                        endsArry[i] + kEndHWidth, endsArry[i + 1] + kEndHWidth,
                        endsArry[i] - kEndHWidth, endsArry[i + 1] + kEndHWidth,
                });

                GLES20.glVertexAttribPointer(glAttributeV4Position, 2, GLES20.GL_FLOAT, false, 0, verts);
                GLES20.glDrawArrays(GLES20.GL_LINE_LOOP, 0, 4);
            }
        }

        // TASK 5: draw a green line for x-axis from -width to width and y-axis from -height to height
        {
            GLES20.glUniform4f(glUniformVec4Color, 0, 1.f, 0, 1.f);
            FloatBuffer verts = AllocateFloatBuffer(new float[] {
                    -width,     0,
                    width,      0,
                    0,          -height,
                    0,          height,
            });

            GLES20.glVertexAttribPointer(glAttributeV4Position, 2, GLES20.GL_FLOAT, false, 0, verts);
            GLES20.glDrawArrays(GLES20.GL_LINES, 0, 4);
        }

        // TASK 7: draw a 100x100 cyan filled square around the Origin
        // Hint: make 2 triangles with GL_TRIANGLE_STRIP - https://en.wikipedia.org/wiki/Triangle_strip

        // TASK 8:  Use 'EnableTriangleWireframe' and 'DisableTriangleWireframe' to draw a wireframe around the blue square
        //          when 'drawWireframe' is true
        {
            GLES20.glUniform4f(glUniformVec4Color, 0, 1f, 1.f, 1.f);
            final float kHalfWidth = 50;
            FloatBuffer verts = AllocateFloatBuffer(new float[] {
                    -kHalfWidth, -kHalfWidth,     kHalfWidth, -kHalfWidth,      -kHalfWidth, kHalfWidth,
                    kHalfWidth, kHalfWidth,
            });

            if(drawWireframe) EnableTriangleWireframe(verts.capacity());

            GLES20.glVertexAttribPointer(glAttributeV4Position, 2, GLES20.GL_FLOAT, false, 0, verts);
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);

            if(drawWireframe) DisableTriangleWireframe();
        }

        // Compute elapsed time
        long time = System.nanoTime();
        float dt = (1E-9f)*(time - lastNanoTime);
        lastNanoTime = time;

        //Apply Drift to world
        if(driftEnabled) {

            float dx = kDriftSpeedX*dt,
                  dy = kDriftSpeedY*dt;

            TranslateWorld(dx, dy);
        }
    }

    public synchronized
    void TranslateWorld(float dx, float dy) {

        // apply translation to the matrix by multiplying it translation matrix:
        //      | 1 0 0 dx |
        //      | 0 1 0 dy |
        //      | 0 0 1 0  |
        //      | 0 0 0 1  |
        // Note: matrix is 4x4 column major so its stored as its transpose in memory
        mat4Projection[3*4 + 0] = XToClipSpace(dx, dy);
        mat4Projection[3*4 + 1] = YToClipSpace(dx, dy);

        worldX+= dx;
        worldY+= dy;

        // Log("Moved World: [dx: "+dx+", dy: "+dy+" | worldX: "+worldX+", worldY: "+worldY+"]");
    }

    public synchronized
    void TranslatePoint(int endIndex, float dx, float dy) {

        // TASK 9: translate the point ends[endIndex] by 'dx' and ends[endIndex+1] by 'dy'
        // Hint: use FloatBuffer put and get methods
        ends.put(endIndex, ends.get(endIndex)+dx);
        ends.put(endIndex+1, ends.get(endIndex+1)+dy);
    }

    public synchronized
    float[] GetEndsScreenCoords() {

        float halfWidth = .5f*width,
              halfHeight = .5f*height;

        float endsArry[] = FloatBufferToArray(ends);
        for(int i = 0; i < endsArry.length; i+=2) {
            float x = endsArry[i],
                  y = endsArry[i+1];

            //Note: android has the screen coords that start in upper left, but opengl has origin in bottomLeft so we negate the Y-component
            endsArry[i]   = halfWidth * XToClipSpace(x, y) + halfWidth;
            endsArry[i+1] = -halfHeight * YToClipSpace(x, y) + halfHeight;
        }

        return endsArry;
    }

    private synchronized
    float XToClipSpace(float x, float y) {
        //returns x component of multiplying projectionMatrix with (x, y, 0, 1)
        return x * mat4Projection[0*4 + 0] + y * mat4Projection[1*4 + 0] + mat4Projection[3*4 + 0];
    }

    private synchronized
    float YToClipSpace(float x, float y) {
        //returns y component of multiplying projectionMatrix with (x, y, 0, 1)
        return x * mat4Projection[0*4 + 1] + y * mat4Projection[1*4 + 1] + mat4Projection[3*4 + 1];
    }


    private static FloatBuffer AllocateFloatBuffer(float[] initVals) {

        // create native buffer that is contiguous in native memory (Java buffers can be out of order)
        // Note: 4 bytes per float
        FloatBuffer buffer = ByteBuffer.allocateDirect(initVals.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();

        buffer.put(initVals); // fill buffer
        buffer.rewind(); // set pointer to start of buffer

        return buffer;
    }

    private static float[] FloatBufferToArray(FloatBuffer buffer) {
        float arry[] = new float[buffer.capacity()];
        buffer.get(arry);
        buffer.rewind();
        return arry;
    }

    private static void Log(String msg) { Log.d("Lab0Renderer - MSG", msg); }
    private static void Warn(String msg) {
        Log.w("Lab0Renderer - WARN", msg);
    }
    private static void Error(String msg) { Log.e("Lab0Renderer - ERROR", msg); }

    private static void Panic(String msg) {
        Error(msg);
        System.exit(1);
    }

    private void EnableTriangleWireframe(int numVertices) {

        GLES20.glEnableVertexAttribArray(glAttributeV3PrimitiveVertex);

        //create primitive index array in the form: {1,0,0, 0,1,0, 0,0,1,...}
        float verts[] = new float[3*numVertices];
        for(int i = 0; i < numVertices; i++) {
            verts[3*i + i%3] = 1;
        }

        //upload to
        GLES20.glVertexAttribPointer(glAttributeV3PrimitiveVertex, 3, GLES20.GL_FLOAT, false, 0, AllocateFloatBuffer(verts));
    }

    private void DisableTriangleWireframe() {
        GLES20.glDisableVertexAttribArray(glAttributeV3PrimitiveVertex);
        GLES20.glVertexAttrib3f(glAttributeV3PrimitiveVertex, 0, 0, 0);
    }

    private int loadShader(int type, String code) {
        int shader = GLES20.glCreateShader(type);   // Create a new shader
        GLES20.glShaderSource(shader, code);        // Set shader code
        GLES20.glCompileShader(shader);             // Compile shader into gpu code

        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, glStatus, 0);
        if(glStatus[0] == GLES20.GL_FALSE) {
            Panic(  "Failed to compile shader with {\n"+
                        "\tSOURCE: [\n" +
                            "\t\t"+code.trim().replace("\n", "\n\t\t")+"\n" +
                        "\t]\n\n" +
                        "\tINFO: [\n" +
                            "\t\t"+GLES20.glGetShaderInfoLog(shader).trim().replace("\n", "\n\t\t")+"\n" +
                        "\t]\n" +
                    "}");
        }

        return shader;
    }

    private static int GlUniformLocation(int glProgram, String var) {
        int index = GLES20.glGetUniformLocation(glProgram, var);
        if(index == -1) Panic("Failed to find Uniform variable: ["+var+"] in glProgram: "+glProgram);
        return index;
    }

    private static int GLAttributeLocation(int glProgram, String var) {
        int index = GLES20.glGetAttribLocation(glProgram, var);
        if(index == -1) Panic("Failed to find Attribute variable: ["+var+"] in glProgram: "+glProgram);
        return index;
    }

    private void EnableShaderProgram() {

        // Tell opengl to upload program to GPU render pipeline
        GLES20.glUseProgram(glProgram);

        // Tell opengl to set the glAttributeV4Position on a perVertex bases from sequential values from vertexAttribPointer
        GLES20.glEnableVertexAttribArray(glAttributeV4Position);
    }

    private void DisableShaderProgram() {

        // Tell the GPU to go back to treating vertex attributes as registers.
        // Prevents the GPU from reading stale memory address which which may be freed and cause a
        // index out of bounds error
        GLES20.glDisableVertexAttribArray(glAttributeV4Position);
        GLES20.glDisableVertexAttribArray(glAttributeV3PrimitiveVertex);
    }
}
