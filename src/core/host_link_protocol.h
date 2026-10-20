#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "../../firmware/common/host_link_limits.h"
#include "../../firmware/common/can_burst.h"

namespace firmware_upgrade::host_link {

constexpr std::uint8_t kSof0 = 0x55;
constexpr std::uint8_t kSof1 = 0xAA;
constexpr std::uint8_t kVersion = 0x02;
constexpr std::uint32_t kCanBurstFeature = CB_FEATURE;
constexpr std::uint32_t kCanFdBurstFeature = CB_FD_FEATURE;
constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kPayloadCrcSize = 2;
constexpr std::size_t kMaximumPayloadSize = HOST_WIRE_MAX_PAYLOAD;
constexpr std::size_t kMaximumFrameSize =
    kHeaderSize + kMaximumPayloadSize + kPayloadCrcSize;
constexpr std::size_t kStreamBufferCapacity = 512;

constexpr std::uint16_t kCanChannelSourceBusError = 0x0001U;
constexpr std::uint16_t kCanChannelSourceErrorWarning = 0x0002U;
constexpr std::uint16_t kCanChannelSourceErrorPassive = 0x0004U;
constexpr std::uint16_t kCanChannelSourceBusOffEntry = 0x0008U;
constexpr std::uint16_t kCanChannelSourceBusOffRecovery = 0x0010U;
constexpr std::uint16_t kCanChannelSourceOverload = 0x0020U;
constexpr std::uint16_t kCanChannelSourceBusLock = 0x0040U;
constexpr std::uint16_t kCanChannelSourceArbitrationLost = 0x0080U;
constexpr std::uint16_t kCanChannelSourceTdcViolation = 0x0100U;
constexpr std::uint16_t kCanChannelSourceKnownMask = 0x01FFU;

constexpr std::uint16_t kCanGlobalSourceDlcError = 0x0001U;
constexpr std::uint16_t kCanGlobalSourceMessageLost = 0x0002U;
constexpr std::uint16_t kCanGlobalSourceTxHistoryLost = 0x0004U;
constexpr std::uint16_t kCanGlobalSourcePayloadOverflow = 0x0008U;
constexpr std::uint16_t kCanGlobalSourceRxFifo0Lost = 0x0010U;
constexpr std::uint16_t kCanGlobalSourceRxFifo1Lost = 0x0020U;
constexpr std::uint16_t kCanGlobalSourceCommonFifoLost = 0x0100U;
constexpr std::uint16_t kCanGlobalSourceKnownMask = 0x013FU;

enum class FrameType : std::uint8_t {
    Request = 0x10,
    Response = 0x20,
    Event = 0x30,
};

enum class Command : std::uint8_t {
    Ping = 0x00,
    GetHostInfo = 0x01,
    GetCapabilities = 0x02,
    ResetHost = 0x03,
    GetPortInfo = 0x04,
    GetLinkBaud = 0x05,
    SetLinkBaud = 0x06,
    ConfirmLinkBaud = 0x07,
    SetCanConfig = 0x10,
    SetCanFilter = 0x11,
    CanStart = 0x12,
    CanStop = 0x13,
    SelectCanPort = 0x14,
    CanTx = 0x20,
    CanTxWaitRx = 0x21,
    CanBurstWaitRx = 0x22,
    GetStats = 0x30,
    ClearStats = 0x31,
    SetI2cConfig = 0x40,
    I2cTransaction = 0x41,
    CanRxEvent = 0x80,
    CanStateEvent = 0x81,
    HostErrorEvent = 0x82,
};

enum class Status : std::uint8_t {
    Ok = 0x00,
    Busy = 0x01,
    BadCommand = 0x02,
    BadArgument = 0x03,
    CrcError = 0x04,
    SequenceError = 0x05,
    Timeout = 0x06,
    CanError = 0x07,
    Overflow = 0x08,
    Incompatible = 0x09,
    BadState = 0x0A,
    InternalError = 0x0B,
    I2cNack = 0x0C,
    I2cArbitrationLost = 0x0D,
    I2cBusError = 0x0E,
};

enum class DecodeResult {
    Ok,
    TooShort,
    BadSof,
    UnsupportedVersion,
    InvalidFrameType,
    InvalidStatus,
    HeaderCrcError,
    InvalidLength,
    SizeMismatch,
    PayloadCrcError,
};

struct Frame {
    FrameType type = FrameType::Request;
    std::uint16_t sequence = 0;
    std::uint8_t command = 0;
    Status status = Status::Ok;
    std::vector<std::uint8_t> payload;
};

struct StreamParserStats {
    std::uint64_t discarded_bytes = 0;
    std::uint64_t invalid_headers = 0;
    std::uint64_t invalid_payloads = 0;
    std::uint64_t input_overflow_bytes = 0;
};

std::uint16_t Crc16CcittFalse(const std::uint8_t* data, std::size_t length);
bool EncodeFrame(const Frame& frame, std::vector<std::uint8_t>* encoded);
DecodeResult DecodeFrame(const std::uint8_t* bytes,
                         std::size_t length,
                         Frame* frame);

class FrameStreamParser {
public:
    void Push(const std::uint8_t* data, std::size_t length);
    bool TryPop(Frame* frame);
    void Clear();

    std::size_t BufferedSize() const;
    const StreamParserStats& Stats() const;

private:
    void DiscardFront(std::size_t count);

    std::vector<std::uint8_t> buffer_;
    StreamParserStats stats_{};
};

} // namespace firmware_upgrade::host_link
