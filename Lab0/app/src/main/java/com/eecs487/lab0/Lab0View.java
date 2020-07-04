package com.eecs487.lab0;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.MotionEvent;

public class Lab0View extends GLSurfaceView {

    public Lab0Renderer renderer;

    private final float kClickDistance = 30.f; //in pixels

    private int draggingEnd = -1;
    private float lastX, lastY;

    Lab0View(Context context) {
        super(context);

        // tell GLSurfaceView eglFactory to create a openGLES context that supports openGLES 2.0
        setEGLContextClientVersion(2);

        //prevent opengl context being destroyed when app is paused
        setPreserveEGLContextOnPause(true);

        renderer = new Lab0Renderer(this);
        setRenderer(renderer);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        float x = e.getRawX(),
              y = e.getRawY();


        switch(e.getAction()) {
            case MotionEvent.ACTION_DOWN: {

                float[] endsArry = renderer.GetEndsScreenCoords();
                for(int i = 0; i < endsArry.length; i+=2) {

                    // See if we are withing the click distance;
                    if(Math.abs(endsArry[i + 0] - x) < kClickDistance &&
                       Math.abs(endsArry[i + 1] - y) < kClickDistance) {

                        draggingEnd = i;
                        break;
                    }
                }
                Log("Clicked on point [x: "+x+", y: "+y+", end: "+draggingEnd+"]");
                lastX = x;
                lastY = y;
            } break;

            case MotionEvent.ACTION_UP: {
                draggingEnd = -1;
                Log("Released point");
            } break;

            case MotionEvent.ACTION_MOVE: {
                float dx = x - lastX,
                      dy = lastY - y; // view-coordinates start from upper left, but openGL uses bottom-left origin so we flip y

                // Log("Moved point [dx: "+dx+", dy: "+dy+"]");

                lastX = x;
                lastY = y;

                //TASK 8: use 'renderer.TranslateWorld' to move world x by 'dx' and y by 'dy' if draggingEnd == -1
                //TASK 9: use 'renderer.TranslatePoint' to move point 'draggingEnd' by ('dx','dy') if 'draggingEnd != -1'
                if(draggingEnd != -1) renderer.TranslatePoint(draggingEnd, dx, dy);
                else                  renderer.TranslateWorld(dx, dy);
            } break;
        }

        return true; // consumed touch event
    }

    private static void Log(String msg) { Log.d("Lab0View - MSG", msg); }
    private static void Warn(String msg) {
        Log.w("Lab0View - WARN", msg);
    }
    private static void Error(String msg) { Log.e("Lab0View - ERROR", msg); }
}
