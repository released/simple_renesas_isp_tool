#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "host_link_protocol.h"
#include "host_link_transport.h"

namespace firmware_upgrade::host_link {

struct ClientOptions {
    std::uint32_t response_timeout_ms = 800U;
    std::uint32_t maximum_attempts = 2U;
    std::size_t maximum_pending_events = 16U;
};

struct ClientStats {
    std::uint64_t requests = 0U;
    std::uint64_t retries = 0U;
    std::uint64_t response_timeouts = 0U;
    std::uint64_t unmatched_frames = 0U;
    std::uint64_t received_events = 0U;
    std::uint64_t dropped_events = 0U;
};

struct HostInfo {
    std::uint16_t board_id = 0U;
    std::uint8_t firmware_major = 0U;
    std::uint8_t firmware_minor = 0U;
    std::uint8_t firmware_patch = 0U;
    std::uint8_t minimum_protocol_version = 0U;
    std::uint8_t maximum_protocol_version = 0U;
    std::uint32_t firmware_build = 0U;
    std::uint32_t feature_flags = 0U;
};

struct Capabilities {
    std::uint32_t feature_flags = 0U;
    std::uint16_t maximum_host_link_payload = 0U;
    std::uint8_t maximum_can_data_bytes = 0U;
    std::uint8_t maximum_outstanding_requests = 0U;
    std::uint8_t uart_rx_queue_depth = 0U;
    std::uint8_t can_rx_queue_depth = 0U;
    std::uint8_t filter_slots = 0U;
    std::uint32_t nominal_bitrate_mask = 0U;
    std::uint32_t data_bitrate_mask = 0U;
    std::uint32_t timestamp_frequency_hz = 0U;
};

struct HostStats {
    std::uint32_t uart_rx_bytes = 0U;
    std::uint32_t uart_tx_bytes = 0U;
    std::uint32_t uart_crc_errors = 0U;
    std::uint32_t uart_invalid_errors = 0U;
    std::uint32_t uart_rx_overflows = 0U;
    std::uint32_t can_rx_frames = 0U;
    std::uint32_t can_tx_frames = 0U;
    std::uint32_t can_rx_lost_frames = 0U;
    std::uint32_t can_tx_failures = 0U;
    std::uint32_t can_warning_entries = 0U;
    std::uint32_t can_error_passive_entries = 0U;
    std::uint32_t can_bus_off_entries = 0U;
    std::uint32_t can_recovery_count = 0U;
    std::uint32_t host_command_errors = 0U;
};

struct Response {
    std::uint16_t sequence = 0U;
    Command command = Command::Ping;
    Status status = Status::InternalError;
    std::vector<std::uint8_t> payload;
};

enum class TraceKind {
    Request,
    Response,
    Event,
    Retry,
    Timeout,
    Unmatched,
};

struct TraceRecord {
    TraceKind kind = TraceKind::Request;
    std::uint16_t sequence = 0U;
    std::uint8_t command = 0U;
    Status status = Status::Ok;
    std::uint32_t attempt = 0U;
    std::vector<std::uint8_t> payload;
};

using TraceCallback = std::function<void(const TraceRecord&)>;

class Client {
public:
    void SetTransport(Transport* transport);
    void SetOptions(const ClientOptions& options);
    void SetCancelCheck(std::function<bool()> cancel_check);
    void SetTraceCallback(TraceCallback trace_callback);
    const ClientOptions& Options() const;

    void BeginSession();
    bool Request(Command command,
                 const std::vector<std::uint8_t>& payload,
                 Response* response,
                 std::wstring* error);
    bool RequestWithPolicy(Command command,
                           const std::vector<std::uint8_t>& payload,
                           std::uint32_t response_timeout_ms,
                           std::uint32_t maximum_attempts,
                           Response* response,
                           std::wstring* error);
    bool Ping(const std::vector<std::uint8_t>& payload, std::wstring* error);
    bool GetHostInfo(HostInfo* info, std::wstring* error);
    bool GetCapabilities(Capabilities* capabilities, std::wstring* error);
    bool GetStats(HostStats* stats, std::wstring* error);
    bool ResetHost(std::wstring* error);
    bool PollEvents(std::uint32_t timeout_ms, std::wstring* error);
    bool Probe(std::uint16_t expected_board_id,
               HostInfo* info,
               Capabilities* capabilities,
               std::wstring* error);

    bool TryPopEvent(Frame* event);
    bool IsHostValidated() const;
    const ClientStats& Stats() const;
    const FrameStreamParser& Parser() const;

private:
    enum class WaitResult {
        Complete,
        Timeout,
        TransportError,
        Cancelled,
    };

    WaitResult WaitForResponse(std::uint16_t sequence,
                               Command command,
                               std::uint32_t response_timeout_ms,
                               Response* response,
                               std::wstring* error);
    std::uint16_t AllocateSequence();
    void QueueEvent(Frame&& event);
    void Trace(const TraceRecord& record) const;

    Transport* transport_ = nullptr;
    ClientOptions options_{};
    ClientStats stats_{};
    FrameStreamParser parser_;
    std::deque<Frame> pending_events_;
    std::uint16_t next_sequence_ = 1U;
    bool host_validated_ = false;
    std::function<bool()> cancel_check_;
    TraceCallback trace_callback_;
};

} // namespace firmware_upgrade::host_link
