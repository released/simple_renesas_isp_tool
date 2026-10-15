#include "can_host_client.h"

#include <algorithm>
#include <utility>

namespace firmware_upgrade::can_host {

namespace {

constexpr std::uint8_t kFlagIde = 0x01U;
constexpr std::uint8_t kFlagRtr = 0x02U;
constexpr std::uint8_t kFlagFdf = 0x04U;
constexpr std::uint8_t kFlagBrs = 0x08U;
constexpr std::uint8_t kFlagEsi = 0x10U;
constexpr std::uint8_t kFlagMask = 0x1FU;
constexpr std::size_t kFrameHeaderSize = 12U;

void SetError(std::wstring* error, const wchar_t* text) {
    if (error != nullptr) {
        *error = text;
    }
}

void PutLe16(std::vector<std::uint8_t>* output, std::uint16_t value) {
    output->push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
}

void PutLe32(std::vector<std::uint8_t>* output, std::uint32_t value) {
    output->push_back(static_cast<std::uint8_t>(value & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFU));
    output->push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFU));
}

std::uint16_t ReadLe16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(data[0]) |
           static_cast<std::uint16_t>(
               static_cast<std::uint16_t>(data[1]) << 8);
}

std::uint32_t ReadLe32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

bool DlcToLength(std::uint8_t dlc, std::uint8_t* length) {
    static constexpr std::uint8_t kFdLengths[] = {
        12U, 16U, 20U, 24U, 32U, 48U, 64U,
    };

    if (length == nullptr) {
        return false;
    }
    if (dlc <= 8U) {
        *length = dlc;
        return true;
    }
    if (dlc <= 15U) {
        *length = kFdLengths[dlc - 9U];
        return true;
    }
    return false;
}

std::uint8_t FrameFlags(const Frame& frame) {
    std::uint8_t flags = 0U;
    if (frame.extended) flags |= kFlagIde;
    if (frame.remote) flags |= kFlagRtr;
    if (frame.fd) flags |= kFlagFdf;
    if (frame.bit_rate_switch) flags |= kFlagBrs;
    if (frame.error_state_indicator) flags |= kFlagEsi;
    return flags;
}

bool ValidateMatch(const ReceiveMatch& match, std::wstring* error) {
    const std::uint32_t maximum_id =
        (match.flags_value & kFlagIde) != 0U ? 0x1FFFFFFFU : 0x7FFU;

    if (match.timeout_ms == 0U || match.timeout_ms > 30000U ||
        match.retry_count > 3U ||
        (match.expected_dlc != 0xFFU && match.expected_dlc > 15U) ||
        match.id_mask == 0U || match.id > maximum_id ||
        match.id_mask > maximum_id ||
        (match.flags_value & static_cast<std::uint8_t>(~kFlagMask)) != 0U ||
        (match.flags_mask & static_cast<std::uint8_t>(~kFlagMask)) != 0U) {
        SetError(error, L"Invalid CAN receive-match parameters.");
        return false;
    }
    return true;
}

} // namespace

const wchar_t* NominalPresetName(std::uint8_t preset) {
    switch (preset) {
        case 1U: return L"250K";
        case 2U: return L"500K";
        case 4U: return L"1M";
        default: return L"Unknown";
    }
}

bool NominalPresetFromText(const std::wstring& text, std::uint8_t* preset) {
    if (preset == nullptr) return false;
    for (const auto value : kNominalPresets) {
        if (text == NominalPresetName(value)) {
            *preset = value;
            return true;
        }
    }
    return false;
}

bool IsConfigurationSupported(const Configuration& configuration,
                              const host_link::Capabilities& capabilities,
                              std::wstring* error) {
    const auto preset = configuration.nominal_bitrate_preset;
    if ((preset != 1U && preset != 2U && preset != 4U) ||
        (configuration.mode != Mode::Classical && configuration.mode != Mode::CanFd) ||
        (configuration.mode == Mode::Classical && configuration.data_bitrate_preset != 0xFFU) ||
        (configuration.mode == Mode::CanFd && configuration.data_bitrate_preset != 1U)) {
        SetError(error, L"Unsupported CAN bitrate preset.");
        return false;
    }
    const std::uint32_t mode_bit = configuration.mode == Mode::Classical ? 1U : 2U;
    if ((capabilities.feature_flags & mode_bit) == 0U ||
        (capabilities.nominal_bitrate_mask & (1UL << preset)) == 0U ||
        (configuration.mode == Mode::CanFd && (capabilities.data_bitrate_mask & 2U) == 0U)) {
        SetError(error, L"The connected bridge does not advertise this CAN mode/bitrate.");
        return false;
    }
    return true;
}

void Client::SetHostLinkClient(host_link::Client* client) {
    client_ = client;
}

