#include "host_link_protocol.h"

#include <algorithm>

namespace firmware_upgrade::host_link {

namespace {

constexpr std::size_t kMinimumFrameSize = kHeaderSize + kPayloadCrcSize;

std::uint16_t ReadLe16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8));
}

void WriteLe16(std::uint8_t* data, std::uint16_t value) {
    data[0] = static_cast<std::uint8_t>(value & 0xFFU);
    data[1] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
}

bool IsValidFrameType(FrameType type) {
    return type == FrameType::Request || type == FrameType::Response ||
           type == FrameType::Event;
}

bool IsValidStatus(Status status) {
    return static_cast<std::uint8_t>(status) <=
           static_cast<std::uint8_t>(Status::I2cBusError);
}

bool IsValidTypeAndStatus(FrameType type, Status status) {
    if (!IsValidFrameType(type) || !IsValidStatus(status)) {
        return false;
    }
    if ((type == FrameType::Request || type == FrameType::Event) &&
        status != Status::Ok) {
        return false;
    }
    return true;
}

} // namespace

std::uint16_t Crc16CcittFalse(const std::uint8_t* data,
                              std::size_t length) {
    std::uint16_t crc = 0xFFFFU;

    if (data == nullptr && length != 0U) {
        return 0U;
    }

    for (std::size_t index = 0; index < length; ++index) {
        crc ^= static_cast<std::uint16_t>(data[index]) << 8;
        for (unsigned bit = 0; bit < 8U; ++bit) {
            if ((crc & 0x8000U) != 0U) {
                crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021U);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

bool EncodeFrame(const Frame& frame, std::vector<std::uint8_t>* encoded) {
    std::size_t payload_length;
    std::uint16_t crc;

    if (encoded == nullptr) {
        return false;
    }
    encoded->clear();

    payload_length = frame.payload.size();
    if (payload_length > kMaximumPayloadSize ||
        !IsValidTypeAndStatus(frame.type, frame.status)) {
        return false;
    }

    encoded->assign(kHeaderSize + payload_length + kPayloadCrcSize, 0U);
    (*encoded)[0] = kSof0;
    (*encoded)[1] = kSof1;
    (*encoded)[2] = kVersion;
    (*encoded)[3] = static_cast<std::uint8_t>(frame.type);
    WriteLe16(&(*encoded)[4], frame.sequence);
    (*encoded)[6] = frame.command;
    (*encoded)[7] = static_cast<std::uint8_t>(frame.status);
    WriteLe16(&(*encoded)[8], static_cast<std::uint16_t>(payload_length));

    crc = Crc16CcittFalse(&(*encoded)[2], 8U);
    WriteLe16(&(*encoded)[10], crc);

    if (payload_length != 0U) {
        std::copy(frame.payload.begin(), frame.payload.end(),
                  encoded->begin() + static_cast<std::ptrdiff_t>(kHeaderSize));
    }
    crc = Crc16CcittFalse(
        payload_length == 0U ? nullptr : frame.payload.data(), payload_length);
    WriteLe16(&(*encoded)[kHeaderSize + payload_length], crc);
    return true;
}

DecodeResult DecodeFrame(const std::uint8_t* bytes,
                         std::size_t length,
                         Frame* frame) {
    FrameType type;
    Status status;
    std::uint16_t payload_length;
    std::size_t expected_length;
    std::uint16_t received_crc;
    std::uint16_t calculated_crc;

    if (bytes == nullptr || frame == nullptr || length < kMinimumFrameSize) {
        return DecodeResult::TooShort;
    }
    if (bytes[0] != kSof0 || bytes[1] != kSof1) {
        return DecodeResult::BadSof;
    }
    if (bytes[2] != kVersion) {
        return DecodeResult::UnsupportedVersion;
    }

    type = static_cast<FrameType>(bytes[3]);
    if (!IsValidFrameType(type)) {
        return DecodeResult::InvalidFrameType;
    }
    status = static_cast<Status>(bytes[7]);
    if (!IsValidTypeAndStatus(type, status)) {
        return DecodeResult::InvalidStatus;
    }

    received_crc = ReadLe16(&bytes[10]);
    calculated_crc = Crc16CcittFalse(&bytes[2], 8U);
    if (received_crc != calculated_crc) {
        return DecodeResult::HeaderCrcError;
    }

    payload_length = ReadLe16(&bytes[8]);
    if (payload_length > kMaximumPayloadSize) {
        return DecodeResult::InvalidLength;
    }
    expected_length = kHeaderSize + payload_length + kPayloadCrcSize;
    if (length != expected_length) {
        return DecodeResult::SizeMismatch;
    }

    received_crc = ReadLe16(&bytes[kHeaderSize + payload_length]);
    calculated_crc = Crc16CcittFalse(
        payload_length == 0U ? nullptr : &bytes[kHeaderSize], payload_length);
    if (received_crc != calculated_crc) {
        return DecodeResult::PayloadCrcError;
    }

    frame->type = type;
    frame->sequence = ReadLe16(&bytes[4]);
    frame->command = bytes[6];
    frame->status = status;
    frame->payload.assign(
        bytes + kHeaderSize,
        bytes + kHeaderSize + static_cast<std::size_t>(payload_length));
    return DecodeResult::Ok;
}

void FrameStreamParser::Push(const std::uint8_t* data, std::size_t length) {
    std::size_t overflow;

    if (data == nullptr || length == 0U) {
        return;
    }

    if (length >= kStreamBufferCapacity) {
        stats_.input_overflow_bytes += buffer_.size() +
                                       length - kStreamBufferCapacity;
        buffer_.assign(data + (length - kStreamBufferCapacity), data + length);
        return;
    }

    if (buffer_.size() + length > kStreamBufferCapacity) {
        overflow = buffer_.size() + length - kStreamBufferCapacity;
        DiscardFront(overflow);
        stats_.input_overflow_bytes += overflow;
    }
    buffer_.insert(buffer_.end(), data, data + length);
}

bool FrameStreamParser::TryPop(Frame* frame) {
    std::size_t sof_index;
    std::uint16_t payload_length;
    std::size_t frame_length;
    DecodeResult result;

    if (frame == nullptr) {
        return false;
    }

    for (;;) {
        if (buffer_.size() < 2U) {
            return false;
        }

        sof_index = 0U;
        while (sof_index + 1U < buffer_.size() &&
               (buffer_[sof_index] != kSof0 ||
                buffer_[sof_index + 1U] != kSof1)) {
            ++sof_index;
        }
        if (sof_index + 1U >= buffer_.size()) {
            if (buffer_.back() == kSof0) {
                stats_.discarded_bytes += buffer_.size() - 1U;
                buffer_.erase(buffer_.begin(), buffer_.end() - 1);
            } else {
                stats_.discarded_bytes += buffer_.size();
                buffer_.clear();
            }
            return false;
        }
        if (sof_index != 0U) {
            DiscardFront(sof_index);
            stats_.discarded_bytes += sof_index;
        }

        if (buffer_.size() < kHeaderSize) {
            return false;
        }

        if (buffer_[2] != kVersion ||
            !IsValidFrameType(static_cast<FrameType>(buffer_[3])) ||
            !IsValidTypeAndStatus(static_cast<FrameType>(buffer_[3]),
                                  static_cast<Status>(buffer_[7])) ||
            ReadLe16(&buffer_[10]) != Crc16CcittFalse(&buffer_[2], 8U)) {
            DiscardFront(1U);
            ++stats_.invalid_headers;
            continue;
        }

        payload_length = ReadLe16(&buffer_[8]);
        if (payload_length > kMaximumPayloadSize) {
            DiscardFront(1U);
            ++stats_.invalid_headers;
            continue;
        }

        frame_length = kHeaderSize + payload_length + kPayloadCrcSize;
        if (buffer_.size() < frame_length) {
            return false;
        }

        result = DecodeFrame(buffer_.data(), frame_length, frame);
        if (result == DecodeResult::Ok) {
            DiscardFront(frame_length);
            return true;
        }

        DiscardFront(1U);
        if (result == DecodeResult::PayloadCrcError) {
            ++stats_.invalid_payloads;
        } else {
            ++stats_.invalid_headers;
        }
    }
}

void FrameStreamParser::Clear() {
    buffer_.clear();
    stats_ = {};
}

std::size_t FrameStreamParser::BufferedSize() const {
    return buffer_.size();
}

const StreamParserStats& FrameStreamParser::Stats() const {
    return stats_;
}

void FrameStreamParser::DiscardFront(std::size_t count) {
    if (count >= buffer_.size()) {
        buffer_.clear();
    } else if (count != 0U) {
        buffer_.erase(buffer_.begin(),
                      buffer_.begin() + static_cast<std::ptrdiff_t>(count));
    }
}

} // namespace firmware_upgrade::host_link
