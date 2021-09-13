# Welcome to the JniTeapot Repo

The JniTeapot repo is an augmented reality app powered by Google's ArCore API which explores realtime reflections and shadowing techniques.

<br><br>

## How to compile the project
JniTeapot includes a portable install of VSCode including all the extensions needed to build and run the JniTeapot project. However, JniTeapot does not include a copy of the Android SDK and NDK which must be installed first in order to build the project.
 
To build VSCode execute `code.cmd` and press `shift+ctrl+B`. After the build completes you run the app on a connected android device by pressing `F5`.

You can access the following commands in the VSCode command pallet:
|Command                    |Description                                                        |
|:--------------------------|:------------------------------------------------------------------|
|`Android: View Logcat`     |Opens a logcat view of the connected android device                |
|`Run Android Emulator`     |Opens an android emulator installed on the Machine<sup>1,2</sup>   |
|`Run task`                 |Runs a designated build task such as Build Release APK or Clean    |

><small>
1. This currently requires you to add `%LOCALAPPDATA%\Android\Sdk\emulator` and `%LOCALAPPDATA%\Android\Sdk\platform-tools` to your system path.
2. JniTeapot currently only works with emulators running Android API version 29 with GoogleAPIs and no Google Play. We recommend using the Pixel 3a as an emulation device.
></small> 


<br><br>

## What about Android Studio?
JniTeapot was originally developed in and is backwards compatible with Android studio. The decision to switch to using VSCode came in an effort to make the development process more lightweight and add support for custom development extensions such as the JniTeapot C++ extension which adds support for GLSL syntax highlighting embedded in C++ code via the the `Shader` macro. 

To build JniTeapot with Android Studio. Navigate to `File->New->Import Project...` and select the root JniTeapot folder.   

> **Note:** There currently isn't any NDK debugging support for JniTeapot via VSCode. If you need to step though the NDK assembly we recommend using Android Studio with the native debugging turned on.

<br><br>

## What about Documentation?
Documentation about how JniTeapot works can be found at the [JniTeapot wiki](https://github.com/UM-EECS-441/graphics/wiki/JniTeapot-Documentation).
A list of known bugs and planned updates can be found in [TODO.txt](TODO.txt) 

> **Note:** Because JniTeapot is still in its development phase the documentation is subject to frequent changes and may not always be up to date.