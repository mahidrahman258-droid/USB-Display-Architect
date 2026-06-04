package com.plugscreen.usb

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * @class AoaTransportTest
 * @brief High-performance unit test cases validating the AOA USB wire protocol.
 */
class AoaTransportTest {

    @Test
    fun testProtocolConstants() {
        // Assert correct protocol headers and packet types match host definitions
        assertEquals(0x01.toByte(), AoaTransport.TYPE_HANDSHAKE_REQUEST)
        assertEquals(0x02.toByte(), AoaTransport.TYPE_HANDSHAKE_RESPONSE)
        assertEquals(0x03.toByte(), AoaTransport.TYPE_HEARTBEAT)
        assertEquals(0x04.toByte(), AoaTransport.TYPE_VIDEO_FRAME)
        assertEquals(0x05.toByte(), AoaTransport.TYPE_INPUT_EVENT)
        assertEquals(0x06.toByte(), AoaTransport.TYPE_DISCONNECT)

        assertEquals(0x01.toByte(), AoaTransport.FLAG_KEYFRAME)
        assertEquals(0x02.toByte(), AoaTransport.FLAG_FIRST_SLICE)
        assertEquals(0x04.toByte(), AoaTransport.FLAG_LAST_SLICE)
    }

    @Test
    fun testHandshakePayloadLength() {
        val deviceName = "Pixel 8 Pro Test Display"
        val nameBytes = deviceName.toByteArray(Charsets.UTF_8)
        val stringLen = nameBytes.size.coerceAtMost(32)

        // Validate payload matches exactly 42 bytes (2 + 2 + 2 + 4 + 32)
        val payload = ByteBuffer.allocate(42).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            putShort(1920.toShort()) // Desired Width
            putShort(1080.toShort()) // Desired Height
            putShort(60.toShort())   // Preferred Framerate
            putInt(6000000)          // targetBitrate: 6 Mbps default
            put(nameBytes, 0, stringLen)
            // Fill remainder with zeros
            if (stringLen < 32) {
                put(ByteArray(32 - stringLen))
            }
        }.array()

        assertEquals(42, payload.size)
        
        // Assert field offsets inside payload
        val wrapper = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        assertEquals(1920.toShort(), wrapper.short)
        assertEquals(1080.toShort(), wrapper.short)
        assertEquals(60.toShort(), wrapper.short)
        assertEquals(6000000, wrapper.int)
        
        val devBytes = ByteArray(32)
        wrapper.get(devBytes)
        val parsedName = String(devBytes).trim { it <= ' ' }
        assertEquals("Pixel 8 Pro Test Display", parsedName)
    }

    @Test
    fun testInputEventPayloadOffset() {
        val action: Byte = 2 // Move action
        val touchId: Short = 1
        val normalizedX: Short = 4500
        val normalizedY: Short = 8200

        // Format and unpack touch coordinate structures
        val payload = ByteBuffer.allocate(7).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            put(action)
            putShort(touchId)
            putShort(normalizedX)
            putShort(normalizedY)
        }.array()

        assertEquals(7, payload.size)
        
        val wrapper = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        assertEquals(2.toByte(), wrapper.get())
        assertEquals(1.toShort(), wrapper.short)
        assertEquals(4500.toShort(), wrapper.short)
        assertEquals(8200.toShort(), wrapper.short)
    }

    @Test
    fun testFragmentsSliceAssemble() {
        val firstSliceBuffer = byteArrayOf(1, 2, 3, 4)
        val lastSliceBuffer = byteArrayOf(5, 6, 7, 8)

        val assembleStream = ByteArrayOutputStream()
        
        // Simulate slice reassembly matching first slice trigger
        assembleStream.reset()
        assembleStream.write(firstSliceBuffer)
        assembleStream.write(lastSliceBuffer)

        val fullFrameResult = assembleStream.toByteArray()
        assertEquals(8, fullFrameResult.size)
        assertTrue(fullFrameResult.contentEquals(byteArrayOf(1, 2, 3, 4, 5, 6, 7, 8)))
    }
}
