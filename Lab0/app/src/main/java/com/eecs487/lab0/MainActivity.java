package com.eecs487.lab0;

import android.app.Activity;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup.LayoutParams;

import java.util.zip.Inflater;

public class MainActivity extends Activity {

    private Lab0View glView;

    public void ToggleRenderMode(View v) {
        Log("Toggle Render Mode");

        //TASK 1 - toggle 'glView.renderer.glRenderMode' to change between rendering points and lines
        glView.renderer.glRenderMode = (glView.renderer.glRenderMode == GLES20.GL_POINTS) ? GLES20.GL_LINES
                                                                                          : GLES20.GL_POINTS;
    }

    public void ToggleLineWidth(View v) {
        Log("Toggle Line Width");
        glView.renderer.glLineWidth = (glView.renderer.glLineWidth == 1) ? 20 : 1;
    }

    public void ToggleDrift(View v) {
        Log("Toggle Drift");

        //TASK 6 - toggle 'glView.renderer.driftEnabled'
        glView.renderer.driftEnabled = !glView.renderer.driftEnabled;
    }

    public void ToggleWireframe(View v) {
        Log("Toggle Polygon Mode");

        //TASK 8 - toggle 'glView.renderer.drawWireframe'
        glView.renderer.drawWireframe = !glView.renderer.drawWireframe;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        //Create opengl view
        glView = new Lab0View(this);
        setContentView(glView);

        //create a button layout view - Note: this gets added on top of opengl view you can explicitly the order with buttonView.setZ(zValue)
        View buttonView = getLayoutInflater().inflate(  R.layout.button_layout,
                                                        null    //parent viewGroup
                                                     );
        addContentView(buttonView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));

        Log("Created App");
    }

    private static void Log(String msg) { Log.d("Lab0Activity - MSG", msg); }
    private static void Warn(String msg) {
        Log.w("Lab0Activity - WARN", msg);
    }
    private static void Error(String msg) { Log.e("Lab0Activity - ERROR", msg); }

}