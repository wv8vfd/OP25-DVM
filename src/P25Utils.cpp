#include "P25Utils.h"
#include <cstring>
#include <arpa/inet.h>

namespace op25gateway {

uint16_t P25Utils::crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc = crc << 1;
        }
    }
    return crc;
}

void P25Utils::buildDVMHeader(uint8_t* buffer, uint8_t func, uint8_t subFunc,
                               uint32_t streamId, uint32_t peerId,
                               uint16_t& seq, uint32_t& timestamp,
                               size_t payloadLen, bool endOfCall) {
    // RTP Header (12 bytes)
    buffer[0] = 0x90;  // V=2, P=0, X=1, CC=0
    buffer[1] = 0x56;  // PT=86 (DVMProject)

    uint16_t seqNum;
    if (endOfCall) {
        seqNum = RTP_END_OF_CALL_SEQ;
    } else {
        seqNum = seq++;
    }
    buffer[2] = (seqNum >> 8) & 0xFF;
    buffer[3] = seqNum & 0xFF;

    // Timestamp
    timestamp += 160;
    buffer[4] = (timestamp >> 24) & 0xFF;
    buffer[5] = (timestamp >> 16) & 0xFF;
    buffer[6] = (timestamp >> 8) & 0xFF;
    buffer[7] = timestamp & 0xFF;

    // SSRC (peer ID)
    buffer[8] = (peerId >> 24) & 0xFF;
    buffer[9] = (peerId >> 16) & 0xFF;
    buffer[10] = (peerId >> 8) & 0xFF;
    buffer[11] = peerId & 0xFF;

    // RFC 3550 Extension Header (4 bytes)
    buffer[12] = 0x00;
    buffer[13] = DVM_FRAME_START;  // 0xFE
    buffer[14] = 0x00;
    buffer[15] = 0x04;  // Extension length = 4 words

    // FNE Extension Data (16 bytes)
    buffer[16] = 0x00;  // CRC-16 placeholder
    buffer[17] = 0x00;

    buffer[18] = func;
    buffer[19] = subFunc;

    // Stream ID
    buffer[20] = (streamId >> 24) & 0xFF;
    buffer[21] = (streamId >> 16) & 0xFF;
    buffer[22] = (streamId >> 8) & 0xFF;
    buffer[23] = streamId & 0xFF;

    // Peer ID
    buffer[24] = (peerId >> 24) & 0xFF;
    buffer[25] = (peerId >> 16) & 0xFF;
    buffer[26] = (peerId >> 8) & 0xFF;
    buffer[27] = peerId & 0xFF;

    // Message length
    buffer[28] = (payloadLen >> 24) & 0xFF;
    buffer[29] = (payloadLen >> 16) & 0xFF;
    buffer[30] = (payloadLen >> 8) & 0xFF;
    buffer[31] = payloadLen & 0xFF;
}

void P25Utils::insertDVMCrc(uint8_t* buffer, size_t totalLen) {
    uint16_t crc = crc16_ccitt(buffer + 32, totalLen - 32);
    buffer[16] = (crc >> 8) & 0xFF;
    buffer[17] = crc & 0xFF;
}

void P25Utils::buildP25Header(uint8_t* buffer, uint8_t duid,
                               uint32_t srcId, uint32_t dstId,
                               uint32_t wacn, uint16_t sysId, uint8_t count) {
    // P25 header layout - matches real RF site capture
    buffer[0] = 'P';
    buffer[1] = '2';
    buffer[2] = '5';
    buffer[3] = 'D';

    // LCO (Link Control Opcode) - 0x00 = Group Voice Channel User
    buffer[4] = P25_LCO_GROUP_VOICE;

    // Source ID (24-bit, big-endian)
    buffer[5] = (srcId >> 16) & 0xFF;
    buffer[6] = (srcId >> 8) & 0xFF;
    buffer[7] = srcId & 0xFF;

    // Destination ID (24-bit, big-endian)
    buffer[8] = (dstId >> 16) & 0xFF;
    buffer[9] = (dstId >> 8) & 0xFF;
    buffer[10] = dstId & 0xFF;

    // System ID (16-bit, big-endian)
    buffer[11] = (sysId >> 8) & 0xFF;
    buffer[12] = sysId & 0xFF;

    // Reserved
    buffer[13] = 0x00;

    // Control
    buffer[14] = 0x00;

    // MFId (Manufacturer ID)
    buffer[15] = 0x00;

    // WACN (24-bit, big-endian)
    buffer[16] = (wacn >> 16) & 0xFF;
    buffer[17] = (wacn >> 8) & 0xFF;
    buffer[18] = wacn & 0xFF;

    // Reserved
    buffer[19] = 0x00;

    // LSD (Low Speed Data)
    buffer[20] = 0x00;
    buffer[21] = 0x00;

    // DUID
    buffer[22] = duid;

    // Count
    buffer[23] = count;
}

