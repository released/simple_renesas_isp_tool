#include "host_link_client.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <utility>

namespace firmware_upgrade::host_link {

namespace {

std::uint16_t ReadLe16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8));
}

std::uint32_t ReadLe32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) |
           (static_cast<std::uint32_t>(data[3]) << 24);
}

const wchar_t* StatusText(Status status) {
    switch (status) {
        case Status::Ok: return L"OK";
        case Status::Busy: return L"BUSY";
        case Status::BadCommand: return L"BAD_COMMAND";
        case Status::BadArgument: return L"BAD_ARGUMENT";
        case Status::CrcError: return L"CRC_ERROR";
        case Status::SequenceError: return L"SEQUENCE_ERROR";
        case Status::Timeout: return L"TIMEOUT";
        case Status::CanError: return L"CAN_ERROR";
        case Status::Overflow: return L"OVERFLOW";
        case Status::Incompatible: return L"INCOMPATIBLE";
        case Status::BadState: return L"BAD_STATE";
        case Status::InternalError: return L"INTERNAL_ERROR";
        case Status::I2cNack: return L"I2C_NACK (address or data not acknowledged)";
        case Status::I2cArbitrationLost: return L"I2C_ARBITRATION_LOST";
        case Status::I2cBusError: return L"I2C_BUS_ERROR";
        default: return L"UNKNOWN";
    }
}

void SetError(std::wstring* error, const std::wstring& text) {
    if (error != nullptr) {
        *error = text;
    }
}

} // namespace

void Client::SetTransport(Transport* transport) {
    transport_ = transport;
    BeginSession();
}

void Client::SetOptions(const ClientOptions& options) {
    options_ = options;
    if (options_.response_timeout_ms == 0U) {
        options_.response_timeout_ms = 1U;
    }
    if (options_.maximum_attempts == 0U) {
        options_.maximum_attempts = 1U;
    }
}

void Client::SetCancelCheck(std::function<bool()> cancel_check) {
    cancel_check_ = std::move(cancel_check);
}

void Client::SetTraceCallback(TraceCallback trace_callback) {
    trace_callback_ = std::move(trace_callback);
}

const ClientOptions& Client::Options() const {
    return options_;
}

void Client::BeginSession() {
    parser_.Clear();
    pending_events_.clear();
    stats_ = {};
    next_sequence_ = 1U;
    host_validated_ = false;
}

bool Client::Request(Command command,
                     const std::vector<std::uint8_t>& payload,
                     Response* response,
                     std::wstring* error) {
    return RequestWithPolicy(command, payload, options_.response_timeout_ms,
                             options_.maximum_attempts, response, error);
}

bool Client::RequestWithPolicy(Command command,
                               const std::vector<std::uint8_t>& payload,
                               std::uint32_t response_timeout_ms,
                               std::uint32_t maximum_attempts,
                               Response* response,
                               std::wstring* error) {
    Frame request;
    std::vector<std::uint8_t> encoded;
    Response received;
    std::uint16_t sequence;

    if (error != nullptr) {
        error->clear();
    }
    if (transport_ == nullptr || !transport_->IsOpen()) {
        SetError(error, L"Host Link transport is not open.");
        return false;
    }
    if (payload.size() > kMaximumPayloadSize) {
        SetError(error, L"Host Link payload exceeds 128 bytes.");
        return false;
    }
    if (response_timeout_ms == 0U) {
        response_timeout_ms = 1U;
    }
    if (maximum_attempts == 0U) {
        maximum_attempts = 1U;
    }

    sequence = AllocateSequence();
    request.type = FrameType::Request;
    request.sequence = sequence;
    request.command = static_cast<std::uint8_t>(command);
    request.status = Status::Ok;
    request.payload = payload;
    if (!EncodeFrame(request, &encoded)) {
        SetError(error, L"Failed to encode Host Link request.");
        return false;
    }

    ++stats_.requests;
    for (std::uint32_t attempt = 0U;
         attempt < maximum_attempts;
         ++attempt) {
        WaitResult wait_result;

        if (cancel_check_ && cancel_check_()) {
            SetError(error, L"Host Link request canceled.");
            return false;
        }
        if (attempt != 0U) {
            ++stats_.retries;
            Trace(TraceRecord{TraceKind::Retry, sequence,
                              static_cast<std::uint8_t>(command),
                              Status::Ok, attempt + 1U, payload});
        }
        if (!transport_->WriteAll(encoded.data(), encoded.size(),
                                  response_timeout_ms, error)) {
            return false;
        }
        Trace(TraceRecord{TraceKind::Request, sequence,
                          static_cast<std::uint8_t>(command),
                          Status::Ok, attempt + 1U, payload});

        wait_result = WaitForResponse(sequence, command, response_timeout_ms,
                                      &received, error);
        if (wait_result == WaitResult::Complete) {
            if (response != nullptr) {
                *response = received;
            }
            if (received.status != Status::Ok) {
                std::wstringstream stream;
                stream << L"Host returned " << StatusText(received.status)
                       << L" for command 0x" << std::hex
                       << static_cast<unsigned>(received.command) << L".";
                SetError(error, stream.str());
                return false;
            }
            return true;
        }
        if (wait_result == WaitResult::TransportError) {
            return false;
        }
        if (wait_result == WaitResult::Cancelled) {
            SetError(error, L"Host Link request canceled.");
            return false;
        }

        ++stats_.response_timeouts;
        Trace(TraceRecord{TraceKind::Timeout, sequence,
                          static_cast<std::uint8_t>(command),
                          Status::Timeout, attempt + 1U, {}});
    }

    SetError(error, L"Host Link response timeout after retries.");
    return false;
}

