#ifndef P25UTILS_H
#define P25UTILS_H

#include <cstdint>
#include <cstddef>
#include <array>

namespace op25gateway {

// IMBE frame size
constexpr size_t IMBE_FRAME_SIZE = 11;

// P25 LDU sizes
constexpr size_t P25_LDU1_LENGTH = 201;
constexpr size_t P25_LDU2_LENGTH = 189;
constexpr size_t P25_TDU_LENGTH = 24;

// DVMProject network functions
constexpr uint8_t NET_FUNC_PROTOCOL  = 0x00;
constexpr uint8_t NET_FUNC_RPTL      = 0x60;
constexpr uint8_t NET_FUNC_RPTK      = 0x61;
constexpr uint8_t NET_FUNC_RPTC      = 0x62;
constexpr uint8_t NET_FUNC_RPT_DISC  = 0x70;
constexpr uint8_t NET_FUNC_PING      = 0x74;
constexpr uint8_t NET_FUNC_PONG      = 0x75;
constexpr uint8_t NET_FUNC_ACK       = 0x7E;
constexpr uint8_t NET_FUNC_NAK       = 0x7F;
constexpr uint8_t NET_SUBFUNC_NOP    = 0xFF;
constexpr uint8_t NET_SUBFUNC_P25    = 0x01;

// P25 DUIDs
constexpr uint8_t P25_DUID_LDU1 = 0x05;
constexpr uint8_t P25_DUID_LDU2 = 0x0A;
constexpr uint8_t P25_DUID_TDU  = 0x03;

// P25 LCOs
constexpr uint8_t P25_LCO_GROUP_VOICE = 0x00;
constexpr uint8_t P25_LCO_CALL_TERM = 0x0F;

// Network control flags
constexpr uint8_t NET_CTRL_GRANT_DEMAND = 0x80;

// RTP end-of-call sequence
constexpr uint16_t RTP_END_OF_CALL_SEQ = 0xFFFF;

// DVM frame marker
constexpr uint8_t DVM_FRAME_START = 0xFE;

// OP25 packet magic bytes
constexpr uint16_t OP25_MAGIC = 0x4F50;  // "OP"

// OP25 frame types
constexpr uint8_t OP25_FRAME_LDU1 = 1;
constexpr uint8_t OP25_FRAME_LDU2 = 2;

// OP25 packet structure (27 bytes)
struct OP25Packet {
    uint16_t magic;         // 0x4F50 "OP"
    uint16_t nac;           // NAC (big-endian)
    uint32_t talkgroup;     // Talkgroup ID (big-endian)
    uint32_t sourceId;      // Source Radio ID (big-endian)
    uint8_t  frameType;     // 1=LDU1, 2=LDU2
    uint8_t  voiceIndex;    // Voice Frame Index (0-8)
    uint8_t  flags;         // Bit 0: encrypted
    uint8_t  reserved;
    uint8_t  imbe[11];      // IMBE Frame Data
};

constexpr size_t OP25_PACKET_SIZE = 27;

class P25Utils {
public:
    // CRC-16-CCITT calculation
    static uint16_t crc16_ccitt(const uint8_t* data, size_t len);

    // Build DVM/RTP header (32 bytes)
    static void buildDVMHeader(uint8_t* buffer, uint8_t func, uint8_t subFunc,
                                uint32_t streamId, uint32_t peerId,
                                uint16_t& seq, uint32_t& timestamp,
                                size_t payloadLen, bool endOfCall = false);

    // Insert CRC into DVM header
    static void insertDVMCrc(uint8_t* buffer, size_t totalLen);

    // Build P25 message header (24 bytes)
    static void buildP25Header(uint8_t* buffer, uint8_t duid,
                                uint32_t srcId, uint32_t dstId,
                                uint32_t wacn, uint16_t sysId, uint8_t count);

    // Build LDU1 frame (201 bytes) from 9 IMBE frames
    static void buildLDU1(uint8_t* buffer, const uint8_t imbe[9][IMBE_FRAME_SIZE],
                          uint32_t srcId, uint32_t dstId,
                          uint32_t wacn, uint16_t sysId, bool firstLDU);

    // Build LDU2 frame (189 bytes) from 9 IMBE frames
    static void buildLDU2(uint8_t* buffer, const uint8_t imbe[9][IMBE_FRAME_SIZE],
                          uint32_t srcId, uint32_t dstId,
                          uint32_t wacn, uint16_t sysId);

    // Build TDU frame (24 bytes)
    static void buildTDU(uint8_t* buffer, uint32_t srcId, uint32_t dstId,
                         uint32_t wacn, uint16_t sysId, bool grantDemand);

    // Parse OP25 packet
    static bool parseOP25Packet(const uint8_t* data, size_t len, OP25Packet& packet);

    // Reed-Solomon encoding for LC
    static void encodeLC(uint8_t* rsEncoded, uint32_t srcId, uint32_t dstId);
};

} // namespace op25gateway

#endif // P25UTILS_H