void P25Utils::encodeLC(uint8_t* rsEncoded, uint32_t srcId, uint32_t dstId) {
    // Simple LC encoding - encode srcId and dstId into RS bytes
    // This is a simplified version - real RS encoding would use proper codec

    // LC bytes format: LCO, MFID, ServiceOpts, DstId(3 bytes), SrcId(3 bytes)
    uint8_t lcBytes[9];
    lcBytes[0] = P25_LCO_GROUP_VOICE;  // LCO
    lcBytes[1] = 0x00;                  // MFID
    lcBytes[2] = 0x00;                  // Service Options
    lcBytes[3] = (dstId >> 16) & 0xFF;
    lcBytes[4] = (dstId >> 8) & 0xFF;
    lcBytes[5] = dstId & 0xFF;
    lcBytes[6] = (srcId >> 16) & 0xFF;
    lcBytes[7] = (srcId >> 8) & 0xFF;
    lcBytes[8] = srcId & 0xFF;

    // For now, just copy LC bytes and pad RS with zeros
    // Real implementation would use RS(24,12,13) encoding
    std::memcpy(rsEncoded, lcBytes, 9);
    std::memset(rsEncoded + 9, 0, 15);  // RS parity bytes
}

void P25Utils::buildLDU1(uint8_t* buffer, const uint8_t imbe[9][IMBE_FRAME_SIZE],
                          uint32_t srcId, uint32_t dstId,
                          uint32_t wacn, uint16_t sysId, bool firstLDU) {
    std::memset(buffer, 0x00, P25_LDU1_LENGTH);

    // P25 message header (24 bytes)
    buildP25Header(buffer, P25_DUID_LDU1, srcId, dstId, wacn, sysId, 0xB2);

    // Encode LC for DFSI frames
    uint8_t rsEncoded[24];
    encodeLC(rsEncoded, srcId, dstId);

    // Voice1 (22 bytes at offset 24): Frame type + LC + RSSI + IMBE
    buffer[24] = 0x62;                                    // Frame type
    buffer[25] = rsEncoded[0];                            // LC byte 0
    buffer[26] = rsEncoded[1];                            // LC byte 1
    buffer[27] = rsEncoded[2];                            // LC byte 2
    buffer[28] = rsEncoded[3];                            // LC byte 3
    buffer[29] = rsEncoded[4];                            // LC byte 4
    buffer[30] = 0x00;                                    // RSSI
    buffer[31] = 0x00;
    buffer[32] = 0x00;
    buffer[33] = 0x00;
    std::memcpy(&buffer[34], imbe[0], 11);

    // Voice2 (14 bytes at offset 46): Frame type + IMBE
    buffer[46] = 0x63;
    std::memcpy(&buffer[47], imbe[1], 11);

    // Voice3 (17 bytes at offset 60)
    buffer[60] = 0x64;
    buffer[61] = rsEncoded[5];
    buffer[62] = rsEncoded[6];
    buffer[63] = rsEncoded[7];
    buffer[64] = 0x00;
    std::memcpy(&buffer[65], imbe[2], 11);

    // Voice4 (17 bytes at offset 77)
    buffer[77] = 0x65;
    buffer[78] = rsEncoded[8];
    buffer[79] = rsEncoded[9];
    buffer[80] = rsEncoded[10];
    buffer[81] = 0x00;
    std::memcpy(&buffer[82], imbe[3], 11);

    // Voice5 (17 bytes at offset 94)
    buffer[94] = 0x66;
    buffer[95] = rsEncoded[11];
    buffer[96] = rsEncoded[12];
    buffer[97] = rsEncoded[13];
    buffer[98] = 0x00;
    std::memcpy(&buffer[99], imbe[4], 11);

    // Voice6 (17 bytes at offset 111)
    buffer[111] = 0x67;
    buffer[112] = rsEncoded[14];
    buffer[113] = rsEncoded[15];
    buffer[114] = rsEncoded[16];
    buffer[115] = 0x00;
    std::memcpy(&buffer[116], imbe[5], 11);

    // Voice7 (17 bytes at offset 128)
    buffer[128] = 0x68;
    buffer[129] = rsEncoded[17];
    buffer[130] = rsEncoded[18];
    buffer[131] = rsEncoded[19];
    buffer[132] = 0x00;
    std::memcpy(&buffer[133], imbe[6], 11);

    // Voice8 (17 bytes at offset 145)
    buffer[145] = 0x69;
    buffer[146] = rsEncoded[20];
    buffer[147] = rsEncoded[21];
    buffer[148] = rsEncoded[22];
    buffer[149] = 0x00;
    std::memcpy(&buffer[150], imbe[7], 11);

    // Voice9 (16 bytes at offset 162)
    buffer[162] = 0x6A;
    buffer[163] = 0x00;  // LSD byte 1
    buffer[164] = 0x00;  // LSD byte 2
    buffer[165] = 0x00;
    std::memcpy(&buffer[166], imbe[8], 11);

    // LDU1 trailer bytes [180-200]
    if (firstLDU) {
        buffer[180] = 0x01;  // HDU_VALID flag - signals new call
        buffer[181] = 0x80;  // Algorithm ID (0x80 = unencrypted)
    }
}

