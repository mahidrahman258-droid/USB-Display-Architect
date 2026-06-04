import { useState } from 'react';
import { BookOpen, Sliders, ShieldCheck, Milestone, Cpu, Compass, Check, Laptop, Smartphone } from 'lucide-react';

export default function SpecsDoc() {
  const [activeTab, setActiveTab] = useState<string>('components');

  const tabs = [
    { id: 'components', label: 'Host & Client Engine', icon: Cpu },
    { id: 'usb', label: 'USB Protocol Matrix', icon: Sliders },
    { id: 'opt', label: 'Latency & Security', icon: ShieldCheck },
    { id: 'mvp', label: '30-Day MVP Plan', icon: Milestone },
    { id: 'comp', label: 'Competitor Analysis', icon: Compass },
  ];

  return (
    <div className="bg-slate-900 rounded-xl border border-slate-800 shadow-xl p-6">
      
      {/* Tab bar header */}
      <div className="flex items-center gap-2.5 mb-6 border-b border-slate-950 pb-4">
        <span className="p-2 bg-slate-950 text-cyan-400 rounded-lg border border-cyan-900/60">
          <BookOpen className="w-5 h-5" />
        </span>
        <div>
          <h3 className="text-base font-bold text-white uppercase tracking-wide font-mono">
            // DEEP_DIVE_TECHNICAL_SPECIFICATION_MANUAL
          </h3>
          <p className="text-xs text-slate-400 mt-1">
            Production-grade systems architecture blueprints covering driver frameworks, low-level serializing, and latency benchmarks.
          </p>
        </div>
      </div>

      {/* Tabs navigation */}
      <div className="flex flex-wrap gap-1.5 mb-6 bg-slate-955 bg-slate-950 p-1.5 rounded-lg border border-slate-850">
        {tabs.map((tab) => {
          const IconComp = tab.icon;
          return (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`flex items-center gap-2 px-4 py-2 text-xs font-mono font-bold rounded cursor-pointer transition-all ${
                activeTab === tab.id
                  ? 'bg-slate-900 text-cyan-300 border border-cyan-900/50 shadow-md'
                  : 'text-slate-400 hover:text-slate-205 hover:text-cyan-400'
              }`}
            >
              <IconComp className="w-3.5 h-3.5" />
              <span>{tab.label.toUpperCase()}</span>
            </button>
          );
        })}
      </div>

      {/* Tab Panels */}
      <div className="space-y-6">
        
        {/* TAB 1: Host & Client Component Deep-Dive */}
        {activeTab === 'components' && (
          <div className="space-y-6 animate-fadeIn">
            
            {/* Laptop-Side */}
            <div>
              <h4 className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-3.5 flex items-center gap-2 font-mono border-b border-slate-950 pb-1.5">
                <span className="px-1.5 py-0.5 bg-slate-950 text-cyan-400 border border-cyan-900/65 rounded text-[9px] font-mono font-bold">WINDOWS_OS</span>
                Laptop-Side Engineering Layer
              </h4>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-5 leading-relaxed text-xs text-slate-400">
                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850">
                  <h5 className="font-bold text-white mb-1.5 font-mono text-cyan-405 text-cyan-400">// 1. INDIRECT DISPLAY DRIVER (IDD) LAYER</h5>
                  <p className="mb-2">
                    Creating virtual monitors on modern Windows systems requires building a custom <strong>Indirect Display Driver (IDD)</strong> using the Windows Driver Kit (WDK) <strong>UMDF 2.0 Class Extension Model (IddCx)</strong>. This framework completely replaces obsolete mirror drivers. 
                  </p>
                  <p className="font-bold text-slate-350 mt-3 font-mono text-[10px]">DRIVER INITIALIZATION SEQUENCE:</p>
                  <ul className="list-disc pl-4 space-y-1 mt-1 text-slate-500">
                    <li>Load and construct the software-only GDI adapter context.</li>
                    <li>Define preferred viewport capabilities (e.g. 1920×1080@60Hz) during device startup checks.</li>
                    <li>Call <code className="font-mono bg-slate-900 px-1.5 py-0.5 rounded text-cyan-350 text-cyan-300 text-[10px] border border-cyan-900/30">IddCxMonitorArrival</code> inside the user-mode driver entry loop.</li>
                    <li>Windows treats the system as a real display, creating a desktop frame buffer.</li>
                  </ul>
                </div>

                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 flex flex-col justify-between">
                  <div>
                    <h5 className="font-bold text-white mb-1.5 font-mono text-cyan-400">// 2. DXGI DIRECT MEMORY FRAME RETRIEVAL</h5>
                    <p className="mb-2">
                      In the usermode streaming coordinator app, retrieve screen states using the <strong>DXGI Desktop Duplication API</strong>. Duplicate frames directly inside the active rendering GPU pipeline:
                    </p>
                    <ul className="list-disc pl-4 space-y-1 text-slate-500">
                      <li>Call <code className="font-mono bg-slate-900 px-1.5 py-0.5 rounded text-cyan-300 text-[10px] border border-cyan-900/30">AcquireNextFrame</code> to fetch a pointer to a Direct3D 11 texture.</li>
                      <li>Extract the texture metadata including cursor coordinates and damaged screen sub-rectangles (to optimize incremental refresh maps).</li>
                    </ul>
                  </div>
                  <div className="mt-4 bg-cyan-950/20 p-2.5 rounded border border-cyan-900/40 text-[11px] text-cyan-400">
                    <strong>Zero-Copy Bridge Note:</strong> Raw frame texture objects inside GPU memory are referenced directly as shader resources for the video encoder block without fetching pixels back to CPU RAM.
                  </div>
                </div>

                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 md:col-span-2">
                  <h5 className="font-bold text-white mb-1.5 font-mono text-cyan-400">// 3. HARDWARE ACCELERATED VIDEO ENCODER PLANE (NVENC / AMF / QSV)</h5>
                  <p className="text-slate-400 mb-2">
                    By binding to <strong>NVIDIA NVENC</strong> or <strong>Intel QuickSync technology</strong>, the application compresses raw 2K/4K display textures straight in hardware.
                  </p>
                  <p className="mt-3 font-semibold text-slate-350 font-mono text-[10px]">LOW-LATENCY HARDWARE PRESETS (CRUCIAL FOR GRAPHICS EXTENSION):</p>
                  <div className="grid grid-cols-1 sm:grid-cols-3 gap-3 mt-1.5 font-mono text-[11px]">
                    <div className="bg-slate-900 border border-slate-800 p-2.5 rounded">
                      <span className="font-bold block border-b border-slate-950 pb-1 mb-1 text-cyan-400">Preset Configuration</span>
                      - Config: Low-Latency HQ<br />
                      - B-Frames: 0 (Strict)<br />
                      - CABAC: Active for H.264 Main
                    </div>
                    <div className="bg-slate-900 border border-slate-800 p-2.5 rounded">
                      <span className="font-bold block border-b border-slate-950 pb-1 mb-1 text-cyan-400">Rate Control Params</span>
                      - Type: Constant Bitrate (CBR)<br />
                      - Strict Frame Size: On<br />
                      - Intra Refresh: Enabled
                    </div>
                    <div className="bg-slate-900 border border-slate-800 p-2.5 rounded">
                      <span className="font-bold block border-b border-slate-950 pb-1 mb-1 text-cyan-400">Slicing & Chunking</span>
                      - Slices per Frame: 4-8<br />
                      - Slice-based Handoff: Active<br />
                      - Maximum Packet: MTU size compliant
                    </div>
                  </div>
                </div>
              </div>
            </div>

            {/* Android-Side */}
            <div>
              <h4 className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-3.5 flex items-center gap-2 font-mono border-b border-slate-950 pb-1.5">
                <span className="px-1.5 py-0.5 bg-slate-950 text-emerald-400 border border-emerald-900/60 rounded text-[9px] font-mono font-bold">ANDROID_OS</span>
                Android Phone-Side Decoding & Renderer Layer
              </h4>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-5 leading-relaxed text-xs text-slate-400">
                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850">
                  <h5 className="font-bold text-white mb-1.5 font-mono text-cyan-400">// 1. HARDWARE DECODE VIA MEDIACODEC API</h5>
                  <p className="mb-2">
                    Decompress raw compressed bytes inside Phone Silicon blocks via Android's low-level <strong>MediaCodec API</strong>. Software decoders (like soft-FFmpeg) are completely unusable due to severe CPU overhead.
                  </p>
                  <p className="font-semibold text-slate-350 font-mono text-[10px]">MEDIACODEC IMPLEMENTATION SUMMARY:</p>
                  <ul className="list-disc pl-4 space-y-1 mt-1 text-slate-500">
                    <li>Create an input queue and extract free byte buffers: <code className="font-mono bg-slate-900 px-1 py-0.5 rounded text-emerald-400 border border-emerald-950">dequeueInputBuffer()</code></li>
                    <li>Directly place byte fragments into native NIO buffers inside Kotlin lines.</li>
                    <li>Submit compressed frame packets instantly via <code className="font-mono bg-slate-900 px-1 py-0.5 rounded text-emerald-400 border border-emerald-950">queueInputBuffer()</code>.</li>
                  </ul>
                </div>

                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 flex flex-col justify-between">
                  <div>
                    <h5 className="font-bold text-white mb-1.5 font-mono text-cyan-400">// 2. NATIVE OPENGL ES DIRECT-TO-SCREEN COMPOSITING</h5>
                    <p className="mb-2">
                      To completely bypass cross-buffer copies inside Android graphic systems:
                    </p>
                    <ul className="list-disc pl-4 space-y-1 text-slate-500">
                      <li>Create an <strong>OpenGL ES SurfaceTexture</strong> object inside a background GL context.</li>
                      <li>Register MediaCodec with this Surface: <code className="font-mono bg-slate-900 px-1.5 py-0.5 rounded text-emerald-400 border border-emerald-950">codec.configure(..., surface, null, 0)</code>.</li>
                      <li>Call <code className="font-mono bg-slate-900 px-1.5 py-0.5 rounded text-cyan-300 border border-cyan-900/30">surfaceTexture.updateTexImage()</code> to paint instantly on the live frame layer.</li>
                    </ul>
                  </div>
                  <div className="mt-4 bg-emerald-950/20 p-2.5 rounded border border-emerald-900/40 text-[11px] text-emerald-405 text-emerald-450 text-emerald-400">
                    <strong>Double Buffer Optimization:</strong> Set swap buffers to disable classic EGL swap sync roadblocks, skipping native display VSync blocking waits.
                  </div>
                </div>
              </div>
            </div>

          </div>
        )}

        {/* TAB 2: USB Transmission Channel Comparison */}
        {activeTab === 'usb' && (
          <div className="space-y-5 animate-fadeIn">
            <h4 className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 font-mono border-b border-slate-950 pb-1.5 flex items-center justify-between">
              <span>USB Transport Interface Profiles Matrix</span>
              <span className="text-[10px] text-slate-500 lowercase font-mono">comparison_table.csv</span>
            </h4>
            
            <p className="text-xs text-slate-400 leading-relaxed mb-1">
              Low-latency display mirroring depends heavily on the core physical pipe protocol. The table below compares the three primary mechanisms used to transport screen packets over USB.
            </p>

            <div className="overflow-x-auto border border-slate-800 rounded-lg">
              <table className="w-full text-left border-collapse text-xs">
                <thead>
                  <tr className="bg-slate-950 text-slate-300 font-bold border-b border-slate-800 font-mono">
                    <th className="p-3">Requirement</th>
                    <th className="p-3 text-cyan-405 text-cyan-450 text-cyan-400">Option A: ADB Local Sockets</th>
                    <th className="p-3 text-emerald-400">Option B: Android Accessory (AOA)</th>
                    <th className="p-3 text-amber-400">Option C: Direct Custom WinUSB</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-900 text-slate-400 bg-slate-950/40">
                  <tr>
                    <td className="p-3 font-semibold bg-slate-950/80 font-mono text-[10px]">Underlying Protocol</td>
                    <td className="p-3">ADB port forwarding (<code className="font-mono bg-slate-900 text-[10px] text-cyan-400 border border-slate-800 px-1 py-0.5 rounded">adb reverse tcp:3000</code>)</td>
                    <td className="p-3">Standard Bulk transfers via Android Open Accessory 2.0</td>
                    <td className="p-3">Microsoft WinUSB direct interface descriptors</td>
                  </tr>
                  <tr>
                    <td className="p-3 font-semibold bg-slate-950/80 font-mono text-[10px]">Transport Latency</td>
                    <td className="p-3 text-orange-405 text-orange-400 font-semibold font-mono">Slow (3.5ms–7ms) - Socket buffers</td>
                    <td className="p-3 text-emerald-400 font-semibold font-mono">Very Low (1.2ms–2ms) - Direct OS kernel pipe</td>
                    <td className="p-3 text-cyan-400 font-semibold font-mono">Optimal (&lt; 0.5ms) - Zero-overhead direct device bus writes</td>
                  </tr>
                  <tr>
                    <td className="p-3 font-semibold bg-slate-950/80 font-mono text-[10px]">User Friction</td>
                    <td className="p-3 text-red-400">High - Requires developer USB debugging turned on on the device</td>
                    <td className="p-3 text-emerald-400 font-bold">Zero Friction - Companion App auto-launches on USB plug-in</td>
                    <td className="p-3 text-amber-500">Moderate - PC requires signed INF driver installation</td>
                  </tr>
                  <tr>
                    <td className="p-3 font-semibold bg-slate-950/80 font-mono text-[10px]">Driver Signature</td>
                    <td className="p-3">Not Needed (uses standard loopback)</td>
                    <td className="p-3">Not Needed (uses standard bus layer)</td>
                    <td className="p-3 text-red-400">Strict - Requires certified INF catalog certificates</td>
                  </tr>
                  <tr>
                    <td className="p-3 font-semibold bg-slate-950/80 font-mono text-[10px]">Bandwidth Max</td>
                    <td className="p-3">ADB limited socket loops (~180 Mbps max)</td>
                    <td className="p-3">USB 2.0 Bulk Endpoint limit (~320 Mbps real)</td>
                    <td className="p-3 text-cyan-400 font-bold">Full USB 3.0 speed caps up to 5 Gbps</td>
                  </tr>
                  <tr className="bg-slate-900/60 font-mono text-[11px]">
                    <td className="p-3 font-semibold bg-slate-950/80 text-cyan-400">Recommendation</td>
                    <td className="p-3">Perfect for immediate development sandboxing.</td>
                    <td className="p-3 font-bold text-emerald-400 font-mono">The best balance for mass production and flawless user UX.</td>
                    <td className="p-3 text-slate-300">Best choice for ultimate high-performance 4K workstation displays.</td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>
        )}

        {/* TAB 3: Latency Secrets and Security */}
        {activeTab === 'opt' && (
          <div className="space-y-6 animate-fadeIn">
            
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div className="bg-slate-950 p-5 rounded-lg border border-slate-850">
                <h4 className="text-xs font-bold uppercase tracking-wider text-cyan-405 text-cyan-400 mb-3.5 flex items-center gap-1.5 font-mono">
                  <span className="w-1.5 h-1.5 rounded-full bg-cyan-400"></span>
                  // PIPELINE_LATENCY_OPTIMIZATIONS
                </h4>
                <ul className="space-y-3 text-xs leading-relaxed text-slate-405 text-slate-400">
                  <li className="flex gap-2">
                    <Check className="w-4 h-4 text-cyan-400 flex-shrink-0 mt-0.5" />
                    <div>
                      <strong>Intra-Refresh (Slice-based) Encoding:</strong> Spread keyframe decoding overhead equally across all frames. Spread slice buffers to avoid sudden network transmission bottlenecks.
                    </div>
                  </li>
                  <li className="flex gap-2">
                    <Check className="w-4 h-4 text-cyan-405 text-cyan-400 flex-shrink-0 mt-0.5" />
                    <div>
                      <strong>Strictly Skip B-Frames:</strong> Ensure bidirectional frames are completely bypassed, eliminating encoder queuing delays and dropping capture lag by 30-55ms.
                    </div>
                  </li>
                  <li className="flex gap-2">
                    <Check className="w-4 h-4 text-cyan-400 flex-shrink-0 mt-0.5" />
                    <div>
                      <strong>Zero GC Allocations on NIO Receiver:</strong> Cache direct NIO byte structures inside native loops, eliminating Java runtime VM garbage sweeps.
                    </div>
                  </li>
                </ul>
              </div>

              {/* Security Profiling */}
              <div className="bg-slate-950 p-5 rounded-lg border border-slate-850">
                <h4 className="text-xs font-bold uppercase tracking-wider text-cyan-400 mb-3.5 flex items-center gap-1.5 font-mono">
                  <span className="w-1.5 h-1.5 rounded-full bg-cyan-400"></span>
                  // SYSTEM_SECURITY_CONTROLS
                </h4>
                <ul className="space-y-3 text-xs leading-relaxed text-slate-400">
                  <li className="flex gap-2">
                    <Check className="w-4 h-4 text-cyan-400 flex-shrink-0 mt-0.5" />
                    <div>
                      <strong>INF Driver Catalog Verification:</strong> Require Windows Hardware Certification tests to avoid raw buffer scraping and kernel memory exploits.
                    </div>
                  </li>
                  <li className="flex gap-2">
                    <Check className="w-4 h-4 text-cyan-400 flex-shrink-0 mt-0.5" />
                    <div>
                      <strong>Host-to-Client Public Key Handshakes:</strong> Prevent unauthorized devices from mirroring desktop data, validating inputs via stored signatures.
                    </div>
                  </li>
                  <li className="flex gap-2">
                    <Check className="w-4 h-4 text-cyan-400 flex-shrink-0 mt-0.5" />
                    <div>
                      <strong>Pointer Backflow Bounds Checks:</strong> Enforce absolute viewport coordinates matching on virtual inputs to block cross-script triggers.
                    </div>
                  </li>
                </ul>
              </div>
            </div>

          </div>
        )}

        {/* TAB 4: 30-Day MVP Road Map */}
        {activeTab === 'mvp' && (
          <div className="space-y-5 animate-fadeIn">
            <h4 className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 font-mono border-b border-slate-950 pb-1.5">
              30-Day MVP Development & Milestones Roadmap
            </h4>
            
            <p className="text-xs text-slate-400 leading-relaxed mb-3">
              Incremental, realistic timeline detailing steps to build a fully validated USB screen extension system from scratch.
            </p>

            <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mt-4">
              
              {/* Week 1 */}
              <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 flex flex-col justify-between">
                <div>
                  <span className="text-[9px] font-bold text-cyan-400 font-mono tracking-wider block mb-1">DAYS 1 - 7</span>
                  <h5 className="font-bold text-white text-xs leading-tight mb-2 font-mono uppercase tracking-wide">// Driver & Display Creation</h5>
                  <p className="text-[11px] text-slate-450 text-slate-400 leading-relaxed">
                    Set up the virtual monitor Indirect Driver using Windows WDK 10. Ensure OS Display Settings registers and creates second virtual screen space.
                  </p>
                </div>
                <div className="text-[10px] font-mono text-slate-500 mt-3 pt-2 border-t border-slate-900">
                  TARGET: virtual adapter compiles, registers on Windows panel.
                </div>
              </div>

              {/* Week 2 */}
              <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 flex flex-col justify-between">
                <div>
                  <span className="text-[9px] font-bold text-cyan-400 font-mono tracking-wider block mb-1">DAYS 8 - 15</span>
                  <h5 className="font-bold text-white text-xs leading-tight mb-2 font-mono uppercase tracking-wide">// D3D DXGI Capture</h5>
                  <p className="text-[11px] text-slate-400 leading-relaxed">
                    Implement the zero-copy Direct3D texture grabber loop. Map acquired DXGI surface handles directly to hardware NVENC encoders.
                  </p>
                </div>
                <div className="text-[10px] font-mono text-slate-500 mt-3 pt-2 border-t border-slate-900">
                  TARGET: Screen bytes encoded continuously to local AVC logs.
                </div>
              </div>

              {/* Week 3 */}
              <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 flex flex-col justify-between">
                <div>
                  <span className="text-[9px] font-bold text-cyan-400 font-mono tracking-wider block mb-1">DAYS 16 - 22</span>
                  <h5 className="font-bold text-white text-xs leading-tight mb-2 font-mono uppercase tracking-wide">// USB AOA Conduit Setup</h5>
                  <p className="text-[11px] text-slate-405 text-slate-400 leading-relaxed">
                    Build Kotlin client companion receivers using android.hardware.usb queues. Route bytes into MediaCodec hardware decoders.
                  </p>
                </div>
                <div className="text-[10px] font-mono text-slate-500 mt-3 pt-2 border-t border-slate-900">
                  TARGET: Phone receives bulk blocks, decoding frames.
                </div>
              </div>

              {/* Week 4 */}
              <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 flex flex-col justify-between">
                <div>
                  <span className="text-[9px] font-bold text-cyan-400 font-mono tracking-wider block mb-1">DAYS 23 - 30</span>
                  <h5 className="font-bold text-white text-xs leading-tight mb-2 font-mono uppercase tracking-wide">// Composite & Input Backflow</h5>
                  <p className="text-[11px] text-slate-400 leading-relaxed">
                    Complete Android OpenGL ES SurfaceTexture composition frame paints, adding screen touch events dispatcher back to Windows.
                  </p>
                </div>
                <div className="text-[10px] font-mono text-slate-500 mt-3 pt-2 border-t border-slate-900">
                  TARGET: Mirroring works seamlessly at 1080p@60FPS, glass lag &lt; 25ms.
                </div>
              </div>

            </div>
          </div>
        )}

        {/* TAB 5: Competitor Analysis */}
        {activeTab === 'comp' && (
          <div className="space-y-6 animate-fadeIn text-xs text-slate-400 font-sans">
            <div>
              <h4 className="text-xs font-bold text-slate-400 uppercase tracking-wider mb-2 font-mono border-b border-slate-950 pb-1.5">
                Market Mirroring Engines & Conceptual Designs
              </h4>
              <p className="mb-4 text-slate-400 leading-relaxed">
                Analyzing standard production paradigms used by industry market leaders clarifies structural decisions.
              </p>

              <div className="grid grid-cols-1 md:grid-cols-3 gap-5">
                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850">
                  <h5 className="font-bold text-white mb-2 border-b border-slate-900 pb-1.5 flex items-center justify-between font-mono text-[11px]">
                    <span>1. DUET DISPLAY</span>
                    <span className="text-[9px] bg-slate-900 border border-slate-800 px-1.5 py-0.5 rounded font-mono text-cyan-400">COMMERCIAL STD</span>
                  </h5>
                  <p className="leading-relaxed">
                    <strong>Commercial Framework:</strong> Duet Display utilizes an Indirect Display Driver surface, performing H.264 video encoding straight on Intel/AMD/Nvidia GPUs. The compressed packets are dispatched via proprietary USB multiplexer layers directly to iOS or Android companion app sockets over WinUSB pipelines.
                  </p>
                </div>

                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850">
                  <h5 className="font-bold text-white mb-2 border-b border-slate-900 pb-1.5 flex items-center justify-between font-mono text-[11px]">
                    <span>2. SPLASHTOP WIRED</span>
                    <span className="text-[9px] bg-slate-900 border border-slate-800 px-1.5 py-0.5 rounded font-mono text-cyan-450 text-cyan-400">ADB SOCKET forward</span>
                  </h5>
                  <p className="leading-relaxed">
                    <strong>Socket Loopbacks:</strong> Splashtop bypasses strict signed kernel driver registrations by loading standard background daemons. They initiate local virtual TCP ports and loopback the video frames through regular ADB reverse bridges. This relies on native Android loopback sockets, adding moderate CPU parsing lag.
                  </p>
                </div>

                <div className="bg-slate-950 p-4 rounded-lg border border-slate-850 flex flex-col justify-between">
                  <div>
                    <h5 className="font-bold text-white mb-2 border-b border-slate-900 pb-1.5 flex items-center justify-between font-mono text-[11px]">
                      <span>3. SCRCPY</span>
                      <span className="text-[9px] bg-slate-900 border border-slate-800 px-1.5 py-0.5 rounded font-mono text-cyan-400">OPEN ENGINE</span>
                    </h5>
                    <p className="leading-relaxed">
                      <strong>Reverse Screencasting:</strong> Scrcpy executes a zero-install on Android by loading a client JAR into Android's native system class directory <code className="font-mono bg-slate-900 text-[10px] text-cyan-400 border border-slate-850 px-1 rounded">app_process</code> under adb developer shells. This binary hooks into private compositor layers to transcode frames back to standard PC decoders.
                    </p>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

      </div>
    </div>
  );
}
