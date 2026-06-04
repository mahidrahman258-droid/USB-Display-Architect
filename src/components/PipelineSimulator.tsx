import { useState, useEffect, useMemo } from 'react';
import { Cpu, HardDrive, CpuIcon, RefreshCw, Layers, ArrowRight, Zap, Info, Play, Pause } from 'lucide-react';
import { SimulatorParams, LatencyBreakdown } from '../types';

export default function PipelineSimulator() {
  const [params, setParams] = useState<SimulatorParams>({
    resolutionWidth: 1920,
    resolutionHeight: 1080,
    fps: 60,
    colorFormat: 'NV12',
    encoderConfig: 'H264_BASELINE',
    transportProtocol: 'CUSTOM_WINUSB',
    bitrateMbps: 18,
    cpuMode: 'Performance',
  });

  const [isPlaying, setIsPlaying] = useState<boolean>(true);
  const [pipelineProgress, setPipelineProgress] = useState<number>(0);

  // Playback timer for the animated frame pipeline
  useEffect(() => {
    if (!isPlaying) return;
    const interval = setInterval(() => {
      setPipelineProgress((prev) => (prev >= 100 ? 0 : prev + 1.2));
    }, 16);
    return () => clearInterval(interval);
  }, [isPlaying]);

  // Adjust default bitrates based on resolution, FPS and Codec
  useEffect(() => {
    let idealBitrate = 15;
    const pixels = params.resolutionWidth * params.resolutionHeight;
    const scaleFactor = (pixels / (1920 * 1080)) * (params.fps / 60);
    
    if (params.encoderConfig === 'H265_HEVC') {
      idealBitrate = Math.round(14 * scaleFactor * 0.65); // HEVC uses approx 35% less bandwidth
    } else if (params.encoderConfig === 'H264_MAIN') {
      idealBitrate = Math.round(16 * scaleFactor * 0.85);
    } else {
      idealBitrate = Math.round(18 * scaleFactor);
    }
    
    // YUV444 takes double the data of standard chroma-subsampled NV12
    if (params.colorFormat === 'YUV444') {
      idealBitrate = Math.round(idealBitrate * 1.8);
    }

    setParams(prev => ({
      ...prev,
      bitrateMbps: Math.max(4, Math.min(150, idealBitrate))
    }));
  }, [params.resolutionWidth, params.resolutionHeight, params.fps, params.colorFormat, params.encoderConfig]);

  // Comprehensive latency calculations
  const analysis = useMemo((): LatencyBreakdown => {
    const is4K = params.resolutionWidth >= 3840;
    const is2K = params.resolutionWidth >= 2560 && params.resolutionWidth < 3840;
    const is60Fps = params.fps >= 60;

    // 1. Capture Latency (DXGI Capture API overhead)
    let captureBase = 2.0; // standard 1080p performance
    if (is2K) captureBase = 3.0;
    if (is4K) captureBase = 5.2;

    const cpuStateDivisor = params.cpuMode === 'Performance' ? 1.2 : params.cpuMode === 'Balanced' ? 0.9 : 0.5;
    let captureMs = parseFloat((captureBase / cpuStateDivisor).toFixed(2));

    // 2. Encoding Latency (NVIDIA NVENC, AMD AMF, Intel QuickSync)
    let encodeBase = 3.5;
    if (params.encoderConfig === 'H264_BASELINE') encodeBase = 2.8;
    if (params.encoderConfig === 'H264_MAIN') encodeBase = 4.0;
    if (params.encoderConfig === 'H265_HEVC') encodeBase = 6.2; // HEVC block calculations add encoding path time
    if (is4K) encodeBase *= 1.8;
    else if (is2K) encodeBase *= 1.35;

    let encodeMs = parseFloat((encodeBase / cpuStateDivisor).toFixed(2));

    // 3. User-mode Queue Wait Overhead (Threading handoff & pacing)
    let queueMs = params.cpuMode === 'Performance' ? 0.8 : params.cpuMode === 'Balanced' ? 1.8 : 4.5;
    if (!is60Fps) queueMs += 1.5; // lower frame rates lead to longer buffer pacing delays

    // 4. USB Transfer Latency
    // Payload size in bits per frame = (Bitrate * 10^6) / fps. In Bytes: / 8
    const bytesPerFrame = (params.bitrateMbps * 1000000) / params.fps / 8;
    
    let transportSpeedBps = 480 * 1000000; // USB 2.0 default for ADB port forwarding or standard accessories
    if (params.transportProtocol === 'CUSTOM_WINUSB') {
      transportSpeedBps = 5000 * 1000050 * 0.7; // USB 3.0 superspeed payload rate (approx 3.5 Gbps real throughput)
    }

    const wireTimeMs = (bytesPerFrame * 8 * 1000) / transportSpeedBps;
    
    // Protocol encapsulation & OS driver context switches add time
    let driverOverheadMs = 0.5;
    if (params.transportProtocol === 'ADB_TCP') {
      driverOverheadMs = 2.8; // ADB port forward runs loopback sockets, adding IP/TCP stack routing latency
    } else if (params.transportProtocol === 'USB_ACCESSORY_AOA') {
      driverOverheadMs = 1.2; // Standard AOA 2.0 Android bulk buffer limits
    } else if (params.transportProtocol === 'CUSTOM_WINUSB') {
      driverOverheadMs = 0.3; // Low-overhead kernel bulk transfer IOCTL
    }

    let transferMs = parseFloat((wireTimeMs + driverOverheadMs).toFixed(2));

    // 5. Android Decoding Latency (MediaCodec hardware pipeline)
    let decodeBase = 3.2;
    if (params.encoderConfig === 'H265_HEVC') decodeBase = 5.2; // Android HEVC hardware pipeline requires wider bit stream buffers
    if (is4K) decodeBase *= 1.7;
    else if (is2K) decodeBase *= 1.25;

    // Android device profile estimation
    const androidPerformanceDivisor = params.cpuMode === 'Performance' ? 1.15 : params.cpuMode === 'Balanced' ? 0.95 : 0.6;
    let decodeMs = parseFloat((decodeBase / androidPerformanceDivisor).toFixed(2));

    // 6. Surface Rendering & Choreographer sync (OpenGL swapbuffer & phone display scanout)
    let renderMs = 1.8;
    if (params.cpuMode === 'LowPower') renderMs = 3.8; // background composite lag on cheap phones

    // Calculations of Raw Stream through capacity (Gbps)
    const bpp = params.colorFormat === 'NV12' || params.colorFormat === 'YUV420p' ? 12 : 24;
    const rawThroughputGbps = (params.resolutionWidth * params.resolutionHeight * bpp * params.fps) / 1000000000;

    const totalMs = parseFloat((captureMs + encodeMs + queueMs + transferMs + decodeMs + renderMs).toFixed(1));

    return {
      captureMs,
      encodeMs,
      queueMs,
      transferMs,
      decodeMs,
      renderMs,
      totalMs,
      throughputGbps: parseFloat(rawThroughputGbps.toFixed(3)),
      compressedThroughputMbps: params.bitrateMbps,
    };
  }, [params]);

  // Determine Latency Grade Colors
  const latencyGrade = useMemo(() => {
    if (analysis.totalMs < 16) return { label: 'Optimal (Gaming Ready)', text: 'text-emerald-400', bg: 'bg-emerald-950/30 border-emerald-800/80 shadow-[0_0_15px_rgba(16,185,129,0.07)]' };
    if (analysis.totalMs < 30) return { label: 'Excellent (Workstation Grade)', text: 'text-cyan-400', bg: 'bg-cyan-950/35 border-cyan-800/80 shadow-[0_0_15px_rgba(6,182,212,0.07)]' };
    if (analysis.totalMs < 50) return { label: 'Moderate (Standard Office Fluid)', text: 'text-amber-400', bg: 'bg-amber-950/30 border-amber-800/80 shadow-[0_0_15px_rgba(245,158,11,0.07)]' };
    return { label: 'Poor (High Lag/UI Desync)', text: 'text-rose-400', bg: 'bg-rose-950/30 border-rose-800/80 shadow-[0_0_15px_rgba(244,63,94,0.07)]' };
  }, [analysis.totalMs]);

  return (
    <div className="bg-slate-900 rounded-xl border border-slate-800 shadow-xl p-6">
      <div className="flex flex-col md:flex-row md:items-center justify-between mb-6 gap-4">
        <div>
          <h3 className="text-base font-bold text-white flex items-center gap-2 uppercase tracking-wide font-mono">
            <Layers className="w-5 h-5 text-cyan-400" />
            // PIPELINE_SPEED_SIMULATOR
          </h3>
          <p className="text-xs text-slate-400 mt-1 font-sans">
            Adjust screen resolution, code formatters, and OS transport channels to simulate real-time performance overheads recursively.
          </p>
        </div>
        <button
          onClick={() => setIsPlaying(!isPlaying)}
          className="flex items-center gap-1.5 px-3 py-1.5 text-xs bg-slate-950 hover:bg-slate-850 text-cyan-400 rounded-lg transition-colors font-mono font-bold border border-cyan-800/60 shadow-[0_0_10px_rgba(6,182,212,0.05)] cursor-pointer"
        >
          {isPlaying ? <Pause className="w-3.5 h-3.5" /> : <Play className="w-3.5 h-3.5" />}
          <span>{isPlaying ? 'PAUSE PIPELINE' : 'RUN WAVE PIPELINE'}</span>
        </button>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
        
        {/* Controls Board (5 Columns) */}
        <div className="lg:col-span-5 space-y-5 lg:border-r lg:border-slate-800 pr-0 lg:pr-8">
          <span className="text-[10px] font-mono font-bold uppercase tracking-wider text-cyan-500 block mb-2">// CAPTURE_AND_ENCODER_CONFIG</span>
          
          <div className="space-y-4">
            {/* Resolution */}
            <div>
              <label className="block text-xs font-semibold text-slate-400 mb-1.5 font-sans">
                Target Extended Screen Resolution
              </label>
              <div className="grid grid-cols-3 gap-2">
                {[
                  { w: 1920, h: 1080, label: '1080p FHD' },
                  { w: 2560, h: 1600, label: '2K QHD' },
                  { w: 3840, h: 2160, label: '4K UHD' },
                ].map((res) => (
                  <button
                    key={res.label}
                    onClick={() => setParams({ ...params, resolutionWidth: res.w, resolutionHeight: res.h })}
                    className={`px-3 py-2 text-xs font-semibold rounded-lg border transition-all text-center ${
                      params.resolutionWidth === res.w
                        ? 'border-cyan-500 bg-cyan-950/40 text-cyan-300 shadow-[0_0_10px_rgba(6,182,212,0.1)]'
                        : 'border-slate-800 bg-slate-950/40 hover:bg-slate-850 text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <div className="font-sans">{res.label}</div>
                    <div className="text-[10px] font-mono font-normal text-slate-500 mt-0.5">{res.w}×{res.h}</div>
                  </button>
                ))}
              </div>
            </div>

            {/* Framerate & Subsampling */}
            <div className="grid grid-cols-2 gap-4">
              <div>
                <label className="block text-xs font-semibold text-slate-400 mb-1.5 font-sans">
                  Target Framerate (FPS)
                </label>
                <div className="grid grid-cols-2 gap-1.5">
                  {[30, 60].map((f) => (
                    <button
                      key={f}
                      onClick={() => setParams({ ...params, fps: f })}
                      className={`py-1.5 text-xs font-mono font-bold rounded-lg border text-center transition-all ${
                        params.fps === f
                          ? 'border-cyan-500 bg-cyan-950/40 text-cyan-300'
                          : 'border-slate-800 bg-slate-950/40 hover:bg-slate-850 text-slate-400'
                      }`}
                    >
                      {f} FPS
                    </button>
                  ))}
                </div>
              </div>

              <div>
                <label className="block text-xs font-semibold text-slate-400 mb-1.5 font-sans">
                  Color Pixel Format
                </label>
                <select
                  value={params.colorFormat}
                  onChange={(e) => setParams({ ...params, colorFormat: e.target.value as any })}
                  className="w-full text-xs py-1.5 px-2 bg-slate-950 border border-slate-800 rounded-lg text-slate-300 focus:outline-none focus:ring-1 focus:ring-cyan-500 font-mono"
                >
                  <option value="NV12">NV12 (Speed/HW Favored)</option>
                  <option value="YUV420p">YUV 4:2:0 Planar</option>
                  <option value="YUV444">YUV 4:4:4 (Text Accurate)</option>
                </select>
              </div>
            </div>

            {/* Codec */}
            <div>
              <label className="block text-xs font-semibold text-slate-400 mb-1.5 flex items-center justify-between font-sans">
                <span>Hardware Encoding Codec (Host GPU)</span>
                {params.encoderConfig === 'H265_HEVC' && (
                  <span className="text-[9px] bg-cyan-950 text-cyan-400 border border-cyan-800 px-1.5 py-0.5 rounded font-mono font-bold leading-none">
                    Requires HW SOC Unit
                  </span>
                )}
              </label>
              <select
                value={params.encoderConfig}
                onChange={(e) => setParams({ ...params, encoderConfig: e.target.value as any })}
                className="w-full text-xs py-2 px-3 bg-slate-950 border border-slate-800 rounded-lg text-slate-300 focus:outline-none focus:ring-1 focus:ring-cyan-500 font-mono"
              >
                <option value="H264_BASELINE">AVC H.264 Baseline Profile (Fastest / Zero B-frames)</option>
                <option value="H264_MAIN">AVC H.264 Main Profile (Standard compression/CABAC)</option>
                <option value="H265_HEVC">HEVC H.265 (High-efficiency, complex SoC hardware)</option>
              </select>
            </div>

            {/* Transport protocol */}
            <div>
              <label className="block text-xs font-semibold text-slate-400 mb-1.5 font-sans">
                USB Connection Interface Layer
              </label>
              <select
                value={params.transportProtocol}
                onChange={(e) => setParams({ ...params, transportProtocol: e.target.value as any })}
                className="w-full text-xs py-2 px-3 bg-slate-950 border border-slate-800 rounded-lg text-slate-300 focus:outline-none focus:ring-1 focus:ring-cyan-500 font-mono"
              >
                <option value="CUSTOM_WINUSB">Custom WinUSB Bulk Pipe (USB 3.0 SuperSpeed)</option>
                <option value="USB_ACCESSORY_AOA">Android Open Accessory AOA 2.0 (Plug-and-Play)</option>
                <option value="ADB_TCP">ADB Port Forward Loopback (Development Sockets)</option>
              </select>
            </div>

            {/* Performance State Switcher */}
            <div>
              <label className="block text-xs font-semibold text-slate-400 mb-1.5 font-sans">
                Host / Client Hardware Profile
              </label>
              <div className="grid grid-cols-3 gap-2">
                {[
                  { id: 'LowPower', label: 'Battery Saver' },
                  { id: 'Balanced', label: 'Standard/Balanced' },
                  { id: 'Performance', label: 'Max Performance' },
                ].map((mode) => (
                  <button
                    key={mode.id}
                    onClick={() => setParams({ ...params, cpuMode: mode.id as any })}
                    className={`py-1.5 px-0.5 text-center rounded-lg border text-[11px] font-sans transition-all ${
                      params.cpuMode === mode.id
                        ? 'border-cyan-500 bg-cyan-950/40 text-cyan-350'
                        : 'border-slate-800 bg-slate-950/40 hover:bg-slate-850 text-slate-500 hover:text-slate-400'
                    }`}
                  >
                    {mode.label}
                  </button>
                ))}
              </div>
            </div>
          </div>
        </div>

        {/* Latency Projection (7 Columns) */}
        <div className="lg:col-span-7 flex flex-col justify-between">
          <div>
            <span className="text-[10px] font-mono font-bold uppercase tracking-wider text-cyan-500 block mb-3">// END_TO_END_LATENCY_METRICS</span>
            
            {/* Master display card */}
            <div className={`p-5 rounded-xl border ${latencyGrade.bg} flex flex-col sm:flex-row items-start sm:items-center justify-between mb-6 shadow-sm transition-all duration-300 gap-4`}>
              <div>
                <span className="text-[10px] text-slate-400 block font-mono">TOTAL ACCUMULATED GLASS-TO-GLASS LATENCY</span>
                <span className="text-4xl font-extrabold text-white font-mono tracking-tight mt-1 ml-0.5 block flex items-baseline gap-1">
                  {analysis.totalMs} <span className="text-lg font-bold text-cyan-400">ms</span>
                </span>
                <div className="text-[11px] font-mono text-cyan-400/80 mt-2 block">
                  Processing Rate: {params.fps} Hz (Interval: {(1000 / params.fps).toFixed(1)} ms)
                </div>
              </div>
              <div className="sm:text-right">
                <span className={`px-2.5 py-1 font-mono font-bold text-[10px] uppercase tracking-wide rounded border ${latencyGrade.text} border-current/30 bg-black/30`}>
                  {latencyGrade.label}
                </span>
                <p className="text-[11px] text-slate-400 mt-2 leading-relaxed max-w-[220px]">
                  Under 30ms latency delivers fluid desktop feel mimicking true local hardware displays.
                </p>
              </div>
            </div>

            {/* Horizontal Timeline Bar */}
            <div className="space-y-2 mb-6">
              <span className="text-xs font-mono font-bold text-slate-450 block">Delay Attribution Percentages:</span>
              <div className="w-full h-8 rounded-lg overflow-hidden flex border border-slate-850 shadow-inner">
                <div
                  style={{ width: `${(analysis.captureMs / analysis.totalMs) * 100}%` }}
                  className="bg-amber-400 hover:opacity-90 transition-all duration-300 flex items-center justify-center text-[10px] text-slate-950 font-bold overflow-hidden"
                  title={`Capture Screen: ${analysis.captureMs}ms`}
                >
                  {analysis.captureMs > 2 && `${analysis.captureMs}ms`}
                </div>
                <div
                  style={{ width: `${(analysis.encodeMs / analysis.totalMs) * 100}%` }}
                  className="bg-emerald-400 hover:opacity-90 transition-all duration-300 flex items-center justify-center text-[10px] text-slate-950 font-bold overflow-hidden"
                  title={`Encode: ${analysis.encodeMs}ms`}
                >
                  {analysis.encodeMs > 2 && `${analysis.encodeMs}ms`}
                </div>
                <div
                  style={{ width: `${(analysis.queueMs / analysis.totalMs) * 100}%` }}
                  className="bg-cyan-400 hover:opacity-90 transition-all duration-300 flex items-center justify-center text-[10px] text-slate-950 font-bold overflow-hidden"
                  title={`Thread Hand-off: ${analysis.queueMs}ms`}
                >
                  {analysis.queueMs > 2 && `${analysis.queueMs}ms`}
                </div>
                <div
                  style={{ width: `${(analysis.transferMs / analysis.totalMs) * 100}%` }}
                  className="bg-purple-400 hover:opacity-90 transition-all duration-300 flex items-center justify-center text-[10px] text-slate-950 font-bold overflow-hidden"
                  title={`USB Bus Transport: ${analysis.transferMs}ms`}
                >
                  {analysis.transferMs > 2 && `${analysis.transferMs}ms`}
                </div>
                <div
                  style={{ width: `${(analysis.decodeMs / analysis.totalMs) * 100}%` }}
                  className="bg-rose-405 bg-rose-400 hover:opacity-90 transition-all duration-300 flex items-center justify-center text-[10px] text-slate-950 font-bold overflow-hidden"
                  title={`Android Decode: ${analysis.decodeMs}ms`}
                >
                  {analysis.decodeMs > 2 && `${analysis.decodeMs}ms`}
                </div>
                <div
                  style={{ width: `${(analysis.renderMs / analysis.totalMs) * 100}%` }}
                  className="bg-blue-400 hover:opacity-90 transition-all duration-300 flex items-center justify-center text-[10px] text-slate-950 font-bold overflow-hidden"
                  title={`Surface Rendering: ${analysis.renderMs}ms`}
                >
                  {analysis.renderMs > 1.5 && `${analysis.renderMs}ms`}
                </div>
              </div>

              {/* Timeline custom legend */}
              <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-6 gap-2 text-[10px] text-slate-450 pt-1.5 font-mono">
                <div className="flex items-center gap-1.5">
                  <span className="w-2 h-2 rounded bg-amber-400 shrink-0"></span>
                  <span className="truncate">PC Capture</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2 h-2 rounded bg-emerald-400 shrink-0"></span>
                  <span className="truncate">PC Encode</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2 h-2 rounded bg-cyan-400 shrink-0"></span>
                  <span className="truncate">Queue Pace</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2 h-2 rounded bg-purple-400 shrink-0"></span>
                  <span className="truncate">USB Transfer</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2 h-2 rounded bg-rose-400 shrink-0"></span>
                  <span className="truncate">SoC Decode</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2 h-2 rounded bg-blue-400 shrink-0"></span>
                  <span className="truncate">Render Out</span>
                </div>
              </div>
            </div>

            {/* Bandwidth display breakdown */}
            <div className="bg-slate-950 rounded-lg p-4 border border-slate-800 grid grid-cols-1 sm:grid-cols-2 gap-4">
              <div>
                <span className="text-[10px] font-mono font-bold text-slate-500 block uppercase">RAW PIXEL THROUGHPUT RATE</span>
                <span className="text-base font-bold text-slate-200 mt-1 block font-mono">
                  {analysis.throughputGbps} <span className="text-xs text-slate-500 font-normal">Gbps</span>
                </span>
                <span className="text-[10px] text-slate-450 leading-relaxed block mt-1 font-sans">Requires uncompressed copper cabling standard or native PCIe pathways if not encoded.</span>
              </div>
              <div className="border-t sm:border-t-0 sm:border-l border-slate-800 pt-3 sm:pt-0 sm:pl-4">
                <span className="text-[10px] font-mono font-bold text-slate-500 block uppercase">ENCODED STREAM FLOW RATE</span>
                <span className="text-base font-bold text-slate-200 mt-1 block font-mono flex items-center gap-2">
                  <span>{analysis.compressedThroughputMbps} Mbps</span>
                  <span className="text-[9px] text-cyan-400 bg-cyan-950/80 px-1.5 py-0.5 rounded font-bold font-mono border border-cyan-800/40">
                    {(100 - (analysis.compressedThroughputMbps / (analysis.throughputGbps * 1000)) * 100).toFixed(1)}% Red.
                  </span>
                </span>
                <span className="text-[10px] text-slate-450 leading-relaxed block mt-1 font-sans">Extremely compact. Fits easily in modern high-speed USB device conduits.</span>
              </div>
            </div>
          </div>

          {/* Real-time frame loop visualizer at the bottom */}
          <div className="mt-6 border-t border-slate-800 pt-5">
            <span className="text-[10px] font-mono font-bold uppercase tracking-wider text-cyan-500 block mb-3">
              // ACTIVE_WAVE_PACKETS_DIAGNOSTIC
            </span>

            <div className="relative h-20 bg-slate-950 rounded-lg border border-dashed border-slate-800 flex items-center justify-between px-6 overflow-hidden">
              {/* Progress track */}
              <div className="absolute left-6 right-6 top-1/2 -translate-y-1/2 h-[1px] bg-slate-800 z-0"></div>
              
              {/* Animated pulses representing frame packets */}
              {isPlaying && (
                <>
                  <div
                    style={{ left: `${pipelineProgress}%` }}
                    className="absolute w-4 h-4 -ml-2 top-1/2 -translate-y-1/2 rounded-full bg-cyan-400 shadow-[0_0_15px_rgba(6,182,212,0.8)] border border-slate-950 z-10 transition-all duration-75 flex items-center justify-center animate-ping"
                  ></div>
                  <div
                    style={{ left: `${pipelineProgress}%` }}
                    className="absolute w-3.5 h-3.5 -ml-1.75 top-1/2 -translate-y-1/2 rounded-full bg-cyan-400 shadow-md border-2 border-slate-950 z-10 transition-all duration-75"
                  ></div>
                  {/* Delayed pulse in queue */}
                  <div
                    style={{ left: `${(pipelineProgress + 40) % 100}%` }}
                    className="absolute w-2.5 h-2.5 -ml-1.25 top-1/2 -translate-y-1/2 rounded-full bg-cyan-500/40 z-10 transition-all duration-75"
                  ></div>
                </>
              )}

              {/* Station 1: PCAcapture */}
              <div className="flex flex-col items-center gap-1 z-10 relative">
                <span className="p-1.5 bg-slate-900 border border-amber-500/50 text-amber-400 rounded-md shadow-sm">
                  <HardDrive className="w-3.5 h-3.5" />
                </span>
                <span className="text-[9px] font-mono font-bold text-slate-400">DXGI</span>
              </div>

              <ArrowRight className="w-4 h-4 text-slate-700 z-10" />

              {/* Station 2: Encode */}
              <div className="flex flex-col items-center gap-1 z-10 relative">
                <span className="p-1.5 bg-slate-900 border border-emerald-500/50 text-emerald-400 rounded-md shadow-sm">
                  <CpuIcon className="w-3.5 h-3.5" />
                </span>
                <span className="text-[9px] font-mono font-bold text-slate-400">MAIN_ENC</span>
              </div>

              <ArrowRight className="w-4 h-4 text-slate-700 z-10" />

              {/* Station 3: USB pipe */}
              <div className="flex flex-col items-center gap-1 z-10 relative">
                <span className="p-1.5 bg-slate-900 border border-purple-500/50 text-purple-400 rounded-md shadow-sm">
                  <Zap className="w-3.5 h-3.5" />
                </span>
                <span className="text-[9px] font-mono font-bold text-slate-400">USB_BUS</span>
              </div>

              <ArrowRight className="w-4 h-4 text-slate-700 z-10" />

              {/* Station 4: Android Decoder */}
              <div className="flex flex-col items-center gap-1 z-10 relative">
                <span className="p-1.5 bg-slate-900 border border-rose-500/50 text-rose-400 rounded-md shadow-sm">
                  <Cpu className="w-3.5 h-3.5" />
                </span>
                <span className="text-[9px] font-mono font-bold text-slate-400">CODEC</span>
              </div>

              <ArrowRight className="w-4 h-4 text-slate-700 z-10" />

              {/* Station 5: Smart Screen Rendering */}
              <div className="flex flex-col items-center gap-1 z-10 relative">
                <span className="p-1.5 bg-slate-900 border border-blue-500/50 text-blue-400 rounded-md shadow-sm">
                  <RefreshCw className="w-3.5 h-3.5" />
                </span>
                <span className="text-[9px] font-mono font-bold text-slate-400">DISPLAY</span>
              </div>
            </div>
          </div>

        </div>
      </div>
    </div>
  );
}
