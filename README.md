# OpenMapleClient
OpenMapleClient is a custom, made-from-scratch game client.

# Supported versions
- The client is currently compatible with version 83 servers.
- The client has only been tested with [Cosmic] on PC.
- The client has only been tested with [MapleServerAndroid] on Android.

# Building
Requires JDK 17, Android SDK, NDK 26.1.10909125 and CMake 3.22.1.

The `glfm` and `stb` dependencies are git submodules, so they have to be cloned as well:

```
git clone --recurse-submodules https://github.com/speedyHKjournalist/OpenMapleClient.git
```

Build the debug APK (`gradlew.bat` on Windows):

```
./gradlew assembleDebug
```

The APK is written to `app/build/outputs/apk/debug/app-debug.apk`. Runtime logs are
written to logcat under the `OpenMapleClient` tag:

```
adb logcat -s OpenMapleClient
```

# How To Play
1. Download required NX files from [NXFiles].
2. Install Client APK on your Android phone.
3. Copy all nxfiles to location /storage/emulated/0/Android/data/com.OpenMapleClient/files using file manager app like Root Explorer.
4. Launch A Server either on PC or on Android using servers mentioned Above.
5. Launch Client and specify hostip to connect to server.
6. Play!

# Bug
1. Graphic issues.
2. Map issues.
3. Quests are not implemented.

[Cosmic]:              https://github.com/P0nk/Cosmic/tree/master
[MapleServerAndroid]:  https://github.com/speedyHKjournalist/MapleServerAndroid/tree/main
[NXFiles]: https://drive.google.com/drive/folders/1vloXiDOAQOUPfSO3aCjRBJDbtQ1PZTuU?usp=share_link