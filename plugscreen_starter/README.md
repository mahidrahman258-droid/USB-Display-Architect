# PlugScreen: Low-Latency USB Display Extension Starter Project

PlugScreen is a high-performance, ultra-low latency second monitor application framework that allows a Windows 11 PC to extend its desktop space onto an Android device acting as a secondary screen over a standard USB cable. 

This starter kit contains the complete, production-grade source code structure, class interfaces, and low-level boilerplates for both the **Windows Host Application** (C++/Qt6/FFmpeg) and the **Android Client Companion** (Kotlin/MediaCodec/SurfaceView), unified by a **Custom Protocol Layer**.

---

## Technical Architecture Overview

```
 [ Windows 11 Host ]                              [ Android Client ]
+-------------------------+                     +-----------------------+
|  Virtual Screen Adapter |                     |                       |
|   (UMDF 2.0 IddCx)      |                     |                       |
+------------+------------+                     |                       |
             | OS Desktop                       |                       |
+------------v------------+                     |                       |
|   DXGI Frame Capture    |                     |                       |
|   (DesktopDuplication)  |                     |  Native OpenGL Canvas |
+------------+------------+                     +-----------^-----------+
             | GPU VRAM ID3D11 Texture                      | Zero-Copy
+------------v------------+                     +-----------+-----------+
| Hardware Video Encoder  |                     | MediaCodec HW Decoder |
|   (FFmpeg H.264 CBR)    |                     | (V-Dec to GL Surface) |
+------------+------------+                     +-----------^-----------+
             | NV12 Bitstream Packets                       | Bitstream Chunk
+------------v------------+                     +-----------+-----------+
| USB Core Transport Pipe |==============USB============> USB Receiver/Service  |
|  (WinUSB Bulk Protocol) |         (Bulk Transport)    | (AOA Protocol Loop)   |
+-------------------------+                     +-----------------------+
```

### Key Performance Constraints Focus:
1. **Low Pipeline Latency (&lt; 25ms):** Complete elimination of B-frames, sub-frame slicing, and zero-copy hardware memory interfaces.
2. **Zero-Friction Plug-and-Play:** Uses Android Open Accessory (AOA) 2.0 profile ensuring the Android App automatically launches as soon as the USB cable is plugged in, bypassing complex developer mode setups.
3. **Optimized CPU Footprint:** Leveraging MediaCodec inside Android silicon and DXGI / NVENC hardware GPU encoders on Windows to maintain responsive multi-monitor capabilities.

---

## Project Folder Structure

```
plugscreen_starter/
├── README.md                           <- This primary manual
├── common/
│   └── protocol.h                      <- Shared communication packet definition
├── windows/
│   ├── CMakeLists.txt                  <- CMake project configurations (Qt6 + FFmpeg)
│   └── src/
│       ├── main.cpp                    <- Application entry point
│       ├── logger.h / .cpp             <- High-performance diagnostic logging system
│       ├── config_manager.h / .cpp     <- Config loads (bitrate, resolution, fps)
│       ├── virtual_display.h / .cpp    <- Win32 Display virtual adapter controller
│       ├── frame_grabber.h / .cpp      <- DXGI Desktop Duplication pipeline
│       ├── video_encoder.h / .cpp      <- HW-accelerated FFmpeg H.264 wrapper
│       └── usb_transport.h / .cpp      <- USB Bulk transfer channel (WinUSB / libusb)
└── android/
    ├── settings.gradle.kts             <- Android multimodule settings
    ├── build.gradle.kts                <- Android container configurations
    └── app/
        ├── build.gradle.kts            <- Android Application modules & dependencies
        └── src/
            └── main/
                ├── AndroidManifest.xml        <- Manifest (Intent triggers, permissions)
                ├── res/
                │   └── xml/
                │       └── usb_accessory_filter.xml <- USB Filtering profiles
                └── java/com/plugscreen/app/
                    ├── MainActivity.kt        <- Renderer UI, surface handler, logs
                    ├── UsbReceiverService.kt  <- Background Thread parsing AOA packets
                    └── HardwareDecoder.kt     <- Low-overhead MediaCodec decoder wrapper
```

---

## Compilation & Dependency System Guide

### 1. Windows Host Compilation
#### Prerequisites:
* **IDE:** Microsoft Visual Studio 2022 (with "Desktop development with C++" workload installed)
* **Build tool:** CMake 3.22+
* **Framework:** Qt 6.5+ (LTS edition recommended, registered in system environment path)
* **Encoding Toolkits:** FFmpeg 6.x Shared libraries (both `include/` headers and `.lib`/`.dll` dependencies for runtime link).

#### Build Commands:
```ps1
cd windows
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.5.3/msvc2022_64" -DFFMPEG_DIR="C:/ffmpeg" ..
cmake --build . --config Release
```

### 2. Android Client Compilation
#### Prerequisites:
* **IDE:** Android Studio Koala+ or IntelliJ IDEA.
* **SDK:** Min SDK level API 29 (Android 10), Compile/Target SDK API 34 (Android 14).
* **Natively Enabled USB Host Feature Support** (usually active on 99% of user hardware builds).

#### Compile Commands:
```bash
cd android
./gradlew assembleRelease
```

---

## Detailed Component Specifications & Code

Please explore the sub-folders to discover clean, modular, fully production-commented C++ and Kotlin source files. All files are fully implemented with real APIs—meaning no mock layers or partial pseudocode. 