void P25Utils::buildLDU2(uint8_t* buffer, const uint8_t imbe[9][IMBE_FRAME_SIZE],
                          uint32_t srcId, uint32_t dstId,
                          uint32_t wacn, uint16_t sysId) {
    std::memset(buffer, 0x00, P25_LDU2_LENGTH);

    // P25 message header (24 bytes)
    buildP25Header(buffer, P25_DUID_LDU2, srcId, dstId, wacn, sysId, 0xB2);

    // Voice10 (22 bytes at offset 24)
    buffer[24] = 0x6B;
    // MI bytes (zeros for unencrypted)
    buffer[25] = 0x00;
    buffer[26] = 0x00;
    buffer[27] = 0x00;
    buffer[28] = 0x00;
    buffer[29] = 0x00;
    buffer[30] = 0x00;  // RSSI
    buffer[31] = 0x00;
    buffer[32] = 0x00;
    buffer[33] = 0x00;
    std::memcpy(&buffer[34], imbe[0], 11);

    // Voice11 (14 bytes at offset 46)
    buffer[46] = 0x6C;
    std::memcpy(&buffer[47], imbe[1], 11);

    // Voice12 (17 bytes at offset 60)
    buffer[60] = 0x6D;
    buffer[61] = 0x00;
    buffer[62] = 0x00;
    buffer[63] = 0x00;
    buffer[64] = 0x00;
    std::memcpy(&buffer[65], imbe[2], 11);

    // Voice13 (17 bytes at offset 77)
    buffer[77] = 0x6E;
    buffer[78] = 0x00;
    buffer[79] = 0x00;
    buffer[80] = 0x00;
    buffer[81] = 0x00;
    std::memcpy(&buffer[82], imbe[3], 11);

    // Voice14 (17 bytes at offset 94)
    buffer[94] = 0x6F;
    buffer[95] = 0x00;
    buffer[96] = 0x00;
    buffer[97] = 0x00;
    buffer[98] = 0x00;
    std::memcpy(&buffer[99], imbe[4], 11);

    // Voice15 (17 bytes at offset 111) - AlgId and KId
    buffer[111] = 0x70;
    buffer[112] = 0x80;  // AlgId (0x80 = unencrypted)
    buffer[113] = 0x00;  // KId MSB
    buffer[114] = 0x00;  // KId LSB
    buffer[115] = 0x00;
    std::memcpy(&buffer[116], imbe[5], 11);

    // Voice16 (17 bytes at offset 128) - RS FEC
    buffer[128] = 0x71;
    buffer[129] = 0xAC;  // RS parity for unencrypted ESS
    buffer[130] = 0xB8;
    buffer[131] = 0xA4;
    buffer[132] = 0x00;
    std::memcpy(&buffer[133], imbe[6], 11);

    // Voice17 (17 bytes at offset 145) - RS FEC
    buffer[145] = 0x72;
    buffer[146] = 0x9B;  // RS parity for unencrypted ESS
    buffer[147] = 0xDC;
    buffer[148] = 0x75;
    buffer[149] = 0x00;
    std::memcpy(&buffer[150], imbe[7], 11);

    // Voice18 (16 bytes at offset 162)
    buffer[162] = 0x73;
    buffer[163] = 0x00;  // LSD byte 1
    buffer[164] = 0x00;  // LSD byte 2
    buffer[165] = 0x00;
    std::memcpy(&buffer[166], imbe[8], 11);

    // Frame type at byte 180
    buffer[180] = 0x00;  // DATA_UNIT
}

void P25Utils::buildTDU(uint8_t* buffer, uint32_t srcId, uint32_t dstId,
                         uint32_t wacn, uint16_t sysId, bool grantDemand) {
    std::memset(buffer, 0x00, P25_TDU_LENGTH);
    buildP25Header(buffer, P25_DUID_TDU, srcId, dstId, wacn, sysId, P25_TDU_LENGTH);

    if (grantDemand) {
        buffer[14] = NET_CTRL_GRANT_DEMAND;
    } else {
        buffer[4] = P25_LCO_CALL_TERM;
    }
}

bool P25Utils::parseOP25Packet(const uint8_t* data, size_t len, OP25Packet& packet) {
    if (len < OP25_PACKET_SIZE) {
        return false;
    }

    // Check magic bytes
    packet.magic = ((uint16_t)data[0] << 8) | data[1];
    if (packet.magic != OP25_MAGIC) {
        return false;
    }

    // Parse fields (all big-endian)
    packet.nac = ((uint16_t)data[2] << 8) | data[3];
    packet.talkgroup = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                       ((uint32_t)data[6] << 8) | data[7];
    packet.sourceId = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) |
                      ((uint32_t)data[10] << 8) | data[11];
    packet.frameType = data[12];
    packet.voiceIndex = data[13];
    packet.flags = data[14];
    packet.reserved = data[15];

    std::memcpy(packet.imbe, data + 16, 11);

    return true;
}

} // namespace op25gateway