void Client::SetTraceCallback(TraceCallback trace_callback) {
    trace_callback_ = std::move(trace_callback);
}

bool Client::SetConfiguration(const Configuration& configuration,
                              std::wstring* error) {
    std::vector<std::uint8_t> payload;
    host_link::Response response;

    if ((configuration.mode != Mode::Classical &&
         configuration.mode != Mode::CanFd) ||
        (configuration.nominal_bitrate_preset != 1U &&
         configuration.nominal_bitrate_preset != 2U &&
         configuration.nominal_bitrate_preset != 4U) ||
        (configuration.mode == Mode::Classical &&
         configuration.data_bitrate_preset != 0xFFU) ||
        (configuration.mode == Mode::CanFd &&
         configuration.data_bitrate_preset != 1U)) {
        SetError(error, L"Unsupported CAN bitrate preset.");
        return false;
    }

    payload.push_back(static_cast<std::uint8_t>(configuration.mode));
    payload.push_back(configuration.nominal_bitrate_preset);
    payload.push_back(configuration.data_bitrate_preset);
    payload.push_back(0U);
    if (!Request(host_link::Command::SetCanConfig, payload, &response, error)) {
        if (response.status == host_link::Status::CanError &&
            response.payload.size() == 8U && response.payload[0] == 0xC1U &&
            error != nullptr) {
            static const wchar_t* stages[] = {
                L"clock validation", L"not initialized", L"X1 stabilization",
                L"main clock selection", L"PLL lock", L"PLL selection",
                L"unexpected startup clock", L"HOCO fallback"
            };
            const unsigned stage = response.payload[1];
            *error += L" Bridge CAN clock failed at " +
                std::wstring(stage < 8U ? stages[stage] : L"unknown stage") +
                L" (code " + std::to_wstring(stage) +
                L"). Check the bridge board's external oscillator/clock configuration.";
        }
        return false;
    }
    if (response.payload != payload) {
        SetError(error, L"SET_CAN_CONFIG response does not echo the request.");
        return false;
    }
    return true;
}

bool Client::SetFilter(std::uint8_t index,
                       const Filter& filter,
                       std::wstring* error) {
    const std::uint32_t maximum_id = filter.extended ? 0x1FFFFFFFU : 0x7FFU;
    std::vector<std::uint8_t> payload;
    host_link::Response response;

    if (index != 0U || filter.id > maximum_id || filter.mask > maximum_id ||
        (filter.enabled && filter.mask == 0U)) {
        SetError(error, L"Invalid CAN receive filter.");
        return false;
    }

    payload.push_back(index);
    payload.push_back(filter.enabled ? 1U : 0U);
    payload.push_back(filter.extended ? 1U : 0U);
    payload.push_back(filter.remote ? 1U : 0U);
    PutLe32(&payload, filter.id);
    PutLe32(&payload, filter.mask);
    if (!Request(host_link::Command::SetCanFilter, payload, &response, error)) {
        return false;
    }
    if (response.payload != payload) {
        SetError(error, L"SET_CAN_FILTER response does not echo the request.");
        return false;
    }
    return true;
}

bool Client::Start(std::uint32_t* controller_state, std::wstring* error) {
    host_link::Response response;

    if (controller_state == nullptr) {
        SetError(error, L"Controller-state output is null.");
        return false;
    }
    if (!Request(host_link::Command::CanStart, {}, &response, error)) {
        return false;
    }
    if (response.payload.size() != 4U) {
        SetError(error, L"CAN_START returned an invalid payload.");
        return false;
    }
    *controller_state = ReadLe32(response.payload.data());
    return true;
}

bool Client::Stop(std::uint32_t* controller_state, std::wstring* error) {
    host_link::Response response;

    if (controller_state == nullptr) {
        SetError(error, L"Controller-state output is null.");
        return false;
    }
    if (!Request(host_link::Command::CanStop, {}, &response, error)) {
        return false;
    }
    if (response.payload.size() != 4U) {
        SetError(error, L"CAN_STOP returned an invalid payload.");
        return false;
    }
    *controller_state = ReadLe32(response.payload.data());
    return true;
}

