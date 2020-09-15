package com.eecs487.jniteapot;

import android.util.Log;
import android.view.Surface;
import android.content.res.AssetManager;

public class App {

	static {
		System.loadLibrary("JniTeapot");
	}

	public static native void NativeOnSurfaceCreated(Surface surface, AssetManager assets);

	public static void Log(String msg)   { Log.i(LogPrefixStr("MSG"), msg); }
	public static void Warn(String msg)  { Log.w(LogPrefixStr("WARN"), msg); }
	public static void Error(String msg) { Log.e(LogPrefixStr("ERROR"), msg); }

	public static void Panic(String msg) {
		Error(msg);
		System.exit(1);
	}

	private static String LogPrefixStr(String suffix) {
		StackTraceElement trace[] = Thread.currentThread().getStackTrace();

		// Note:    trace[0] is native thread,
		//          trace[1] is java thread,
		//          trace[2] is us,
		//          trace[3] is log call
		//          trace[4] is log call caller

		final int callerIndex = 4;
		if(trace.length < callerIndex) {
			return " => UnknownClass.UnknownMethod:UnknownLine - " + suffix;
		}

		StackTraceElement caller = trace[callerIndex];
		return " => "+caller.getClassName()+"."+caller.getMethodName()+":"+caller.getLineNumber()+" - "+suffix;
	}
}
