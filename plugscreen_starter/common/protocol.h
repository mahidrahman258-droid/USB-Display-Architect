#ifndef PLUGSCREEN_COMMON_PROTOCOL_H
#define PLUGSCREEN_COMMON_PROTOCOL_H

#include <stdint.h>

#ifdef _MSC_VER
#pragma pack(push, 1)
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

namespace PlugScreen {

// Magic signatures to validate packet integrity and align incoming streams
const uint8_t MAGIC_SIGNATURE[4] = { 'P', 'S', 'C', 'R' }; // PlugScreen

// Packet Types
enum class PacketType : uint8_t {
    HandshakeRequest   = 0x01, // Windows -> Android: Offers display metrics
    HandshakeResponse  = 0x02, // Android -> Windows: Agrees or configures display
    Heartbeat          = 0x03, // Periodic wellness check (Bidirectional)
    VideoFrame         = 0x04, // Encoded H.264 slice block
    InputEvent         = 0x05, // Android -> Windows: Virtual digitizer pointer events
    Disconnect         = 0x06  // Tear down notification
};

// Flags inside PacketHeader
enum FrameFlags : uint8_t {
    FrameFlag_KeyFrame   = 0x01, // Indicates packet is an IDR/I-Frame keyframe
    FrameFlag_FirstSlice = 0x02, // First packet of a fragmented frame
    FrameFlag_LastSlice  = 0x04, // Last packet of a fragmented frame
};

// Touch actions for virtual input backflow
enum class TouchAction : uint8_t {
    Down = 0x00,
    Move = 0x01,
    Up   = 0x02
};

/**
 * @brief Binary Header prefixed to EVERY packet sent across the raw USB conduit.
 * Must align to a precise 22-byte packing schema.
 */
struct PacketHeader {
    uint8_t  magic[4];       // Must match MAGIC_SIGNATURE
    uint8_t  packetType;     // Map to PacketType enum
    uint32_t payloadLength;  // Size in bytes of the following payload
    uint64_t timestampUs;    // Presentation timestamp (microsecond accuracy)
    uint32_t sequenceIndex;  // Sequential index number tracking frames or states
    uint8_t  flags;          // Bitwise OR of FrameFlags elements
} PACKED;

/**
 * @brief Handshake Payload sent directly following PacketHeader during initial connection setup.
 */
struct HandshakePayload {
    uint16_t requestWidth;   // Preferred extended width (e.g. 1920)
    uint16_t requestHeight;  // Preferred extended height (e.g. 1080)
    uint16_t preferredFps;   // Custom refresh boundaries (e.g. 60)
    uint32_t targetBitrate;  // In bits-per-second (e.g. 4000000 for 4Mbps)
    char     deviceName[32]; // Device identification info
} PACKED;

/**
 * @brief Virtual Touch Input Event Payload mapped back from the device capacitive layer.
 */
struct InputEventPayload {
    uint8_t  action;         // TouchAction down, move, or up
    uint16_t touchId;        // Multi-touch reference tracker identifier
    uint16_t normalizedX;    // Scaled touch coordinate x [0-10000] mapping to display
    uint16_t normalizedY;    // Scaled touch coordinate y [0-10000] mapping to display
} PACKED;

} // namespace PlugScreen

#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef PACKED

#endif // PLUGSCREEN_COMMON_PROTOCOL_H
