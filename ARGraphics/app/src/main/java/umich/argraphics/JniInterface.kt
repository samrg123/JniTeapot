package umich.argraphics

import android.app.Activity
import android.content.Context
import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.opengl.GLUtils
import android.util.Log
import java.io.IOException

/** JNI interface to native layer.  */
object JniInterface {

    private val TAG = "JniInterface"
    internal var assetManager: AssetManager? = null

    init {
        System.loadLibrary("argraphics_native")
    }

    external fun createNativeApplication(assetManager: AssetManager): Long

    external fun destroyNativeApplication(nativeApplication: Long)

    external fun onPause(nativeApplication: Long)

   // external fun getDebugString(nativeApplication: Long): String

    external fun onResume(nativeApplication: Long, context: Context, activity: Activity)

    /** Allocate OpenGL resources for rendering.  */
    external fun onGlSurfaceCreated(nativeApplication: Long)

    /**
     * Called on the OpenGL thread before onGlSurfaceDrawFrame when the view port width, height, or
     * display rotation may have changed.
     */
    external fun onDisplayGeometryChanged(
        nativeApplication: Long, displayRotation: Int, width: Int, height: Int
    )

    /** Main render loop, called on the OpenGL thread.  */
    external fun onGlSurfaceDrawFrame(
        nativeApplication: Long
    )

    /** OnTouch event, called on the OpenGL thread.  */
    external fun onTouched(nativeApplication: Long, x: Float, y: Float)

    /** Get plane count in current session. Used to disable the "searching for surfaces" snackbar.  */
//    external fun hasDetectedPlanes(nativeApplication: Long): Boolean
//
//    external fun isDepthSupported(nativeApplication: Long): Boolean
//
//    external fun onSettingsChange(
//        nativeApplication: Long, isInstantPlacementEnabled: Boolean
//    )

    fun loadImage(imageName: String): Bitmap? {

        try {
            return BitmapFactory.decodeStream(assetManager!!.open(imageName))
        } catch (e: IOException) {
            Log.e(TAG, "Cannot open image $imageName")
            return null
        }

    }

    fun loadTexture(target: Int, bitmap: Bitmap) {
        GLUtils.texImage2D(target, 0, bitmap, 0)
    }


}