bool Client::Ping(const std::vector<std::uint8_t>& payload,
                  std::wstring* error) {
    Response response;

    if (payload.size() > kMaximumPayloadSize) {
        SetError(error, L"PING payload exceeds 128 bytes.");
        return false;
    }
    if (!Request(Command::Ping, payload, &response, error)) {
        return false;
    }
    if (response.payload != payload) {
        SetError(error, L"PING response payload does not match the request.");
        return false;
    }
    return true;
}

bool Client::GetHostInfo(HostInfo* info, std::wstring* error) {
    Response response;
    HostInfo parsed;

    if (info == nullptr) {
        SetError(error, L"Host information output is null.");
        return false;
    }
    if (!Request(Command::GetHostInfo, {}, &response, error)) {
        // A reconnect can collide with the peer's last cached CONFIRM sequence.
        // Only this read-only identity query gets one fresh-sequence recovery.
        if (response.status!=Status::SequenceError ||
            !RequestWithPolicy(Command::GetHostInfo,{},options_.response_timeout_ms,1U,&response,error))
            return false;
    }
    if (response.payload.size() != 16U || response.payload[7] != 0U) {
        SetError(error, L"GET_HOST_INFO returned an invalid payload.");
        return false;
    }

    parsed.board_id = ReadLe16(&response.payload[0]);
    if (parsed.board_id == 0U || parsed.board_id == 0xFFFFU) {
        SetError(error, L"Host returned a reserved board identity.");
        return false;
    }
    parsed.firmware_major = response.payload[2];
    parsed.firmware_minor = response.payload[3];
    parsed.firmware_patch = response.payload[4];
    parsed.minimum_protocol_version = response.payload[5];
    parsed.maximum_protocol_version = response.payload[6];
    parsed.firmware_build = ReadLe32(&response.payload[8]);
    parsed.feature_flags = ReadLe32(&response.payload[12]);
    if (parsed.minimum_protocol_version > kVersion ||
        parsed.maximum_protocol_version < kVersion) {
        SetError(error, L"Host does not support Host Link protocol v2.");
        return false;
    }

    *info = parsed;
    return true;
}

bool Client::GetCapabilities(Capabilities* capabilities,
                             std::wstring* error) {
    Response response;
    Capabilities parsed;

    if (capabilities == nullptr) {
        SetError(error, L"Capabilities output is null.");
        return false;
    }
    if (!Request(Command::GetCapabilities, {}, &response, error)) {
        return false;
    }
    if (response.payload.size() != 24U || response.payload[11] != 0U) {
        SetError(error, L"GET_CAPABILITIES returned an invalid payload.");
        return false;
    }

    parsed.feature_flags = ReadLe32(&response.payload[0]);
    parsed.maximum_host_link_payload = ReadLe16(&response.payload[4]);
    parsed.maximum_can_data_bytes = response.payload[6];
    parsed.maximum_outstanding_requests = response.payload[7];
    parsed.uart_rx_queue_depth = response.payload[8];
    parsed.can_rx_queue_depth = response.payload[9];
    parsed.filter_slots = response.payload[10];
    parsed.nominal_bitrate_mask = ReadLe32(&response.payload[12]);
    parsed.data_bitrate_mask = ReadLe32(&response.payload[16]);
    parsed.timestamp_frequency_hz = ReadLe32(&response.payload[20]);

    const bool has_can = (parsed.feature_flags & 0x03U) != 0U;
    const bool has_i2c = (parsed.feature_flags & 0x100U) != 0U;
    const bool has_bridge_update = (parsed.feature_flags & 0x400U) != 0U;
    if (parsed.maximum_host_link_payload != kMaximumPayloadSize) {
        SetError(error,L"Bridge advertises Host Link payload " +
            std::to_wstring(parsed.maximum_host_link_payload) +
            L" bytes; this tool requires128. Reflash matching bridge firmware;96-byte hosts are not supported.");
        return false;
    }
    if ((!has_can && !has_i2c && !has_bridge_update) ||
        (has_can && ((parsed.maximum_can_data_bytes != 8U &&
                     parsed.maximum_can_data_bytes != 64U) ||
                    parsed.filter_slots == 0U)) ||
        (!has_can && parsed.maximum_can_data_bytes != 0U) ||
        parsed.maximum_outstanding_requests == 0U) {
        SetError(error, L"Host capabilities are incompatible with this tool.");
        return false;
    }

    *capabilities = parsed;
    return true;
}

