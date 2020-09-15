package com.eecs487.jniteapot;

import android.app.Activity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

public class JniTeapotActivity extends Activity
                               implements SurfaceHolder.Callback2 {

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

		//Force the screen to stay on
		window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
	}

	@Override
	public void surfaceRedrawNeeded(SurfaceHolder surfaceHolder) {
		App.Log("Surface Needs Redraw");
	}
	@Override
	public void surfaceCreated(SurfaceHolder surfaceHolder) {
		App.Log("Surface Created!");
		App.NativeOnSurfaceCreated(surfaceHolder.getSurface(), getAssets());
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
