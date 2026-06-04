import { useState } from 'react';
import { ScreenShare, Laptop, Smartphone, HelpCircle, HardDrive, Cpu, Settings, RefreshCw, Send, ShieldAlert, CpuIcon, Cable } from 'lucide-react';

interface ComponentDetail {
  id: string;
  title: string;
  sub: string;
  desc: string;
  techKeywords: string[];
  tips: string;
  apis: string[];
}

export default function ArchitectureVisualizer() {
  const [activeId, setActiveId] = useState<string>('iddcx');

  const components: Record<string, ComponentDetail> = {
    // Windows Laptop Components
    iddcx: {
      id: 'iddcx',
      title: 'UMDF 2.0 Indirect Display Driver (IDD)',
      sub: 'Windows Display Driver Model (WDDM 2.0+)',
      desc: 'An Indirect Display Driver acts as a user-mode class driver that register-creates a virtual monitor on the active GPU. When Windows detects this driver load, it creates a virtual monitor surface, allocates a desktop frame plane in GPU memory, and starts issuing display frames into DXGI surfaces inside the GPU swap-chain.',
      techKeywords: ['C++', 'UMDF 2.0', 'IddCxClass', 'IddCxMonitorCreate', 'Virtual Headless Adapter'],
      tips: 'Always implement as a UMDF 2.0 user-mode driver instead of a legacy kernel-mode mirror driver (which is deprecated and blocks WDDM security models). An IDD can be deployed safely without typical kernel crash risks.',
      apis: ['IddCxDeviceInitConfig', 'IddCxMonitorArrival', 'IddCxAdapterInitFinished'],
    },
    dxgi: {
      id: 'dxgi',
      title: 'DXGI Desktop Duplication API (DDA)',
      sub: 'Zero-Copy VRAM Monitor Capture',
      desc: 'The Desktop Duplication API grabs screen frame textures directly out of the primary or secondary virtual monitor swap chain. It provides raw GPU pointers to ID3D11Texture2D resources, enabling direct hardware access without roundtripping frames back to main system memory (CPU RAM).',
      techKeywords: ['DXGI', 'Direct3D 11', 'IDXGIOutputDuplication', 'AcquireNextFrame', 'GPU Shader Resources'],
      tips: 'Cache your Direct3D 11 device and monitor duplicate instances. Always use ReleaseFrame() as fast as possible to avoid stalling the desktop window manager (DWM) composition pipeline.',
      apis: ['IDXGIOutput5::DuplicateOutput1', 'IDXGIOutputDuplication::AcquireNextFrame', 'ID3D11DeviceContext::CopyResource'],
    },
    nvenc: {
      id: 'nvenc',
      title: 'Hardware Video Encoder (NVENC / AMF / QSV)',
      sub: 'Direct GPU-Hardware Compression Plane',
      desc: 'The compressed video encoder encodes the raw D3D11 textures straight out of DXGI inside the GPU subsystem. By feeding the VRAM pointers directly to NVENC (on NVIDIA) or Intel QuickSync, the frame is compressed into standard NAL H.264/H.265 slices in microseconds, completely dodging system bottlenecks.',
      techKeywords: ['H.264 AVC', 'H.265 HEVC', 'DirectX Video Acceleration (DXVA)', 'NVIDIA NVENC SDK', 'Intel OneVPL'],
      tips: 'Configure the encoder strictly for Ultra-low latency rate control (CBR, single frame buffer, zero B-frames, and intra-refresh slices). B-frames require future frame references, adding standard 30-100ms pipeline delay.',
      apis: ['NvEncCreateEncoder', 'NvEncEncodePicture', 'MFCreateSinkWriterFromURL'],
    },
    daemon: {
      id: 'daemon',
      title: 'Windows Usermode Streaming Daemon',
      sub: 'High-Speed Packetizer & Frame Sequencer',
      desc: 'A background service responsible for loading the virtual monitor driver, subscribing to the DirectX capture loops, pushing frames into the hardware encoder, adding our 24-byte custom framing headers (with sequencing, timestamps, and keyframe state markers) and piping bulk binary payloads to the active USB driver port.',
      techKeywords: ['Asynchronous Win32 Threads', 'C++', 'Rust', 'Socket TCP Streamers', 'Buffer Ring Allocations'],
      tips: 'Use custom ring packet-buffers to prevent memory reallocation heap thrashing. Set your threat affinity masks directly to high-priority threads (Time-Critical scheduling rules) to protect against UI audio/mouse lag spikes.',
      apis: ['SetThreadPriority(THREAD_PRIORITY_TIME_CRITICAL)', 'WinUSB_WritePipe', 'CreateIoCompletionPort'],
    },

    // USB Physical Bridges
    usb_phy: {
      id: 'usb_phy',
      title: 'USB Bulk Physical Interface Pipeline',
      sub: 'Hardware Transport Bridge Layer',
      desc: 'The physical channel carrying the frame packets. To maintain low latency, developers have three options: ADB port forwarding (very high friction), USB Accessory Mode (direct accessory connection), or custom WinUSB configurations. USB 3.0 offers 5Gbps lines, giving enough bandwidth for lossless or raw pixel transport if needed.',
      techKeywords: ['WinUSB Driver Stack', 'ADB Reverse Port Forward', 'Android Accessory Mode AOA 2.0', 'Bulk Endpoints'],
      tips: 'Dodging network socket latency requires direct USB Bulk pipe writes. Use a dual-endpoint scheme: Endpoint IN for raw Touch/Input packets, and Endpoint OUT for fast Video/Control display channels.',
      apis: ['WinUsb_Initialize', 'WinUsb_ReadPipe', 'UsbAccessoryManager'],
    },

    // Android Companion App
    rec_socket: {
      id: 'rec_socket',
      title: 'Android Accessory Receiver Service',
      sub: 'Native Packet Re-assembler Queue',
      desc: 'A foreground Android Service that binds directly to the incoming USB bulk file descriptor or ADB port socket. It extracts raw byte packs, validates custom headers, sorts frame slices using sequence values, handles out-of-order UDP-like packet states, and streams continuous NAL units straight to the MediaCodec block.',
      techKeywords: ['Java NIO ByteBuffers', 'Android UsbAccessory', 'Native Byte Alignment', 'Low-Lock Thread Queues'],
      tips: 'Keep allocations on startup. Avoid GC sweeps during active rendering cycles! Directly reuse static byte buffers in java.nio and pass their offsets to the JNI layer to decrease crossing-overhead.',
      apis: ['UsbManager.openAccessory', 'ParcelFileDescriptor.getFileDescriptor', 'ByteBuffer.allocateDirect'],
    },
    mediacodec: {
      id: 'mediacodec',
      title: 'Android Hardware MediaCodec Engine',
      sub: 'Direct SoC Decoder Mapping',
      desc: 'MediaCodec acts as the primary API into the device\'s dedicated Silicon hardware decoders. By pushing valid H.264 or HEVC NAL slices directly into decoding queues, the device outputs fully decompressed video buffers in hardware memory lines (Gralloc handles) in less than 3 milliseconds without eating system CPU cores.',
      techKeywords: ['MediaCodec API', 'Hardware Acceleration SoC', 'Gralloc Buffers', 'Direct Input Buffers'],
      tips: 'Pass a direct native Surface object (from SurfaceView) to MediaCodec.configure(). This enables ZERO-COPY hardware decoding, routing frame textures from decoding units straight into phone GPU compositor overlays.',
      apis: ['MediaCodec.createDecoderByType', 'MediaCodec.dequeueInputBuffer', 'MediaCodec.queueInputBuffer'],
    },
    gles_render: {
      id: 'gles_render',
      title: 'OpenGL ES SurfaceTexture compositor',
      sub: 'EGL Surface Renderer & Touch Capturer',
      desc: 'A custom OpenGL rendering thread that swaps active buffers and draws the hardware decoded display output on the phone GLSurface. It coordinates display aspect ratios, custom stretch/upscaling algorithms and captures touch MotionEvents from user screen interactions.',
      techKeywords: ['OpenGL ES 3.0', 'GLSurfaceView', 'EGLContext', 'Choreographer Frame Callbacks', 'MotionEvent'],
      tips: 'Hook into Android\'s Choreographer API for display callbacks. Only request render frame sweeps when a new decoded buffer becomes active to avoid screen tearing or battery resource drain.',
      apis: ['SurfaceTexture.updateTexImage', 'EGLExt::eglPresentationTimeANDROID', 'Choreographer.postFrameCallback'],
    },
  };

  return (
    <div className="bg-slate-900 rounded-xl border border-slate-800 shadow-xl p-6">
      <div className="flex items-center gap-2.5 mb-6">
        <span className="p-2 bg-slate-950 text-cyan-400 rounded-lg border border-cyan-900/85">
          <ScreenShare className="w-5 h-5 animate-pulse" />
        </span>
        <div>
          <h3 className="text-base font-bold text-white uppercase tracking-wide font-mono">
            // USB_MULTISCREEN_INTERACTIVE_ARCHITECTURE
          </h3>
          <p className="text-xs text-slate-405 text-slate-400 mt-1">
            Tap on any system module on either Windows or Android to inspect its core role, driver structures, high-performance APIs, and optimization loops.
          </p>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
        
        {/* Schematic Grid (7 Columns) */}
        <div className="lg:col-span-7 flex flex-col justify-center space-y-4">
          
          {/* Top labels */}
          <div className="flex justify-between items-center text-[10px] font-mono font-bold text-slate-500 px-2 uppercase tracking-wider">
            <span className="flex items-center gap-1.5"><Laptop className="w-4 h-4 text-slate-600" /> WINDOWS OS (HOST)</span>
            <span className="flex items-center gap-1.5"><Cable className="w-4 h-4 text-slate-600" /> TRANSPORT BUS</span>
            <span className="flex items-center gap-1.5"><Smartphone className="w-4 h-4 text-slate-600" /> ANDROID (CLIENT)</span>
          </div>

          <div className="relative border border-slate-800 rounded-xl p-6 bg-slate-950/55 grid grid-cols-11 gap-2 items-center">
            
            {/* Windows side (Cols 1-4) */}
            <div className="col-span-4 space-y-2.5">
              <button
                onClick={() => setActiveId('iddcx')}
                className={`w-full p-3 text-left rounded-lg border transition-all text-xs font-sans shadow-sm cursor-pointer ${
                  activeId === 'iddcx'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                }`}
              >
                <div className="font-bold flex items-center gap-1.5 font-mono text-[11px]">
                  <span className={`w-1.5 h-1.5 rounded-full ${activeId === 'iddcx' ? 'bg-cyan-400 animate-pulse' : 'bg-slate-600'}`}></span>
                  01_IndirectDriver
                </div>
                <div className="text-[10px] text-slate-505 text-slate-500 mt-1 leading-tight">UMDF 2.0 virtual monitor plane</div>
              </button>

              <button
                onClick={() => setActiveId('dxgi')}
                className={`w-full p-3 text-left rounded-lg border transition-all text-xs font-sans shadow-sm cursor-pointer ${
                  activeId === 'dxgi'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                }`}
              >
                <div className="font-bold flex items-center gap-1.5 font-mono text-[11px]">
                  <span className={`w-1.5 h-1.5 rounded-full ${activeId === 'dxgi' ? 'bg-cyan-400 animate-pulse' : 'bg-slate-600'}`}></span>
                  02_DesktopCapture
                </div>
                <div className="text-[10px] text-slate-505 text-slate-500 mt-1 leading-tight">Zero-copy hardware frame pull</div>
              </button>

              <button
                onClick={() => setActiveId('nvenc')}
                className={`w-full p-3 text-left rounded-lg border transition-all text-xs font-sans shadow-sm cursor-pointer ${
                  activeId === 'nvenc'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                }`}
              >
                <div className="font-bold flex items-center gap-1.5 font-mono text-[11px]">
                  <span className={`w-1.5 h-1.5 rounded-full ${activeId === 'nvenc' ? 'bg-cyan-400 animate-pulse' : 'bg-slate-600'}`}></span>
                  03_HardwareEncoder
                </div>
                <div className="text-[10px] text-slate-505 text-slate-500 mt-1 leading-tight">AVC / HEVC low-latency profile</div>
              </button>

              <button
                onClick={() => setActiveId('daemon')}
                className={`w-full p-3 text-left rounded-lg border transition-all text-xs font-sans shadow-sm cursor-pointer ${
                  activeId === 'daemon'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                }`}
              >
                <div className="font-bold flex items-center gap-1.5 font-mono text-[11px]">
                  <span className={`w-1.5 h-1.5 rounded-full ${activeId === 'daemon' ? 'bg-cyan-400 animate-pulse' : 'bg-slate-600'}`}></span>
                  04_WinDaemonApp
                </div>
                <div className="text-[10px] text-slate-505 text-slate-500 mt-1 leading-tight">Thread control & USB bulk pipe</div>
              </button>
            </div>

            {/* Connecting physical transport channel (Cols 5-7) */}
            <div className="col-span-3 flex flex-col justify-center items-center px-1">
              <button
                onClick={() => setActiveId('usb_phy')}
                className={`w-full p-4 text-center rounded-lg border transition-all text-xs font-mono shadow-sm flex flex-col items-center justify-center gap-2 cursor-pointer ${
                  activeId === 'usb_phy'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-330 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-405 text-slate-400 hover:text-slate-200'
                }`}
              >
                <Settings className="w-4 h-4 text-cyan-400 animate-spin-slow" />
                <div>
                  <span className="font-bold block text-[10px]/none tracking-tight">USB BUS LINK</span>
                  <span className="text-[8px] text-slate-500 block mt-1 tracking-wider uppercase font-semibold">ADB & WinUSB</span>
                </div>
              </button>
              
              {/* Backflow Touch controls path */}
              <div className="w-full mt-3 flex items-center justify-between text-[9px] text-slate-450 font-semibold px-2 font-mono">
                <span className="text-emerald-500 font-bold">&larr; Touch</span>
                <span className="text-cyan-400 font-bold">Vid &rarr;</span>
              </div>
            </div>

            {/* Android side (Cols 8-11) */}
            <div className="col-span-4 space-y-3">
              <button
                onClick={() => setActiveId('rec_socket')}
                className={`w-full p-3 text-left rounded-lg border transition-all text-xs font-sans shadow-sm cursor-pointer ${
                  activeId === 'rec_socket'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                }`}
              >
                <div className="font-bold flex items-center gap-1.5 font-mono text-[11px]">
                  <span className={`w-1.5 h-1.5 rounded-full ${activeId === 'rec_socket' ? 'bg-cyan-400' : 'bg-slate-600'}`}></span>
                  05_BulkReceiver
                </div>
                <div className="text-[10px] text-slate-505 text-slate-500 mt-1 leading-tight">Direct NIO Bulk Stream subscriber</div>
              </button>

              <button
                onClick={() => setActiveId('mediacodec')}
                className={`w-full p-3 text-left rounded-lg border transition-all text-xs font-sans shadow-sm cursor-pointer ${
                  activeId === 'mediacodec'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                }`}
              >
                <div className="font-bold flex items-center gap-1.5 font-mono text-[11px]">
                  <span className={`w-1.5 h-1.5 rounded-full ${activeId === 'mediacodec' ? 'bg-cyan-400 animate-pulse' : 'bg-slate-600'}`}></span>
                  06_MediaCodec
                </div>
                <div className="text-[10px] text-slate-505 text-slate-500 mt-1 leading-tight">Direct SoC decoding pipelines</div>
              </button>

              <button
                onClick={() => setActiveId('gles_render')}
                className={`w-full p-3 text-left rounded-lg border transition-all text-xs font-sans shadow-sm cursor-pointer ${
                  activeId === 'gles_render'
                    ? 'border-cyan-500 bg-cyan-950/35 text-cyan-300 ring-1 ring-cyan-500/10'
                    : 'border-slate-850 bg-slate-900 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                }`}
              >
                <div className="font-bold flex items-center gap-1.5 font-mono text-[11px]">
                  <span className={`w-1.5 h-1.5 rounded-full ${activeId === 'gles_render' ? 'bg-cyan-400' : 'bg-slate-600'}`}></span>
                  07_GLComposition
                </div>
                <div className="text-[10px] text-slate-505 text-slate-500 mt-1 leading-tight">Dual-buffer texture render loop</div>
              </button>
            </div>
            
          </div>
        </div>

        {/* Dynamic Detail Viewer Sidebar (5 Columns) */}
        <div className="lg:col-span-5 bg-slate-950 rounded-xl border border-slate-800 p-5 flex flex-col justify-between">
          <div>
            <div className="flex items-center gap-2 mb-3.5">
              <span className="text-[9px] font-mono font-bold uppercase tracking-widest bg-slate-900 border border-slate-800 text-cyan-400 px-2 py-0.5 rounded leading-none">
                SYS_MODULE
              </span>
              <span className="text-[10px] font-mono text-slate-500">TAG: {components[activeId].id}</span>
            </div>

            <h4 className="text-base font-bold text-white font-sans leading-tight">
              {components[activeId].title}
            </h4>
            <p className="text-[11px] text-cyan-400 font-mono mt-1 font-semibold uppercase">
              // {components[activeId].sub}
            </p>

            <p className="text-xs text-slate-400 leading-relaxed mt-3 pt-3 border-t border-slate-900/60">
              {components[activeId].desc}
            </p>

            {/* Targeted APIs list */}
            {components[activeId].apis.length > 0 && (
              <div className="mt-4">
                <span className="text-[9px] font-bold text-slate-450 text-slate-400 block uppercase mb-1.5 tracking-wider font-mono">Crucial OS Handles / Entry Points</span>
                <div className="space-y-1">
                  {components[activeId].apis.map((api, idx) => (
                    <code key={idx} className="block text-[10px] font-mono bg-slate-900 border border-slate-800 px-2.5 py-1 rounded text-cyan-330 text-cyan-300 truncate">
                      {api}
                    </code>
                  ))}
                </div>
              </div>
            )}

            {/* Tech keywords tags */}
            <div className="mt-4 flex flex-wrap gap-1">
              {components[activeId].techKeywords.map((tag) => (
                <span key={tag} className="text-[9px] font-mono font-semibold px-2 py-0.5 bg-slate-900 border border-slate-850 text-slate-400 rounded">
                  #{tag.toUpperCase()}
                </span>
              ))}
            </div>
          </div>

          <div className="mt-5 pt-4 border-t border-slate-900 bg-slate-900/40 p-3 rounded-lg border border-slate-800">
            <span className="text-[10px] font-bold text-amber-400 uppercase flex items-center gap-1 mb-1 font-mono">
              <ShieldAlert className="w-3.5 h-3.5 flex-shrink-0 text-amber-400" />
              ARCH_LATENCY_ENGINEERING_TIP:
            </span>
            <p className="text-[11px] text-slate-400 leading-normal">
              {components[activeId].tips}
            </p>
          </div>

        </div>
      </div>
    </div>
  );
}