bool Client::GetStats(HostStats* stats, std::wstring* error) {
    Response response;
    HostStats parsed;

    if (stats == nullptr) {
        SetError(error, L"Host statistics output is null.");
        return false;
    }
    if (!Request(Command::GetStats, {}, &response, error)) {
        return false;
    }
    if (response.payload.size() != 56U && response.payload.size() != 84U) {
        SetError(error, L"GET_STATS returned an invalid payload.");
        return false;
    }

    parsed.uart_rx_bytes = ReadLe32(&response.payload[0U]);
    parsed.uart_tx_bytes = ReadLe32(&response.payload[4U]);
    parsed.uart_crc_errors = ReadLe32(&response.payload[8U]);
    parsed.uart_invalid_errors = ReadLe32(&response.payload[12U]);
    parsed.uart_rx_overflows = ReadLe32(&response.payload[16U]);
    parsed.can_rx_frames = ReadLe32(&response.payload[20U]);
    parsed.can_tx_frames = ReadLe32(&response.payload[24U]);
    parsed.can_rx_lost_frames = ReadLe32(&response.payload[28U]);
    parsed.can_tx_failures = ReadLe32(&response.payload[32U]);
    parsed.can_warning_entries = ReadLe32(&response.payload[36U]);
    parsed.can_error_passive_entries = ReadLe32(&response.payload[40U]);
    parsed.can_bus_off_entries = ReadLe32(&response.payload[44U]);
    parsed.can_recovery_count = ReadLe32(&response.payload[48U]);
    parsed.host_command_errors = ReadLe32(&response.payload[52U]);
    if(response.payload.size()==84U) {
        parsed.telemetry_available=true;
        parsed.uart_rx_high_water=ReadLe32(response.payload.data()+56U);
        parsed.uart_tx_high_water=ReadLe32(response.payload.data()+60U);
        parsed.can_rx_high_water=ReadLe32(response.payload.data()+64U);
        parsed.transaction_queue_high_water=ReadLe32(response.payload.data()+68U);
        parsed.queue_wait_max_ms=ReadLe32(response.payload.data()+72U);
        parsed.transaction_max_ms=ReadLe32(response.payload.data()+76U);
        parsed.process_max_ms=ReadLe32(response.payload.data()+80U);
    }
    *stats = parsed;
    return true;
}

bool Client::ResetHost(std::wstring* error) {
    Response response;

    if (!RequestWithPolicy(Command::ResetHost, {}, 1000U, 1U,
                           &response, error)) {
        return false;
    }
    if (!response.payload.empty()) {
        SetError(error, L"RESET_HOST returned an invalid payload.");
        return false;
    }
    host_validated_ = false;
    return true;
}

bool Client::PollEvents(std::uint32_t timeout_ms, std::wstring* error) {
    std::vector<std::uint8_t> input;
    std::wstring read_error;
    Frame frame;

    if (error != nullptr) {
        error->clear();
    }
    if (transport_ == nullptr || !transport_->IsOpen()) {
        SetError(error, L"Host Link transport is not open.");
        return false;
    }

    if (!transport_->ReadSome(&input, kStreamBufferCapacity, timeout_ms,
                              &read_error)) {
        if (read_error == L"Read timeout.") {
            return true;
        }
        SetError(error, read_error.empty() ?
            L"Host Link transport read failed." : read_error);
        return false;
    }
    parser_.Push(input.data(), input.size());
    while (parser_.TryPop(&frame)) {
        if (frame.type == FrameType::Event && frame.sequence == 0U) {
            Trace(TraceRecord{TraceKind::Event, frame.sequence,
                              frame.command, frame.status, 0U,
                              frame.payload});
            QueueEvent(std::move(frame));
        } else {
            ++stats_.unmatched_frames;
            Trace(TraceRecord{TraceKind::Unmatched, frame.sequence,
                              frame.command, frame.status, 0U,
                              frame.payload});
        }
    }
    return true;
}

