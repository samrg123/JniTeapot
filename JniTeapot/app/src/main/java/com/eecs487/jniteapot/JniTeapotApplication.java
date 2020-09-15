package com.eecs487.jniteapot;

import android.app.Application;

public class JniTeapotApplication extends Application {

	public void onCreate() {
		super.onCreate();
		App.Log("Created application");
	}
}