bool Client::Transmit(const Frame& frame,
                      TransmitResult* result,
                      std::wstring* error) {
    std::vector<std::uint8_t> payload;
    host_link::Response response;

    if (result == nullptr) {
        SetError(error, L"Transmit-result output is null.");
        return false;
    }
    if (!EncodeFrame(frame, true, &payload, error)) {
        return false;
    }
    if (trace_callback_) {
        trace_callback_(TraceDirection::Transmit, frame);
    }
    if (client_ == nullptr ||
        !client_->RequestWithPolicy(host_link::Command::CanTx, payload,
                                    1500U, 2U, &response, error)) {
        return false;
    }
    if (response.payload.size() != 8U && response.payload.size() != 40U) {
        SetError(error, L"CAN_TX returned an invalid payload.");
        return false;
    }
    *result = TransmitResult{};
    result->completion_timestamp = ReadLe32(&response.payload[0]);
    result->controller_result = ReadLe32(&response.payload[4]);
    if (response.payload.size() == 40U) {
        result->diagnostic_available = true;
        for (std::size_t index = 0U; index < 4U; ++index) {
            result->input_head[index] = ReadLe16(&response.payload[8U + index * 2U]);
            result->input_data[index] = ReadLe16(&response.payload[16U + index * 2U]);
            result->ram_data[index] = ReadLe16(&response.payload[30U + index * 2U]);
        }
        result->ram_id_low = ReadLe16(&response.payload[24]);
        result->ram_id_high = ReadLe16(&response.payload[26]);
        result->ram_dlc_register = response.payload[28];
        result->ram_fd_control = response.payload[29];
        result->ram_window_before = ReadLe16(&response.payload[38]);
    }
    return true;
}

bool Client::TransmitWaitReceive(const Frame& transmit_frame,
                                 const ReceiveMatch& match,
                                 Frame* received_frame,
                                 std::wstring* error) {
    std::vector<std::uint8_t> payload;
    std::vector<std::uint8_t> encoded_frame;
    host_link::Response response;
    std::uint32_t host_timeout;

    if (received_frame == nullptr) {
        SetError(error, L"Received-frame output is null.");
        return false;
    }
    if (!ValidateMatch(match, error) ||
        !EncodeFrame(transmit_frame, true, &encoded_frame, error)) {
        return false;
    }
    if (trace_callback_) {
        trace_callback_(TraceDirection::Transmit, transmit_frame);
    }

    PutLe16(&payload, match.timeout_ms);
    payload.push_back(match.retry_count);
    payload.push_back(match.expected_dlc);
    PutLe32(&payload, match.id);
    PutLe32(&payload, match.id_mask);
    payload.push_back(match.flags_value);
    payload.push_back(match.flags_mask);
    payload.push_back(0U);
    payload.push_back(0U);
    payload.insert(payload.end(), encoded_frame.begin(), encoded_frame.end());

    host_timeout = static_cast<std::uint32_t>(match.timeout_ms) *
                   (static_cast<std::uint32_t>(match.retry_count) + 1U) +
                   1500U;
    if (client_ == nullptr ||
        !client_->RequestWithPolicy(host_link::Command::CanTxWaitRx, payload,
                                    host_timeout, 2U, &response, error)) {
        return false;
    }
    if (!DecodeFrame(response.payload, received_frame, error)) {
        return false;
    }
    if (trace_callback_) {
        trace_callback_(TraceDirection::Receive, *received_frame);
    }
    return true;
}

bool Client::BurstWaitReceive(const std::vector<Frame>& frames,
                              const ReceiveMatch& match,
                              std::uint16_t gap_ms,
                              Frame* received_frame,
                              std::wstring* error,
                              host_link::Status* completion_status) {
    if (completion_status) *completion_status = host_link::Status::InternalError;
    if (!client_ || !received_frame || frames.empty() || frames.size() > 10U ||
        !match.timeout_ms || match.timeout_ms > 30000U || match.retry_count ||
        match.id > 0x7FFU || !match.id_mask || match.id_mask > 0x7FFU ||
        match.expected_dlc > 8U || match.flags_value || match.flags_mask != 0x0FU || gap_ms > 10U) {
        SetError(error, L"Invalid Classical standard CAN burst/match.");
        return false;
    }
    std::vector<std::uint8_t> payload{1U, static_cast<std::uint8_t>(frames.size())};
    PutLe16(&payload, match.timeout_ms);
    PutLe16(&payload, static_cast<std::uint16_t>(match.id));
    PutLe16(&payload, static_cast<std::uint16_t>(match.id_mask));
    payload.push_back(match.expected_dlc);
    payload.push_back(0U);
    PutLe16(&payload, gap_ms);
    for (const auto& frame : frames) {
        std::vector<std::uint8_t> encoded;
        if (!EncodeFrame(frame, true, &encoded, error) || FrameFlags(frame) != 0U) {
            SetError(error, L"Burst supports Classical standard data frames only.");
            return false;
        }
        PutLe16(&payload, static_cast<std::uint16_t>(frame.id));
        payload.push_back(frame.dlc);
        payload.insert(payload.end(), frame.data.begin(), frame.data.end());
    }
    if (payload.size() > host_link::kMaximumPayloadSize) {
        SetError(error, L"CAN burst exceeds Host Link payload limit.");
        return false;
    }
    for (const auto& frame : frames) if (trace_callback_) trace_callback_(TraceDirection::Transmit, frame);
    host_link::Response response;
    const auto host_timeout = static_cast<std::uint32_t>(match.timeout_ms) *
        static_cast<std::uint32_t>(frames.size() + 1U) + 1500U + gap_ms * 10U;
    const bool received = client_->RequestWithPolicy(host_link::Command::CanBurstWaitRx, payload,
                                   host_timeout, 2U, &response, error);
    if (completion_status) *completion_status = response.status;
    if (!received || !DecodeFrame(response.payload, received_frame, error)) return false;
    if ((received_frame->id & match.id_mask) != (match.id & match.id_mask) ||
        FrameFlags(*received_frame) != 0U || received_frame->dlc != match.expected_dlc) {
        SetError(error, L"CAN burst response does not match the requested filter.");
        return false;
    }
    if (trace_callback_) trace_callback_(TraceDirection::Receive, *received_frame);
    return true;
}

