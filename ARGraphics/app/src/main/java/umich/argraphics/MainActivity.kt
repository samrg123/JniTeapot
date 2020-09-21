package umich.argraphics

import android.hardware.display.DisplayManager
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import android.widget.TextView
import umich.argraphics.JniInterface.onResume
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10


class MainActivity : AppCompatActivity(),  GLSurfaceView.Renderer, DisplayManager.DisplayListener {
    private var surfaceView: GLSurfaceView? = null
    private var nativeApplication: Long = 0 // Pointer to native instance
    private var viewportChanged: Boolean = false
    private var viewportWidth = 0
    private var viewportHeight = 0
   // private var helloTextView :TextView = findViewById<TextView>(R.id.firstTextView)


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

        var helloTextView = findViewById<TextView>(R.id.firstTextView)
        helloTextView.setText("hiiiiiiiiii")
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

    override fun onDisplayAdded(displayId: Int) {}

    override fun onDisplayRemoved(displayId: Int) {}

    override fun onDisplayChanged(displayId: Int) {
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
            //var helloTextView = findViewById<TextView>(R.id.firstTextView)
            //val text: String = JniInterface.getDebugString(nativeApplication)
            //helloTextView.setText("hiiiiiiiiii")
            //helloTextView.setText(text)
        }
    }
    override fun onResume() {
        super.onResume()
        // ARCore requires camera permissions to operate. If we did not yet obtain runtime
        // permission on Android M and above, now is a good time to ask the user for it.
        if (!CameraPermissionHelper.hasCameraPermission(this)) {
            CameraPermissionHelper.requestCameraPermission(this)
            return
        }

        onResume(nativeApplication, applicationContext, this)
        surfaceView!!.onResume()
//        loadingMessageSnackbar = Snackbar.make(
//            this@HelloArActivity.findViewById(android.R.id.content),
//            "Searching for surfaces...",
//            Snackbar.LENGTH_INDEFINITE
//        )
        // Set the snackbar background to light transparent black color.
//        loadingMessageSnackbar.getView().setBackgroundColor(-0x40cdcdce)
//        loadingMessageSnackbar.show()
//        planeStatusCheckingHandler.postDelayed(
//            planeStatusCheckingRunnable, SNACKBAR_UPDATE_INTERVAL_MILLIS
//        )

        // Listen to display changed events to detect 180° rotation, which does not cause a config
        // change or view resize.
        getSystemService(DisplayManager::class.java).registerDisplayListener(
            this,
            null
        )
    }
}

