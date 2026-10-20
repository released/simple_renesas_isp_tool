#include "f13_transfer_profile.h"

#include <algorithm>
#include <vector>

namespace firmware_upgrade::f13_transfer {
namespace {

constexpr std::uint8_t kRequestMagic = 0xA5U;
constexpr std::uint8_t kResponseMagic = 0x5AU;

bool IsRequestCommand(Command command) {
    const auto value = static_cast<std::uint8_t>(command);
    return value >= static_cast<std::uint8_t>(Command::DeviceInfo) &&
           value <= static_cast<std::uint8_t>(Command::ClearStats);
}

bool IsKnownError(std::uint8_t value) {
    return value >= static_cast<std::uint8_t>(ErrorStatus::BadFormat) &&
           value <= static_cast<std::uint8_t>(ErrorStatus::BadArgument);
}

bool Fail(const wchar_t* message, std::wstring* error) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

} // namespace

std::uint8_t Crc8Atm(const std::uint8_t* data, std::size_t length) {
    std::uint8_t crc = 0U;
    for (std::size_t byte_index = 0; byte_index < length; ++byte_index) {
        crc ^= data[byte_index];
        for (std::uint8_t bit_index = 0U; bit_index < 8U; ++bit_index) {
            crc = (crc & 0x80U) != 0U
                      ? static_cast<std::uint8_t>((crc << 1U) ^ 0x07U)
                      : static_cast<std::uint8_t>(crc << 1U);
        }
    }
    return crc;
}

bool BuildRequest(std::uint8_t sequence,
                  Command command,
                  const std::array<std::uint8_t, 3>& payload,
                  can_host::Frame* frame,
                  std::wstring* error) {
    if (frame == nullptr) {
        return Fail(L"Custom Transfer request output is null.", error);
    }
    if (sequence == 0U) {
        return Fail(L"Custom Transfer sequence zero is reserved.", error);
    }
    if (!IsRequestCommand(command)) {
        return Fail(L"Custom Transfer command is not a request command.", error);
    }

    std::vector<std::uint8_t> bytes(kFrameSize, 0U);
    bytes[0] = kRequestMagic;
    bytes[1] = kProtocolMajor;
    bytes[2] = sequence;
    bytes[3] = static_cast<std::uint8_t>(command);
    std::copy(payload.begin(), payload.end(), bytes.begin() + 4);
    bytes[7] = Crc8Atm(bytes.data(), 7U);

    *frame = can_host::Frame{};
    frame->id = kRequestId;
    frame->dlc = static_cast<std::uint8_t>(kFrameSize);
    frame->data = std::move(bytes);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool ParseResponse(std::uint8_t expected_sequence,
                   Command expected_command,
                   const can_host::Frame& frame,
                   Response* response,
                   std::wstring* error) {
    if (response == nullptr) {
        return Fail(L"Custom Transfer response output is null.", error);
    }
    if (expected_sequence == 0U || !IsRequestCommand(expected_command)) {
        return Fail(L"Custom Transfer expected request is invalid.", error);
    }
    if (frame.extended || frame.remote || frame.fd || frame.bit_rate_switch ||
        frame.id != kResponseId || frame.dlc != kFrameSize ||
        frame.data.size() != kFrameSize) {
        return Fail(L"Custom Transfer response CAN frame is invalid.", error);
    }
    if (frame.data[0] != kResponseMagic || frame.data[1] != kProtocolMajor) {
        return Fail(L"Custom Transfer response identity is invalid.", error);
    }
    if (frame.data[2] != expected_sequence) {
        return Fail(L"Custom Transfer response sequence does not match.", error);
    }
    if (Crc8Atm(frame.data.data(), 7U) != frame.data[7]) {
        return Fail(L"Custom Transfer response CRC-8 is invalid.", error);
    }

    Response parsed;
    parsed.sequence = frame.data[2];
    parsed.command = static_cast<Command>(frame.data[3]);
    parsed.payload = {frame.data[4], frame.data[5], frame.data[6]};
    if (parsed.command == Command::ErrorResponse) {
        if (frame.data[4] != static_cast<std::uint8_t>(expected_command) ||
            !IsKnownError(frame.data[5])) {
            return Fail(L"Custom Transfer error response is invalid.", error);
        }
        parsed.error_status = static_cast<ErrorStatus>(frame.data[5]);
        parsed.error_detail = frame.data[6];
    } else if (parsed.command != expected_command) {
        return Fail(L"Custom Transfer response command does not match.", error);
    }

    *response = parsed;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool DeviceInfoAssembler::AddPage(const Response& response,
                                  std::wstring* error) {
    if (response.command != Command::DeviceInfo ||
        response.error_status != ErrorStatus::None) {
        return Fail(L"Custom Transfer DEVICE_INFO response is invalid.", error);
    }

    const std::uint8_t page = response.payload[0];
    if (page >= pages_.size()) {
        return Fail(L"Custom Transfer DEVICE_INFO page is out of range.", error);
    }

    switch (page) {
        case 0U:
            if (response.payload[1] != kProtocolMajor ||
                response.payload[2] != kProtocolMinor) {
                return Fail(L"Custom Transfer protocol version is incompatible.",
                            error);
            }
            value_.protocol_major = response.payload[1];
            value_.protocol_minor = response.payload[2];
            break;
        case 1U: {
            value_.target_id = static_cast<std::uint16_t>(response.payload[1]) |
                (static_cast<std::uint16_t>(response.payload[2]) << 8U);
            if (value_.target_id == 0U || value_.target_id == 0xFFFFU) {
                return Fail(L"Custom Transfer target identity is invalid.",
                            error);
            }
            break;
        }
        case 2U:
        case 3U:
        case 4U:
        case 5U: {
            const std::size_t offset = static_cast<std::size_t>(page - 2U) * 2U;
            for (std::size_t index = 1U; index <= 2U; ++index) {
                const std::uint8_t character = response.payload[index];
                if (character != 0U &&
                    (character < 0x20U || character > 0x7EU)) {
                    return Fail(
                        L"Custom Transfer device label contains invalid ASCII.",
                        error);
                }
            }
            part_number_[offset] = static_cast<char>(response.payload[1]);
            part_number_[offset + 1U] = static_cast<char>(response.payload[2]);
            break;
        }
        case 6U:
            value_.capabilities =
                static_cast<std::uint16_t>(response.payload[1]) |
                (static_cast<std::uint16_t>(response.payload[2]) << 8U);
            if ((value_.capabilities & 0x003FU) != 0x003FU) {
                return Fail(L"Custom Transfer command capabilities are incomplete.",
                            error);
            }
            break;
        default:
            return Fail(L"Custom Transfer DEVICE_INFO page is unsupported.",
                        error);
    }

    pages_[page] = true;
    if (IsComplete()) {
        const auto label_end = std::find(
            part_number_.begin(), part_number_.end(), '\0');
        if (label_end == part_number_.begin()) {
            return Fail(L"Custom Transfer device label is empty.", error);
        }
        if (std::any_of(label_end, part_number_.end(),
                        [](char value) { return value != '\0'; })) {
            return Fail(
                L"Custom Transfer device label padding is invalid.", error);
        }
        value_.part_number.assign(part_number_.begin(), label_end);
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool DeviceInfoAssembler::IsComplete() const {
    return std::all_of(pages_.begin(), pages_.end(),
                       [](bool present) { return present; });
}

const DeviceInfo& DeviceInfoAssembler::Value() const {
    return value_;
}

void Client::SetCanClient(can_host::Client* client) {
    client_ = client;
}

void Client::SetTransactionPolicy(std::uint16_t timeout_ms,
                                  std::uint8_t retry_count) {
    if (timeout_ms != 0U && timeout_ms <= 30000U && retry_count <= 3U) {
        timeout_ms_ = timeout_ms;
        retry_count_ = retry_count;
    }
}

std::uint8_t Client::NextSequence() {
    ++last_sequence_;
    if (last_sequence_ == 0U) {
        ++last_sequence_;
    }
    return last_sequence_;
}

bool Client::Execute(Command command,
                     const std::array<std::uint8_t, 3>& payload,
                     Response* response,
                     std::wstring* error) {
    if (client_ == nullptr) {
        return Fail(L"Custom Transfer CAN client is not configured.", error);
    }
    if (response == nullptr) {
        return Fail(L"Custom Transfer response output is null.", error);
    }

    const std::uint8_t sequence = NextSequence();
    can_host::Frame request;
    can_host::Frame received;
    if (!BuildRequest(sequence, command, payload, &request, error)) {
        return false;
    }

    if (!Exchange(request, static_cast<std::uint8_t>(kFrameSize), &received, error) ||
        !ParseResponse(sequence, command, received, response, error)) {
        return false;
    }

    if (response->error_status != ErrorStatus::None) {
        if (error != nullptr) {
            *error = L"Custom Transfer target error status " +
                     std::to_wstring(
                         static_cast<std::uint8_t>(response->error_status)) +
                     L", detail " +
                     std::to_wstring(response->error_detail) + L".";
        }
        return false;
    }
    return true;
}

bool Client::Exchange(can_host::Frame request, std::uint8_t expected_dlc,
                      can_host::Frame* received, std::wstring* error) {
    if (!client_ || !received)
        return Fail(L"Custom Transfer CAN client/output is not configured.", error);
    request.fd = request.bit_rate_switch = fd_;
    can_host::ReceiveMatch match;
    match.timeout_ms = timeout_ms_;
    match.retry_count = retry_count_;
    match.expected_dlc = expected_dlc;
    match.id = kResponseId;
    match.id_mask = kResponseMask;
    match.flags_value = fd_ ? 0x0CU : 0U;
    match.flags_mask = fd_ ? 0x0FU : 0x07U;
    if (!client_->TransmitWaitReceive(request, match, received, error)) return false;
    const auto length = expected_dlc == 15U ? 64U : expected_dlc;
    if (received->extended || received->remote || received->id != kResponseId ||
        received->fd != fd_ || received->bit_rate_switch != fd_ ||
        received->dlc != expected_dlc || received->data.size() != length)
        return Fail(L"Custom Transfer response ID, mode, BRS or length mismatch.", error);
    // Parse the shared v1 payload only after strict session-mode validation.
    received->fd = received->bit_rate_switch = false;
    return true;
}

bool Client::EchoFd64(const std::array<std::uint8_t, CT_FD64_PAYLOAD>& input,
                      std::wstring* error) {
    if (!fd_) return Fail(L"ECHO_FD64 requires CAN FD mode.", error);
    const auto sequence = NextSequence();
    can_host::Frame request, response;
    request.id = kRequestId;
    request.dlc = 15U;
    request.data = {kRequestMagic, kProtocolMajor, sequence, CT_FD64_COMMAND};
    request.data.insert(request.data.end(), input.begin(), input.end());
    request.data.push_back(Crc8Atm(request.data.data(), CT_FD64_SIZE - 1U));
    if (!Exchange(request, 15U, &response, error)) return false;
    if (response.data[0] != kResponseMagic || response.data[1] != kProtocolMajor ||
        response.data[2] != sequence || response.data[3] != CT_FD64_COMMAND ||
        !std::equal(input.begin(), input.end(), response.data.begin()+4U) ||
        Crc8Atm(response.data.data(), CT_FD64_SIZE-1U) != response.data.back())
        return Fail(L"Custom Transfer ECHO_FD64 identity, sequence, data or CRC8 mismatch.", error);
    return true;
}

bool Client::Discover(DeviceInfo* info, std::wstring* error) {
    if (info == nullptr) {
        return Fail(L"Custom Transfer device-info output is null.", error);
    }

    DeviceInfoAssembler assembler;
    for (std::uint8_t page = 0U; page < 7U; ++page) {
        Response response;
        if (!Execute(Command::DeviceInfo, {page, 0U, 0U},
                     &response, error) ||
            !assembler.AddPage(response, error)) {
            return false;
        }
    }
    if (!assembler.IsComplete()) {
        return Fail(L"Custom Transfer device identity is incomplete.", error);
    }
    *info = assembler.Value();
    return true;
}

bool Client::Ping(const std::array<std::uint8_t, 3>& token,
                  std::wstring* error) {
    Response response;
    if (!Execute(Command::Ping, token, &response, error)) {
        return false;
    }
    if (response.payload != token) {
        return Fail(L"Custom Transfer PING token does not match.", error);
    }
    return true;
}

bool Client::Echo(const std::array<std::uint8_t, 3>& input,
                  std::array<std::uint8_t, 3>* output,
                  std::wstring* error) {
    Response response;
    if (output == nullptr) {
        return Fail(L"Custom Transfer ECHO output is null.", error);
    }
    if (!Execute(Command::Echo, input, &response, error)) {
        return false;
    }
    *output = response.payload;
    if (*output != input) {
        return Fail(L"Custom Transfer ECHO data does not match.", error);
    }
    return true;
}

bool Client::PatternTest(const std::array<std::uint8_t, 3>& input,
                         std::array<std::uint8_t, 3>* output,
                         std::wstring* error) {
    Response response;
    std::array<std::uint8_t, 3> expected;
    if (output == nullptr) {
        return Fail(L"Custom Transfer pattern output is null.", error);
    }
    if (!Execute(Command::PatternTest, input, &response, error)) {
        return false;
    }
    expected = {
        static_cast<std::uint8_t>(input[0] ^ 0xA5U),
        static_cast<std::uint8_t>(input[1] + 0x3DU),
        static_cast<std::uint8_t>((input[2] << 1U) | (input[2] >> 7U)),
    };
    *output = response.payload;
    if (*output != expected) {
        return Fail(L"Custom Transfer pattern result does not match.", error);
    }
    return true;
}

bool Client::GetStat(std::uint8_t index,
                     std::uint16_t* value,
                     std::wstring* error) {
    Response response;
    if (value == nullptr) {
        return Fail(L"Custom Transfer statistic output is null.", error);
    }
    if (index > 11U) {
        return Fail(L"Custom Transfer statistic index is out of range.", error);
    }
    if (!Execute(Command::GetStats, {index, 0U, 0U}, &response, error)) {
        return false;
    }
    if (response.payload[0] != index) {
        return Fail(L"Custom Transfer statistic response is invalid.", error);
    }
    *value = static_cast<std::uint16_t>(response.payload[1]) |
             (static_cast<std::uint16_t>(response.payload[2]) << 8U);
    return true;
}

bool Client::ClearStats(std::wstring* error) {
    Response response;
    const std::array<std::uint8_t, 3> key{{0xC3U, 0x3CU, 0U}};
    if (!Execute(Command::ClearStats, key, &response, error)) {
        return false;
    }
    if (response.payload != key) {
        return Fail(L"Custom Transfer CLEAR_STATS response is invalid.", error);
    }
    return true;
}

} // namespace firmware_upgrade::f13_transfer
