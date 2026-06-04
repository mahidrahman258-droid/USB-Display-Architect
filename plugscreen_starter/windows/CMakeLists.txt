cmake_minimum_required(VERSION 3.22)
project(PlugScreenHost CORE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable automatic Qt MOC (Meta-Object Compiler) and UIC (User Interface Compiler)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 1. Discover Qt6 Components
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

# 2. Discover FFmpeg Libraries
# Note: In production builds, users specify FFMPEG_DIR variable containing dynamic dependency folders
set(FFMPEG_LIBRARIES avcodec avutil avformat swscale)

# Common directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 3. List Source Files
set(SRC_FILES
    src/main.cpp
    src/logger.h
    src/logger.cpp
    src/config_manager.h
    src/config_manager.cpp
    src/virtual_display.h
    src/virtual_display.cpp
    src/frame_grabber.h
    src/frame_grabber.cpp
    src/video_encoder.h
    src/video_encoder.cpp
    src/usb_transport.h
    src/usb_transport.cpp
)

# 4. Create Host Binary Target
add_executable(PlugScreenHost ${SRC_FILES})

# 5. Link Dependencies
target_link_libraries(PlugScreenHost
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        ${FFMPEG_LIBRARIES}
        # Win32 platform libraries for Direct3D11, Desktop Duplication, WinUSB and SetupAPI
        d3d11
        dxgi
        winusb
        setupapi
)

# Ensure runtime binaries copy correctly on Windows build systems
if(MSVC)
    target_compile_options(PlugScreenHost PRIVATE /W4 /WX-)
    add_custom_command(TARGET PlugScreenHost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:PlugScreenHost>
        ${CMAKE_CURRENT_BINARY_DIR}
    )
endif()
