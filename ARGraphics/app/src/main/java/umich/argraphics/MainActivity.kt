package umich.argraphics

import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10


class MainActivity : AppCompatActivity(),  GLSurfaceView.Renderer {
    private var surfaceView: GLSurfaceView? = null
    private var nativeApplication: Long = 0 // Pointer to native instance
    private var viewportChanged: Boolean = false
    private var viewportWidth = 0
    private var viewportHeight = 0


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        surfaceView = findViewById(R.id.surfaceView)
//        displayRotationHelper = DisplayRotationHelper(/*context=*/this)
//
//        // Set up touch listener.
//        tapHelper = TapHelper(/*context=*/this)
//        surfaceView.setOnTouchListener(tapHelper)

        // Set up renderer.
        surfaceView?.setPreserveEGLContextOnPause(true);
        surfaceView?.setEGLContextClientVersion(2);
        surfaceView?.setEGLConfigChooser(8, 8, 8, 8, 16, 0); // Alpha used for plane blending.
        surfaceView?.setRenderer(this);
        surfaceView?.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        surfaceView?.setWillNotDraw(false);

//        installRequested = false
//        calculateUVTransform = true
//
//        depthSettings.onCreate(this)
//        instantPlacementSettings.onCreate(this)
//        val settingsButton = findViewById(R.id.settings_button)
//        settingsButton.setOnClickListener(
//            object : View.OnClickListener() {
//                fun onClick(v: View) {
//                    val popup = PopupMenu(this@HelloArActivity, v)
//                    popup.setOnMenuItemClickListener(???({ this@HelloArActivity.settingsMenuClick() }))
//                    popup.inflate(R.menu.settings_menu)
//                    popup.show()
//                }
//            })

        JniInterface.assetManager = getAssets();
        nativeApplication = JniInterface.createNativeApplication(getAssets());
    }



    public override fun onDestroy() {
        super.onDestroy()

        // Synchronized to avoid racing onDrawFrame.
        synchronized(this) {
            JniInterface.destroyNativeApplication(nativeApplication)
            nativeApplication = 0
        }
    }

    override fun onSurfaceCreated(gl: GL10, config: EGLConfig) {
        GLES20.glClearColor(0.1f, 0.1f, 0.1f, 1.0f)
        JniInterface.onGlSurfaceCreated(nativeApplication)
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        viewportWidth = width
        viewportHeight = height
        viewportChanged = true
    }

    override fun onDrawFrame(gl: GL10) {
        // Synchronized to avoid racing onDestroy.
        synchronized(this) {
            if (nativeApplication == 0L) {
                return
            }
            if (viewportChanged) {
                val displayRotation = windowManager.defaultDisplay.rotation
                JniInterface.onDisplayGeometryChanged(
                    nativeApplication, displayRotation, viewportWidth, viewportHeight
                )
                viewportChanged = false
            }
            JniInterface.onGlSurfaceDrawFrame(
                nativeApplication)

        }
    }
}

