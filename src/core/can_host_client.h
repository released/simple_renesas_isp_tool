#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "host_link_client.h"

namespace firmware_upgrade::can_host {

enum class Mode : std::uint8_t {
    Classical = 0U,
    CanFd = 1U,
};

constexpr std::uint32_t BurstFeatureFor(Mode mode) {
    return mode == Mode::CanFd ? host_link::kCanFdBurstFeature : host_link::kCanBurstFeature;
}

struct Configuration {
    Mode mode = Mode::Classical;
    std::uint8_t nominal_bitrate_preset = 2U;
    std::uint8_t data_bitrate_preset = 0xFFU;
};

constexpr std::uint32_t kSupportedNominalMask = 0x16U;
constexpr std::uint8_t kNominalPresets[] = {1U, 2U, 4U};
// Host Link preset numbers: 250K=1, 500K=2, 1M=4 (3 is 800K).
const wchar_t* NominalPresetName(std::uint8_t preset);
bool NominalPresetFromText(const std::wstring& text, std::uint8_t* preset);
bool IsConfigurationSupported(const Configuration& configuration,
                              const host_link::Capabilities& capabilities,
                              std::wstring* error);

struct Filter {
    bool enabled = true;
    bool extended = false;
    bool remote = false;
    std::uint32_t id = 0x701U;
    std::uint32_t mask = 0x7FFU;
};

struct Frame {
    bool extended = false;
    bool remote = false;
    bool fd = false;
    bool bit_rate_switch = false;
    bool error_state_indicator = false;
    std::uint8_t dlc = 0U;
    std::uint32_t id = 0U;
    std::uint32_t timestamp = 0U;
    std::vector<std::uint8_t> data;
};

struct ReceiveMatch {
    std::uint16_t timeout_ms = 500U;
    std::uint8_t retry_count = 0U;
    std::uint8_t expected_dlc = 0xFFU;
    std::uint32_t id = 0U;
    std::uint32_t id_mask = 0U;
    std::uint8_t flags_value = 0U;
    std::uint8_t flags_mask = 0U;
};

struct TransmitResult {
    std::uint32_t completion_timestamp = 0U;
    std::uint32_t controller_result = 0U;
    bool diagnostic_available = false;
    std::uint16_t input_head[4] = {};
    std::uint16_t input_data[4] = {};
    std::uint16_t ram_id_low = 0U;
    std::uint16_t ram_id_high = 0U;
    std::uint8_t ram_dlc_register = 0U;
    std::uint8_t ram_fd_control = 0U;
    std::uint16_t ram_data[4] = {};
    std::uint16_t ram_window_before = 0U;
};

enum class TraceDirection {
    Transmit,
    Receive,
};

using TraceCallback = std::function<void(TraceDirection, const Frame&)>;

class Client {
public:
    void SetHostLinkClient(host_link::Client* client);
    void SetTraceCallback(TraceCallback trace_callback);

    bool SetConfiguration(const Configuration& configuration,
                          std::wstring* error);
    bool SetFilter(std::uint8_t index,
                   const Filter& filter,
                   std::wstring* error);
    bool Start(std::uint32_t* controller_state, std::wstring* error);
    bool Stop(std::uint32_t* controller_state, std::wstring* error);
    bool Transmit(const Frame& frame,
                  TransmitResult* result,
                  std::wstring* error);
    bool TransmitWaitReceive(const Frame& transmit_frame,
                             const ReceiveMatch& match,
                             Frame* received_frame,
                             std::wstring* error);
    bool BurstWaitReceive(const std::vector<Frame>& frames,
                          const ReceiveMatch& match,
                          std::uint16_t gap_ms,
                          Frame* received_frame,
                          std::wstring* error,
                          host_link::Status* completion_status = nullptr);

private:
    bool Request(host_link::Command command,
                 const std::vector<std::uint8_t>& payload,
                 host_link::Response* response,
                 std::wstring* error);
    static bool EncodeFrame(const Frame& frame,
                            bool transmit,
                            std::vector<std::uint8_t>* encoded,
                            std::wstring* error);
    static bool DecodeFrame(const std::vector<std::uint8_t>& encoded,
                            Frame* frame,
                            std::wstring* error);

    host_link::Client* client_ = nullptr;
    TraceCallback trace_callback_;
};

} // namespace firmware_upgrade::can_host
