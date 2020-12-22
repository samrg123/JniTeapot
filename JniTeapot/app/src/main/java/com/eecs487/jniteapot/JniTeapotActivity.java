package com.eecs487.jniteapot;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import java.util.ArrayList;

public class JniTeapotActivity extends Activity
                               implements SurfaceHolder.Callback2 {

	private WindowManager windowManager;
	private Display display;
	private Surface surface;

	private enum PermissionRequestCode {StartupRequest, FailedRequest}

	public void onCreate(Bundle savedState) {
		super.onCreate(savedState);
		App.Log("Created Activity");

		Window window = getWindow();
		View decorView = window.getDecorView();

		//set fullscreen sticky imersive mode
		decorView.setSystemUiVisibility(
			View.SYSTEM_UI_FLAG_IMMERSIVE |
			View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
			View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
			View.SYSTEM_UI_FLAG_FULLSCREEN

			//THESE ARE FOR SWITCHING VIEWS? --- not really sure why we need them TODO: look into these
			// | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
			// View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
			// View.SYSTEM_UI_FLAG_LAYOUT_STABLE
		);

		// give us control of surface updates (only one thread can draw to a surface)
		window.takeSurface(this);

		// windowManager = getSystemService(WindowManager.class);
		// display = getDisplay();

		//Force the screen to stay on
		window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResult) {
		ArrayList<String> failedRequests = new ArrayList<String>(permissions.length);

		for(int i = 0; i < permissions.length; ++i) {
			if(grantResult[i] != PackageManager.PERMISSION_GRANTED) {
				App.Warn("Requested permission not granted. Will Request again { permission: "+permissions[i]+", grantResult: "+grantResult[i]+", requestCode: "+requestCode+" }");
				failedRequests.add(permissions[i]);
			}
		}

		if(failedRequests.isEmpty()) {

			//we have all permissions - start the native app
			App.NativeStartApp(surface, getAssets(), this);

		} else {
			requestPermissions(failedRequests.toArray(new String[failedRequests.size()]),
			                   PermissionRequestCode.FailedRequest.ordinal());
		}
	}

	@Override
	public void surfaceRedrawNeeded(SurfaceHolder surfaceHolder) {
		App.Log("Surface Redraw Needed");

		// //TODO: see if there is an easier way to get width and height... maybe in native?
		// WindowMetrics metrics = windowManager.getCurrentWindowMetrics();
		// Rect bounds = metrics.getBounds();
		// App.NativeSurfaceRedraw(display.getRotation(), bounds.width(), bounds.height());
	}
	@Override
	public void surfaceCreated(SurfaceHolder surfaceHolder) {
		App.Log("Surface Created!");
		surface = surfaceHolder.getSurface();

		//request camera permissions if we don't have them
		if(checkSelfPermission(Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {

			App.Log("Requesting Camera permissions");
			requestPermissions(new String[]{ Manifest.permission.CAMERA }, PermissionRequestCode.StartupRequest.ordinal());

		} else {

			//we have all permissions - start the native app
			App.NativeStartApp(surface, getAssets(), this);
		}
	}

	@Override
	public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int width, int height) {
		App.Log("Surface Changed!");
	}
	@Override
	public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
		App.Log("Surface Destroyed");
	}
}
