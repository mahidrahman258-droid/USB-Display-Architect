import { useState } from 'react';
import { Code, Copy, Check, Terminal, Laptop, Smartphone, FileText } from 'lucide-react';

export default function CodeSnippets() {
  const [copied, setCopied] = useState<string | null>(null);
  const [activeSnippet, setActiveSnippet] = useState<string>('dxgi_cpp');

  const copyToClipboard = (text: string, id: string) => {
    navigator.clipboard.writeText(text);
    setCopied(id);
    setTimeout(() => setCopied(null), 2000);
  };

  const snippets = {
    dxgi_cpp: {
      id: 'dxgi_cpp',
      name: 'Windows DXGI Capture Loop (C++)',
      lang: 'cpp',
      platform: 'Windows Desktop duplication API',
      code: `// High-Performance DXGI Desktop Duplication Frame Acquisition (C++)
#include <d3d11.h>
#include <dxgi1_5.h>
#include <iostream>

class DxgiScreenGrabber {
private:
    ID3D11Device*           m_pd3dDevice = nullptr;
    ID3D11DeviceContext*    m_pImmediateContext = nullptr;
    IDXGIOutputDuplication* m_pDeskDupl = nullptr;

public:
    HRESULT Initialize(int monitorIndex) {
        // 1. Initialize Direct3D 11 Context
        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 
                                       D3D11_CREATE_DEVICE_VIDEO_SUPPORT, nullptr, 0, 
                                       D3D11_SDK_VERSION, &m_pd3dDevice, &featureLevel, &m_pImmediateContext);
        if (FAILED(hr)) return hr;

        // 2. Fetch Dxgi Device Interface & Adaptor Output
        IDXGIDevice* pDxgiDevice = nullptr;
        m_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
        
        IDXGIAdapter* pDxgiAdapter = nullptr;
        pDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pDxgiAdapter);
        pDxgiDevice->Release();

        IDXGIOutput* pDxgiOutput = nullptr;
        pDxgiAdapter->GetOutput(monitorIndex, &pDxgiOutput);
        pDxgiAdapter->Release();

        IDXGIOutput5* pDxgiOutput5 = nullptr;
        pDxgiOutput->QueryInterface(__uuidof(IDXGIOutput5), (void**)&pDxgiOutput5);
        pDxgiOutput->Release();

        // 3. Register Desktop Duplication with hardware texture support (NV12 favored)
        DXGI_FORMAT formats[] = { DXGI_FORMAT_NV12, DXGI_FORMAT_B8G8R8A8_UNORM };
        hr = pDxgiOutput5->DuplicateOutput1(m_pd3dDevice, 0, ARRAYSIZE(formats), formats, &m_pDeskDupl);
        pDxgiOutput5->Release();
        
        return hr;
    }

    HRESULT AcquireFrame(ID3D11Texture2D** ppFrameTexture, DXGI_OUTDUPL_FRAME_INFO* pFrameInfo) {
        IDXGIResource* pDesktopResource = nullptr;
        HRESULT hr = m_pDeskDupl->AcquireNextFrame(10, pFrameInfo, &pDesktopResource); // 10ms wait max
        if (FAILED(hr)) return hr;

        // Fetch D3D11 Texture representation directly from GPU VRAM
        hr = pDesktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)ppFrameTexture);
        pDesktopResource->Release();
        
        return hr;
    }

    void ReleaseFrame() {
        if (m_pDeskDupl) {
            m_pDeskDupl->ReleaseFrame();
        }
    }
};`
    },
    idd_cpp: {
      id: 'idd_cpp',
      name: 'Windows IDD Driver Setup (C++)',
      lang: 'cpp',
      platform: 'UMDF 2.0 - Indirect Display Driver initialization skeleton',
      code: `// Indirect Display (UMDF 2.0 WDDM Driver Setup skeleton)
#include <windows.h>
#include <iddcx.h>

extern "C" DRIVER_INITIALIZE DriverEntry;

// UMDF Driver Entry Point
HRESULT DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, IddDeviceAdd);

    // Call WDF framework layer to instantiate Driver object
    WDFDRIVER hDriver;
    NTSTATUS status = WdfDriverCreate(pDriverObject, pRegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config, &hDriver);
    return NT_SUCCESS(status) ? S_OK : E_FAIL;
}

// System triggers IddDeviceAdd when matching WDF Hardware node identified
NTSTATUS IddDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
    // Standard Indirect Display device initialization configs
    IDD_CX_CLIENT_CONFIG idleConfig;
    IDD_CX_CLIENT_CONFIG_INIT(&idleConfig);
    idleConfig.EvtIddCxAdapterInitFinished = EvtIddAdapterInitFinished;
    idleConfig.EvtIddCxMonitorArrival = EvtIddMonitorArrival;

    NTSTATUS status = IddCxDeviceInitConfig(DeviceInit, &idleConfig);
    if (!NT_SUCCESS(status)) return status;

    WDFDEVICE hDevice;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = WdfDeviceCreate(&DeviceInit, &attributes, &hDevice);
    
    return status;
}

NTSTATUS EvtIddMonitorArrival(IDD_CX_MONITOR MonitorObject) {
    // 1. Inform display manager that new virtual screen has logged on
    IDARG_OUT_MONITOR_CREATION_INFO createInfo = {};
    IddCxMonitorCreate(MonitorObject, &createInfo);
    
    // 2. Load display profiles (supports 1085p & 2K screen properties)
    IDD_MONITOR_MODE preferredModes[1] = {
        { sizeof(IDD_MONITOR_MODE), 1920, 1080, { 60, 1 }, IDD_MONITOR_MODE_FLAGS_PREFERRED }
    };
    IddCxMonitorUpdateModes(MonitorObject, 1, preferredModes);
    return STATUS_SUCCESS;
}`
    },
    android_dec: {
      id: 'android_dec',
      name: 'MediaCodec Decoder Loop (Kotlin)',
      lang: 'kotlin',
      platform: 'Android direct SoC decoding to live surface view',
      code: `// Low-Latency Android MediaCodec Hardware Subsystem Decoder (Kotlin)
import android.media.MediaCodec
import android.media.MediaFormat
import android.view.Surface
import java.nio.ByteBuffer

class HardwareVideoDecoder(private val targetSurface: Surface) : Thread() {
    private var isRunning = true
    private var decoder: MediaCodec? = null

    fun initializeDecoder(width: Int, height: Int) {
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)
        
        // Optimize formatting rules strictly for screen mirroring (No buffers)
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
        format.setInteger(MediaFormat.KEY_LATENCY, 1) // Force single frame low latency decoding
        format.setInteger(MediaFormat.KEY_PRIORITY, 0) // Peak priority SoC Threading rules

        decoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
        decoder?.configure(format, targetSurface, null, 0)
        decoder?.start()
    }

    fun submitCompressedFrame(compressedData: ByteArray, presentationTimeUs: Long, isKeyframe: Boolean) {
        val codec = decoder ?: return
        
        // 1. Fetch index representing free direct hardware memory byte buffer
        val inputIndex = codec.dequeueInputBuffer(10000) // 10ms wait timeout
        if (inputIndex >= 0) {
            val inputBuffer: ByteBuffer = codec.getInputBuffer(inputIndex)!!
            inputBuffer.clear()
            inputBuffer.put(compressedData)
            
            val flags = if (isKeyframe) MediaCodec.BUFFER_FLAG_KEY_FRAME else 0
            
            // 2. Submit bytes to SoC decoding queue
            codec.queueInputBuffer(inputIndex, 0, compressedData.size, presentationTimeUs, flags)
        }
    }

    override fun run() {
        val bufferInfo = MediaCodec.BufferInfo()
        while (isRunning) {
            val codec = decoder ?: continue
            
            // 3. Dequeue rendered buffers and paint directly to OpenGL Surface overlay
            val outputIndex = codec.dequeueOutputBuffer(bufferInfo, 5000)
            if (outputIndex >= 0) {
                // By passing true, MediaCodec routes frame instantly to OpenGL screen Surface with zero copy!
                codec.releaseOutputBuffer(outputIndex, true)
            } else if (outputIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                // Display layout updated seamlessly
            }
        }
    }

    fun shutdown() {
        isRunning = false
        decoder?.stop()
        decoder?.release()
    }
}`
    }
  };

  return (
    <div className="bg-slate-900 rounded-xl border border-slate-800 shadow-xl p-6">
      
      {/* Component Title */}
      <div className="flex items-center gap-2.5 mb-6 pb-4 border-b border-slate-950">
        <span className="p-2 bg-slate-950 text-cyan-400 rounded-lg border border-cyan-900/85">
          <Terminal className="w-5 h-5" />
        </span>
        <div>
          <h3 className="text-base font-bold text-white uppercase tracking-wide font-mono">
            // HARDWARE_STREAMING_SOURCE_BOILERPLATES
          </h3>
          <p className="text-xs text-slate-400 mt-1">
            Browse thread-secure C++ and Kotlin memory buffers used for real-time video duplication and low-overhead decode.
          </p>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
        
        {/* Left Side Selector (4 Columns) */}
        <div className="lg:col-span-4 space-y-2.5">
          <span className="text-[10px] font-mono font-bold uppercase tracking-wider text-slate-500 block mb-1">Snippet Selection:</span>
          {Object.values(snippets).map((s) => (
            <button
              key={s.id}
              onClick={() => setActiveSnippet(s.id)}
              className={`w-full p-4 rounded-lg border text-left transition-all relative cursor-pointer ${
                activeSnippet === s.id
                  ? 'border-cyan-500 bg-cyan-950/25 text-cyan-300 ring-1 ring-cyan-500/10'
                  : 'border-slate-850 hover:bg-slate-850 bg-slate-950 text-slate-400 hover:text-slate-205'
              }`}
            >
              <div className="font-bold text-xs flex items-center gap-1.5 font-mono">
                {s.id.includes('cpp') ? <Laptop className="w-3.5 h-3.5 text-cyan-500" /> : <Smartphone className="w-3.5 h-3.5 text-emerald-400" />}
                {s.name}
              </div>
              <p className="text-[10px] text-slate-500 mt-1 leading-normal font-sans">
                {s.platform}
              </p>
              {activeSnippet === s.id && (
                <span className="absolute right-3.5 top-1/2 -translate-y-1/2 w-1.5 h-1.5 rounded-full bg-cyan-400"></span>
              )}
            </button>
          ))}

          <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 mt-4">
            <span className="text-[9px] font-mono font-bold text-slate-500 block uppercase tracking-wider mb-1 flex items-center gap-1">
              <FileText className="w-3.5 h-3.5 text-cyan-400" />
              COMPILATION TARGET REFERENCE:
            </span>
            <p className="text-[11px] text-slate-450 text-slate-400 leading-relaxed font-sans">
              Compile host utility code using <strong>Visual Studio 2022 (MSVC /C++17)</strong> under Windows Driver Kit (WDK) 10. Direct Android codebases can be deployed with <strong>Android Studio Koala+</strong> targeting SDK 34+.
            </p>
          </div>
        </div>

        {/* Right Side Code Panel (8 Columns) */}
        <div className="lg:col-span-8 flex flex-col">
          <div className="flex items-center justify-between px-4 py-2.5 bg-slate-950 rounded-t-lg border border-b-0 border-slate-850">
            <span className="text-xs font-mono font-bold text-slate-400">
              // {snippets[activeSnippet as keyof typeof snippets].name}
            </span>
            <button
              onClick={() => copyToClipboard(snippets[activeSnippet as keyof typeof snippets].code, activeSnippet)}
              className="flex items-center gap-1.5 px-2.5 py-1 text-xs hover:bg-slate-900 border border-slate-800 text-slate-400 hover:text-cyan-400 hover:border-cyan-900/60 rounded cursor-pointer transition-colors"
            >
              {copied === activeSnippet ? <Check className="w-3.5 h-3.5 text-emerald-450 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
              <span className="font-mono text-[10px] uppercase font-bold">{copied === activeSnippet ? 'Copied' : 'Copy'}</span>
            </button>
          </div>
          <div className="bg-slate-950 text-slate-100 p-4 rounded-b-lg overflow-x-auto font-mono text-[11px] leading-relaxed border border-slate-850 h-[380px] shadow-inner scrollbar-thin scrollbar-thumb-slate-800 scrollbar-track-transparent">
            <pre><code>{snippets[activeSnippet as keyof typeof snippets].code}</code></pre>
          </div>
        </div>

      </div>
    </div>
  );
}