bool Client::Probe(std::uint16_t expected_board_id,
                   HostInfo* info,
                   Capabilities* capabilities,
                   std::wstring* error) {
    HostInfo parsed_info;
    Capabilities parsed_capabilities;

    host_validated_ = false;
    if (!GetHostInfo(&parsed_info, error) ||
        !GetCapabilities(&parsed_capabilities, error)) {
        return false;
    }
    if (expected_board_id != 0U && parsed_info.board_id != expected_board_id) {
        std::wstringstream stream;
        stream << L"Unexpected MCU host board ID 0x" << std::hex
               << parsed_info.board_id << L"; expected 0x"
               << expected_board_id << L".";
        SetError(error, stream.str());
        return false;
    }

    if (info != nullptr) {
        *info = parsed_info;
    }
    if (capabilities != nullptr) {
        *capabilities = parsed_capabilities;
    }
    host_validated_ = true;
    return true;
}

bool Client::TryPopEvent(Frame* event) {
    if (event == nullptr || pending_events_.empty()) {
        return false;
    }
    *event = std::move(pending_events_.front());
    pending_events_.pop_front();
    return true;
}

bool Client::IsHostValidated() const {
    return host_validated_;
}

const ClientStats& Client::Stats() const {
    return stats_;
}

const FrameStreamParser& Client::Parser() const {
    return parser_;
}

Client::WaitResult Client::WaitForResponse(std::uint16_t sequence,
                                           Command command,
                                           std::uint32_t response_timeout_ms,
                                           Response* response,
                                           std::wstring* error) {
    const auto started = std::chrono::steady_clock::now();

    for (;;) {
        Frame frame;

        if (cancel_check_ && cancel_check_()) {
            return WaitResult::Cancelled;
        }
        while (parser_.TryPop(&frame)) {
            if (frame.type == FrameType::Event && frame.sequence == 0U) {
                Trace(TraceRecord{TraceKind::Event, frame.sequence,
                                  frame.command, frame.status, 0U,
                                  frame.payload});
                QueueEvent(std::move(frame));
            } else if (frame.type == FrameType::Response &&
                       frame.sequence == sequence &&
                       frame.command == static_cast<std::uint8_t>(command)) {
                Trace(TraceRecord{TraceKind::Response, frame.sequence,
                                  frame.command, frame.status, 0U,
                                  frame.payload});
                response->sequence = frame.sequence;
                response->command = static_cast<Command>(frame.command);
                response->status = frame.status;
                response->payload = std::move(frame.payload);
                return WaitResult::Complete;
            } else {
                ++stats_.unmatched_frames;
                Trace(TraceRecord{TraceKind::Unmatched, frame.sequence,
                                  frame.command, frame.status, 0U,
                                  frame.payload});
            }
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        if (elapsed.count() >= response_timeout_ms) {
            return WaitResult::Timeout;
        }

        const std::uint32_t remaining = response_timeout_ms -
            static_cast<std::uint32_t>(elapsed.count());
        const std::uint32_t read_timeout = cancel_check_ ?
            (std::min)(remaining, 50U) : remaining;
        std::vector<std::uint8_t> input;
        std::wstring read_error;
        if (!transport_->ReadSome(&input, kStreamBufferCapacity, read_timeout,
                                  &read_error)) {
            if (read_error == L"Read timeout.") {
                if (cancel_check_ && cancel_check_()) {
                    return WaitResult::Cancelled;
                }
                if (read_timeout < remaining) {
                    continue;
                }
                return WaitResult::Timeout;
            }
            SetError(error, read_error.empty() ?
                L"Host Link transport read failed." : read_error);
            return WaitResult::TransportError;
        }
        parser_.Push(input.data(), input.size());
    }
}

std::uint16_t Client::AllocateSequence() {
    const std::uint16_t allocated = next_sequence_;
    ++next_sequence_;
    if (next_sequence_ == 0U) {
        next_sequence_ = 1U;
    }
    return allocated;
}

void Client::QueueEvent(Frame&& event) {
    ++stats_.received_events;
    if (pending_events_.size() >= options_.maximum_pending_events) {
        ++stats_.dropped_events;
        return;
    }
    pending_events_.push_back(std::move(event));
}

void Client::Trace(const TraceRecord& record) const {
    if (trace_callback_) {
        trace_callback_(record);
    }
}

} // namespace firmware_upgrade::host_link
