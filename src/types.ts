export interface SimulatorParams {
  resolutionWidth: number;
  resolutionHeight: number;
  fps: number;
  colorFormat: 'NV12' | 'YUV420p' | 'YUV444';
  encoderConfig: 'H264_BASELINE' | 'H264_MAIN' | 'H265_HEVC';
  transportProtocol: 'ADB_TCP' | 'USB_ACCESSORY_AOA' | 'CUSTOM_WINUSB';
  bitrateMbps: number;
  cpuMode: 'LowPower' | 'Balanced' | 'Performance';
}

export interface LatencyBreakdown {
  captureMs: number;
  encodeMs: number;
  queueMs: number;
  transferMs: number;
  decodeMs: number;
  renderMs: number;
  totalMs: number;
  throughputGbps: number;
  compressedThroughputMbps: number;
}

export interface PacketHeaderState {
  magic: string;          // 4 bytes: 'USBD' (0x55534244)
  packetType: number;     // 1 byte: 1 = Frame, 2 = Control, 3 = Input, 4 = KeepAlive
  frameIndex: number;     // 4 bytes
  payloadLength: number;  // 4 bytes
  timestamp: number;      // 8 bytes (milliseconds)
  sliceIndex: number;     // 2 bytes (for slice-based decoding)
  checksum: boolean;      // 1 byte (CRC-16 or checksum inclusion)
}
