import { useState, useMemo } from 'react';
import { Cpu, Binary, Layers, HelpCircle, Check, Code } from 'lucide-react';

export default function ProtocolBuilder() {
  const [packetType, setPacketType] = useState<number>(0x01); // 1: I-Frame, 2: P-Frame, 3: Touch Event, 4: Control
  const [frameIndex, setFrameIndex] = useState<number>(1429);
  const [timestamp, setTimestamp] = useState<number>(14850);
  const [payloadLength, setPayloadLength] = useState<number>(31420);
  const [sliceIndex, setSliceIndex] = useState<number>(3);
  const [flags, setFlags] = useState({
    isKeyFrame: true,
    isCompressedControl: false,
    hasChecksum: true,
  });

  // Calculate binary representation (24-byte packet header)
  const binaryHeader = useMemo(() => {
    const buffer = new ArrayBuffer(24);
    const view = new DataView(buffer);

    // Byte 0-3: Magic Bytes 'USBD' => 0x55 0x53 0x42 0x44
    view.setUint8(0, 0x55);
    view.setUint8(1, 0x53);
    view.setUint8(2, 0x42);
    view.setUint8(3, 0x44);

    // Byte 4: Packet Type
    view.setUint8(4, packetType);

    // Byte 5: Flags bitmap
    let flagByte = 0;
    if (flags.isKeyFrame) flagByte |= 0x01;
    if (flags.isCompressedControl) flagByte |= 0x02;
    if (flags.hasChecksum) flagByte |= 0x04;
    view.setUint8(5, flagByte);

    // Byte 6-7: Slice Index group
    view.setUint16(6, sliceIndex, false); // Big endian

    // Byte 8-11: Frame Index (uint32)
    view.setUint32(8, frameIndex, false); // Big endian

    // Byte 12-15: Payload Length (uint32)
    view.setUint32(12, payloadLength, false);

    // Byte 16-23: Timestamp uint64 (using two uint32s since JS has limits)
    view.setUint32(16, 0, false); // High word
    view.setUint32(20, timestamp, false); // Low word

    const bytes = new Uint8Array(buffer);
    return bytes;
  }, [packetType, frameIndex, timestamp, payloadLength, sliceIndex, flags]);

  // Hex stream formatted
  const hexHexes = Array.from(binaryHeader).map(b => Number(b).toString(16).padStart(2, '0').toUpperCase());

  // Helper to color fields
  const getFieldColor = (index: number) => {
    if (index >= 0 && index <= 3) return 'bg-amber-950/30 text-amber-400 border-amber-900/40';
    if (index === 4) return 'bg-emerald-950/30 text-emerald-400 border-emerald-900/40';
    if (index === 5) return 'bg-sky-950/30 text-sky-400 border-sky-900/40';
    if (index >= 6 && index <= 7) return 'bg-purple-950/30 text-purple-400 border-purple-900/40';
    if (index >= 8 && index <= 11) return 'bg-cyan-950/45 text-cyan-300 border-cyan-800/60 shadow-[0_0_8px_rgba(6,182,212,0.1)]';
    if (index >= 12 && index <= 15) return 'bg-rose-950/30 text-rose-400 border-rose-900/40';
    return 'bg-blue-950/30 text-blue-400 border-blue-900/40';
  };

  const getFieldLabel = (index: number) => {
    if (index >= 0 && index <= 3) return { name: 'Magic ID ("USBD")', color: 'text-amber-400' };
    if (index === 4) return { name: 'Packet Type', color: 'text-emerald-400' };
    if (index === 5) return { name: 'Flags bitmap', color: 'text-sky-400' };
    if (index >= 6 && index <= 7) return { name: 'Slice Index', color: 'text-purple-400' };
    if (index >= 8 && index <= 11) return { name: 'Frame Index', color: 'text-cyan-455 text-cyan-400' };
    if (index >= 12 && index <= 15) return { name: 'Payload Len', color: 'text-rose-400' };
    return { name: 'Timestamp (μs)', color: 'text-blue-400' };
  };

  return (
    <div id="protocol-builder" className="bg-slate-900 rounded-xl border border-slate-800 shadow-xl p-6">
      <div className="flex flex-col md:flex-row md:items-center justify-between mb-6 pb-4 border-b border-slate-800 gap-4">
        <div>
          <div className="flex items-center gap-2">
            <span className="p-2 bg-slate-950 text-cyan-400 rounded-lg border border-cyan-900/80">
              <Binary className="w-5 h-5 animate-pulse" />
            </span>
            <h3 className="text-base font-bold text-white uppercase tracking-wide font-mono">
              // USB_CUSTOM_FRAMING_PROTOCOL_BUILDER
            </h3>
          </div>
          <p className="text-xs text-slate-400 mt-1.5 font-sans">
            Build and inspect the layout of the ultra-low latency, custom 24-byte binary framing header written over high-speed USB channels.
          </p>
        </div>
        <div className="flex items-center gap-2 self-start md:self-auto font-mono text-[10px]">
          <span className="px-2.5 py-1 text-xs font-semibold rounded bg-cyan-950/80 text-cyan-400 border border-cyan-800/40">
            Big Endian (Network Order)
          </span>
          <span className="px-2.5 py-1 text-xs font-semibold rounded bg-slate-950 text-slate-400 border border-slate-800">
            24-Byte Const Header
          </span>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
        {/* Controls Column */}
        <div className="lg:col-span-5 space-y-5">
          <h4 className="text-[10px] font-mono font-bold uppercase tracking-wider text-cyan-500">// HEADER_FIELD_CONFIGURATOR</h4>
          
          <div className="space-y-4">
            {/* Packet Type */}
            <div>
              <label className="block text-xs font-semibold text-slate-400 mb-1.5 flex items-center justify-between">
                <span>Packet Type (Byte 4)</span>
                <span className="text-[10px] font-mono bg-slate-950 border border-slate-850 px-1.5 py-0.5 rounded text-amber-400">
                  0x{packetType.toString(16).padStart(2, '0').toUpperCase()}
                </span>
              </label>
              <div className="grid grid-cols-2 gap-2">
                {[
                  { value: 0x01, label: 'I-Frame (Video)' },
                  { value: 0x02, label: 'P-Frame (Video)' },
                  { value: 0x03, label: 'Touch Event' },
                  { value: 0x04, label: 'Control / Cmd' },
                ].map((type) => (
                  <button
                    key={type.value}
                    onClick={() => setPacketType(type.value)}
                    className={`px-3 py-2 text-xs font-medium rounded-lg border text-left transition-all flex items-center justify-between cursor-pointer ${
                      packetType === type.value
                        ? 'border-cyan-500 bg-cyan-950/30 text-cyan-300'
                        : 'border-slate-800 bg-slate-950/40 hover:bg-slate-850 text-slate-405 text-slate-400'
                    }`}
                  >
                    <span>{type.label}</span>
                    {packetType === type.value && <Check className="w-3.5 h-3.5 text-cyan-400" />}
                  </button>
                ))}
              </div>
            </div>

            {/* Bit Flags */}
            <div>
              <label className="block text-xs font-semibold text-slate-400 mb-1.5">
                Bit Flags Bitmap (Byte 5)
              </label>
              <div className="space-y-2 bg-slate-950 p-3 rounded-lg border border-slate-850">
                <label className="flex items-center gap-2.5 text-xs text-slate-400 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={flags.isKeyFrame}
                    onChange={(e) => setFlags({ ...flags, isKeyFrame: e.target.checked })}
                    className="rounded border-slate-800 text-cyan-500 focus:ring-cyan-500 bg-slate-950"
                  />
                  <span><strong className="text-slate-350 font-mono">0x01</strong> - Frame Is Keyframe (H.264 I-Frame flag)</span>
                </label>
                <label className="flex items-center gap-2.5 text-xs text-slate-400 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={flags.isCompressedControl}
                    onChange={(e) => setFlags({ ...flags, isCompressedControl: e.target.checked })}
                    className="rounded border-slate-800 text-cyan-500 focus:ring-cyan-500 bg-slate-950"
                  />
                  <span><strong className="text-slate-355 text-slate-300 font-mono">0x02</strong> - Payload compressed via LZ4/ZStandard</span>
                </label>
                <label className="flex items-center gap-2.5 text-xs text-slate-400 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={flags.hasChecksum}
                    onChange={(e) => setFlags({ ...flags, hasChecksum: e.target.checked })}
                    className="rounded border-slate-800 text-cyan-500 focus:ring-cyan-500 bg-slate-950"
                  />
                  <span><strong className="text-slate-355 text-slate-300 font-mono">0x04</strong> - Checksum field is populated at trailing 2B</span>
                </label>
              </div>
            </div>

            {/* Numeric fields */}
            <div className="grid grid-cols-2 gap-4">
              <div>
                <label className="block text-xs font-semibold text-slate-400 mb-1">
                  Slice ID (Bytes 6-7)
                </label>
                <input
                  type="number"
                  min="0"
                  max="65535"
                  value={sliceIndex}
                  onChange={(e) => setSliceIndex(Math.max(0, Math.min(65535, parseInt(e.target.value) || 0)))}
                  className="w-full px-3 py-1.5 text-xs font-mono bg-slate-950 border border-slate-800 rounded-lg text-slate-200 focus:outline-none focus:ring-1 focus:ring-cyan-500"
                />
              </div>
              <div>
                <label className="block text-xs font-semibold text-slate-400 mb-1">
                  Frame Index (Bytes 8-11)
                </label>
                <input
                  type="number"
                  min="0"
                  value={frameIndex}
                  onChange={(e) => setFrameIndex(Math.max(0, parseInt(e.target.value) || 0))}
                  className="w-full px-3 py-1.5 text-xs font-mono bg-slate-950 border border-slate-800 rounded-lg text-slate-200 focus:outline-none focus:ring-1 focus:ring-cyan-500"
                />
              </div>
            </div>

            <div className="grid grid-cols-2 gap-4">
              <div>
                <label className="block text-xs font-semibold text-slate-400 mb-1">
                  Payload Length (B)
                </label>
                <input
                  type="number"
                  min="0"
                  max="10000000"
                  value={payloadLength}
                  onChange={(e) => setPayloadLength(Math.max(0, parseInt(e.target.value) || 0))}
                  className="w-full px-3 py-1.5 text-xs font-mono bg-slate-950 border border-slate-800 rounded-lg text-slate-200 focus:outline-none focus:ring-1 focus:ring-cyan-500"
                />
              </div>
              <div>
                <label className="block text-xs font-semibold text-slate-400 mb-1">
                  Timestamp Offset (μs)
                </label>
                <input
                  type="number"
                  min="0"
                  value={timestamp}
                  onChange={(e) => setTimestamp(Math.max(0, parseInt(e.target.value) || 0))}
                  className="w-full px-3 py-1.5 text-xs font-mono bg-slate-950 border border-slate-800 rounded-lg text-slate-200 focus:outline-none focus:ring-1 focus:ring-cyan-500"
                />
              </div>
            </div>

          </div>
        </div>

        {/* Binary Hex Dump Column */}
        <div className="lg:col-span-7 flex flex-col space-y-4">
          <h4 className="text-[10px] font-mono font-bold uppercase tracking-wider text-cyan-500 flex items-center justify-between">
            <span>LIVE BINARY MEMORY MAP (Hex Dump & Field Overlay)</span>
            <span className="text-[10px] text-cyan-400 font-mono font-bold">Offset [0x00 - 0x17]</span>
          </h4>

          {/* Hex display grid */}
          <div className="bg-slate-950 text-white rounded-lg border border-slate-850 p-4 font-mono shadow-inner flex-grow">
            <div className="grid grid-cols-8 gap-x-2 gap-y-3 mb-6 relative">
              {/* Header numbers */}
              {Array.from({ length: 8 }).map((_, col) => (
                <div key={col} className="text-center text-[10px] text-slate-600 font-bold border-b border-slate-900 pb-1.5">
                  +{col.toString(16).toUpperCase()}
                </div>
              ))}

              {/* Hex bytes with colored tags */}
              {hexHexes.map((hex, i) => {
                const colorClass = getFieldColor(i);
                return (
                  <div key={i} className="flex flex-col items-center gap-1 group relative">
                    <span className={`w-10 h-10 flex items-center justify-center text-xs font-extrabold rounded border transition-all ${colorClass} shadow-sm group-hover:scale-110 cursor-help`}>
                      {hex}
                    </span>
                    <span className="text-[9px] text-slate-600 font-bold">
                      {i.toString(10).padStart(2, '0')}
                    </span>
                    
                    {/* Tooltip on hover */}
                    <div className="absolute bottom-full left-1/2 -translate-x-1/2 mb-2 w-36 bg-slate-900 border border-slate-800 p-2.5 rounded shadow-xl hidden group-hover:block z-25 text-left leading-normal">
                      <p className={`text-[10px] font-bold ${getFieldLabel(i).color}`}>{getFieldLabel(i).name}</p>
                      <p className="text-[9px] text-slate-400 mt-0.5">Byte offset: {i}</p>
                      <p className="text-[9px] text-slate-400">Val (Hex): 0x{hex}</p>
                    </div>
                  </div>
                );
              })}
            </div>

            {/* Field Guide legend */}
            <div className="border-t border-slate-900 pt-3 mt-4 space-y-2">
              <span className="text-[10px] font-bold text-slate-505 block uppercase mb-1 font-mono">// FIELD_DEFINITION_MAPPING:</span>
              <div className="grid grid-cols-2 md:grid-cols-2 lg:grid-cols-3 gap-x-3 gap-y-2 text-[10px] font-sans">
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded bg-amber-500 shrink-0"></span>
                  <span className="text-slate-400">Magic ID "USBD"</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded bg-emerald-500 shrink-0"></span>
                  <span className="text-slate-400">Packet Type (1B)</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded bg-sky-500 shrink-0"></span>
                  <span className="text-slate-400">Flags Bitmap (1B)</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded bg-purple-500 shrink-0"></span>
                  <span className="text-slate-400">Slice Index (2B)</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded bg-cyan-400 shrink-0"></span>
                  <span className="text-slate-400">Frame Index (4B)</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded bg-rose-500 shrink-0"></span>
                  <span className="text-slate-400">Payload Length (4B)</span>
                </div>
                <div className="flex items-center gap-1.5">
                  <span className="w-2.5 h-2.5 rounded bg-blue-500 shrink-0"></span>
                  <span className="text-slate-400">Timestamp (8B)</span>
                </div>
              </div>
            </div>
          </div>

          {/* Quick explanations */}
          <div className="bg-slate-950 p-4 rounded-xl border border-slate-850 flex items-start gap-3">
            <Cpu className="w-4 h-4 text-cyan-400 mt-0.5 flex-shrink-0 animate-pulse" />
            <div className="text-xs text-slate-400 leading-relaxed font-sans">
              <strong>Low-Latency Framing Concept:</strong> Frame fragmenting or slicing reduces first-pixel display latency. By including a <code className="bg-slate-900 border border-slate-800 px-1 py-0.5 rounded text-cyan-400 font-mono">Slice Index</code>, the Android receiver can immediately pass raw NAL units to `MediaCodec` as they arrive over the USB wire instead of waiting for the full screen frame payload to materialize, achieving sub-10ms packet-handling queues overhead!
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