bool Client::Request(host_link::Command command,
                     const std::vector<std::uint8_t>& payload,
                     host_link::Response* response,
                     std::wstring* error) {
    if (client_ == nullptr) {
        SetError(error, L"Host Link client is not configured.");
        return false;
    }
    return client_->Request(command, payload, response, error);
}

bool Client::EncodeFrame(const Frame& frame,
                         bool transmit,
                         std::vector<std::uint8_t>* encoded,
                         std::wstring* error) {
    std::uint8_t data_length;
    const std::uint32_t maximum_id = frame.extended ? 0x1FFFFFFFU : 0x7FFU;

    if (encoded == nullptr || !DlcToLength(frame.dlc, &data_length) ||
        frame.id > maximum_id || (frame.bit_rate_switch && !frame.fd) ||
        (frame.remote && (frame.fd || frame.bit_rate_switch)) ||
        (!frame.fd && data_length > 8U) ||
        (transmit && (frame.timestamp != 0U || frame.error_state_indicator)) ||
        (frame.remote && (!frame.data.empty() || data_length != 0U)) ||
        (!frame.remote && frame.data.size() != data_length)) {
        SetError(error, L"Invalid CAN frame.");
        return false;
    }

    encoded->clear();
    encoded->reserve(kFrameHeaderSize + frame.data.size());
    encoded->push_back(0U);
    encoded->push_back(FrameFlags(frame));
    encoded->push_back(frame.dlc);
    encoded->push_back(static_cast<std::uint8_t>(frame.data.size()));
    PutLe32(encoded, frame.id);
    PutLe32(encoded, frame.timestamp);
    encoded->insert(encoded->end(), frame.data.begin(), frame.data.end());
    return true;
}

bool Client::DecodeFrame(const std::vector<std::uint8_t>& encoded,
                         Frame* frame,
                         std::wstring* error) {
    std::uint8_t expected_length;
    std::uint8_t flags;
    std::uint8_t data_length;
    std::uint32_t maximum_id;

    if (frame == nullptr || encoded.size() < kFrameHeaderSize) {
        SetError(error, L"Received CAN record is too short.");
        return false;
    }
    flags = encoded[1];
    data_length = encoded[3];
    if (encoded[0] != 0U ||
        (flags & static_cast<std::uint8_t>(~kFlagMask)) != 0U ||
        !DlcToLength(encoded[2], &expected_length) ||
        encoded.size() != kFrameHeaderSize + data_length ||
        ((flags & kFlagRtr) == 0U && data_length != expected_length) ||
        ((flags & kFlagRtr) != 0U && data_length != 0U) ||
        ((flags & kFlagBrs) != 0U && (flags & kFlagFdf) == 0U) ||
        ((flags & kFlagFdf) == 0U && expected_length > 8U)) {
        SetError(error, L"Received CAN record is invalid.");
        return false;
    }

    maximum_id = (flags & kFlagIde) != 0U ? 0x1FFFFFFFU : 0x7FFU;
    if (ReadLe32(&encoded[4]) > maximum_id) {
        SetError(error, L"Received CAN identifier is invalid.");
        return false;
    }

    frame->extended = (flags & kFlagIde) != 0U;
    frame->remote = (flags & kFlagRtr) != 0U;
    frame->fd = (flags & kFlagFdf) != 0U;
    frame->bit_rate_switch = (flags & kFlagBrs) != 0U;
    frame->error_state_indicator = (flags & kFlagEsi) != 0U;
    frame->dlc = encoded[2];
    frame->id = ReadLe32(&encoded[4]);
    frame->timestamp = ReadLe32(&encoded[8]);
    frame->data.assign(encoded.begin() +
                           static_cast<std::ptrdiff_t>(kFrameHeaderSize),
                       encoded.end());
    return true;
}

} // namespace firmware_upgrade::can_host
