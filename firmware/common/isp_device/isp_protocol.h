#ifndef ISP_PROTOCOL_H
#define ISP_PROTOCOL_H

#include <limits.h>
typedef char IspByteWidth[(CHAR_BIT == 8) ? 1 : -1];
typedef char IspWordWidth[(sizeof(unsigned long) == 4U) ? 1 : -1];

/* Packet/state storage is always data RAM, not far ROM. Explicit qualifiers
 * keep the RL78 implementation compact without truncating Flash addresses.
 */
#if defined(__CCRL__)
#define ISP_RAM __near
#else
#define ISP_RAM
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* IS / version / type / LE16 sequence / command / status / LE16 length.
 * A CRC32/ISO-HDLC covers the ten header bytes and the payload; its LE32
 * trailer is not included in that CRC. No printf text belongs on this link.
 */
#define ISP_PAYLOAD_MAX       96U
#define ISP_HEADER_SIZE       10U
#define ISP_FRAME_MAX         (ISP_HEADER_SIZE + ISP_PAYLOAD_MAX + 4U)
#define ISP_MODE_BOOT         0U
#define ISP_MODE_APP          1U
#define ISP_MODE_TRANSFER     2U
#define ISP_CAP_UPDATE        0x01U
#define ISP_CAP_VERIFY        0x02U
#define ISP_CAP_READ_METADATA 0x04U
#define ISP_CAP_ENTER         0x08U
#define ISP_CAP_TRANSFER      0x10U
#define ISP_CAP_APP_READY     0x20U
#define ISP_CAP_DIAGNOSTICS   0x40U
#define ISP_DIAGNOSTIC_BYTES  24U
#define ISP_CMD_INFO          0x01U
#define ISP_CMD_ENTER         0x02U
#define ISP_CMD_METADATA      0x03U
#define ISP_CMD_START         0x04U
#define ISP_CMD_WRITE         0x05U
#define ISP_CMD_END           0x06U
#define ISP_CMD_ABORT         0x07U
#define ISP_CMD_READ_METADATA 0x08U
#define ISP_CMD_VERIFY        0x09U
#define ISP_CMD_FORCE_START   0x0EU
#define ISP_CMD_PING          0x20U
#define ISP_CMD_ECHO          0x21U
#define ISP_CMD_STATS         0x22U
#define ISP_CMD_CLEAR_STATS   0x23U
#define ISP_CMD_STREAM_TEST   0x24U
#define ISP_CMD_DIAGNOSTICS   0x25U
#define ISP_OK                0U
#define ISP_BAD_COMMAND       1U
#define ISP_BAD_SIZE          2U
#define ISP_BAD_STATE         3U
#define ISP_BAD_SEQUENCE      4U
#define ISP_BAD_METADATA      5U
#define ISP_BAD_VERSION       6U
#define ISP_FLASH_ERROR       7U
#define ISP_CRC_ERROR         8U
#define ISP_ACTION_NONE       0U
#define ISP_ACTION_ENTER      1U
#define ISP_ACTION_RESET      2U
#define ISP_ACTION_STREAM_TEST 3U

typedef struct {
    unsigned short sequence;
    unsigned char command;
    unsigned char status;
    unsigned char response;
    unsigned short length;
    unsigned char data[ISP_PAYLOAD_MAX];
} IspPacket;
typedef char IspPacketCopyBound[(sizeof(IspPacket) <= 128U) ? 1 : -1];

typedef struct {
#if defined(ISP_XMODEM)
    /* Framed control traffic is at most110 bytes, including its CRC. */
    unsigned char count;
#else
    unsigned short count;
#endif
    unsigned char bytes[ISP_FRAME_MAX];
    unsigned long crc_errors;
    unsigned long framing_errors;
} IspStream;

unsigned long IspRead32(const unsigned char ISP_RAM *p);
void IspWrite32(unsigned char ISP_RAM *p, unsigned long value);
/* Exact equality for RAM payloads (0..128 bytes), not ROM/string ordering. */
unsigned char IspBytesEqual(const unsigned char ISP_RAM *a,
    const unsigned char ISP_RAM *b, unsigned char size);
void IspBytesCopy(unsigned char ISP_RAM *to,
    const unsigned char ISP_RAM *from, unsigned char size);
unsigned long IspCrc32(const unsigned char ISP_RAM *data, unsigned short size);
unsigned short IspEncode(const IspPacket ISP_RAM *packet, unsigned char ISP_RAM *output);
/* A STOP-delimited transport already supplies a complete candidate frame. */
unsigned char IspDecode(const unsigned char ISP_RAM *bytes, unsigned short size, IspPacket ISP_RAM *packet);
void IspStreamInit(IspStream ISP_RAM *stream);
/* Feed one byte. Returns 1 only for a complete CRC-valid packet. The caller
 * owns inter-byte timeout and clears a partial packet after a bounded idle.
 * A malformed candidate retains its suffix for overlapping-SOF recovery.
 */
unsigned char IspStreamFeed(IspStream ISP_RAM *stream, unsigned char value,
                            IspPacket ISP_RAM *packet);

#ifdef __cplusplus
}
#endif

#endif
