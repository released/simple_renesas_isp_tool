#include "uart_host_client.h"
#include "isp_bus_settings.h"
#include <algorithm>

namespace firmware_upgrade::uart_host {
namespace {
void Put16(std::vector<std::uint8_t>& p, std::uint16_t n) {
    p.push_back(static_cast<std::uint8_t>(n)); p.push_back(static_cast<std::uint8_t>(n >> 8));
}
std::uint16_t Le16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
bool Fail(std::wstring* error, const wchar_t* text) {
    if (error) *error = text;
    return false;
}
}
void Client::SetHostLinkClient(host_link::Client* client) { link_ = client; Reset(); }
void Client::Reset() { configured_ = false; }
bool Client::Configure(const Configuration& configuration,
                       const std::vector<bridge::Port>& ports, std::wstring* error) {
    Reset();
    const auto found = std::find_if(ports.begin(), ports.end(), [&](const bridge::Port& p) {
        return p.bus == bridge::Bus::Uart && p.id == configuration.port;
    });
    if (!link_ || !link_->IsHostValidated() || found == ports.end() ||
        found->flags != 1U || !isp::UartRateSupported(found->rates,configuration.baud))
        return Fail(error, L"Selected device UART port/baud is not supported by the bridge.");
    std::vector<std::uint8_t> payload{configuration.port, 0U, 0U, 0U};
    Put16(payload, static_cast<std::uint16_t>(configuration.baud));
    Put16(payload, static_cast<std::uint16_t>(configuration.baud >> 16));
    host_link::Response response;
    if (!link_->Request(host_link::Command::SetUartConfig, payload, &response, error)) return false;
    if (response.payload != payload) return Fail(error, L"Device UART configuration acknowledgement mismatch.");
    configuration_ = configuration; port_ = *found; configured_ = true;
    return true;
}
bool Client::Transfer(const Transaction& t, std::vector<std::uint8_t>* receive,
                      std::wstring* error) {
    if (receive) receive->clear();
    if (!link_ || !link_->IsHostValidated() || !configured_)
        return Fail(error, L"Probe and configure the device UART bridge first.");
    if (!receive || t.transmit.size() > 512U || t.transmit.size() > port_.maximum_tx ||
        t.receive_max > 512U || t.receive_max > port_.maximum_rx ||
        t.receive_min > t.receive_max || (t.transmit.empty() && !t.receive_max) ||
        (t.receive_max && !t.receive_min) || !t.timeout_ms || t.timeout_ms > 30000U ||
        t.idle_ms > t.timeout_ms || (t.receive_min < t.receive_max && !t.idle_ms))
        return Fail(error, L"Invalid device UART lengths, timeout or idle completion settings.");
    std::vector<std::uint8_t> begin{0U, configuration_.port,
        static_cast<std::uint8_t>(t.clear_receive), 0U};
    Put16(begin, static_cast<std::uint16_t>(t.transmit.size()));
    Put16(begin, t.receive_max); Put16(begin, t.receive_min);
    Put16(begin, t.timeout_ms); Put16(begin, t.idle_ms);
    host_link::Response response;
    if (!link_->Request(host_link::Command::UartTransaction, begin, &response, error)) return false;
    if (!response.payload.empty()) return Fail(error, L"Invalid UART BEGIN response.");
    for (std::size_t at = 0U; at < t.transmit.size();) {
        const auto count = (std::min)(host_link::kMaximumPayloadSize - 3U, t.transmit.size() - at);
        std::vector<std::uint8_t> chunk{1U}; Put16(chunk, static_cast<std::uint16_t>(at));
        chunk.insert(chunk.end(), t.transmit.begin() + at, t.transmit.begin() + at + count);
        if (!link_->Request(host_link::Command::UartTransaction, chunk, &response, error)) return false;
        if (!response.payload.empty()) return Fail(error, L"Invalid UART CHUNK response.");
        at += count;
    }
    // Retries repeat the identical Host Link EXEC sequence, not a new UART TX.
    if (!link_->RequestWithPolicy(host_link::Command::UartTransaction, {2U},
        static_cast<std::uint32_t>(t.timeout_ms) + 500U, 2U, &response, error)) return false;
    if (response.payload.size() != 2U) return Fail(error, L"Invalid UART EXEC response.");
    const auto size = Le16(response.payload.data());
    if (size < t.receive_min || size > t.receive_max)
        return Fail(error, L"UART completion length mismatch; do not repeat the device command.");
    std::vector<std::uint8_t> result;
    for (std::uint16_t at = 0U; at < size;) {
        const auto count = static_cast<std::uint8_t>((std::min)(host_link::kMaximumPayloadSize - 3U,
            static_cast<std::size_t>(size - at)));
        std::vector<std::uint8_t> fetch{3U}; Put16(fetch, at); fetch.push_back(count);
        if (!link_->Request(host_link::Command::UartTransaction, fetch, &response, error)) return false;
        if (response.payload.size() != static_cast<std::size_t>(count) + 2U ||
            Le16(response.payload.data()) != at) return Fail(error, L"UART FETCH offset/length mismatch.");
        result.insert(result.end(), response.payload.begin() + 2, response.payload.end());
        at = static_cast<std::uint16_t>(at + count);
    }
    if (!link_->Request(host_link::Command::UartTransaction, {5U}, &response, error)) return false;
    if (!response.payload.empty()) return Fail(error, L"Invalid UART RELEASE response.");
    *receive = std::move(result);
    return true;
}
bool Client::Abort(std::wstring* error) {
    if (!link_ || !link_->IsHostValidated()) return true;
    host_link::Response response;
    return link_->RequestWithPolicy(host_link::Command::UartTransaction, {4U}, 500U, 1U,
        &response, error) && (response.payload.empty() || Fail(error, L"Invalid UART ABORT response."));
}
bool Client::ReadStats(std::vector<std::uint32_t>* counts, std::wstring* error) {
    if (!link_ || !link_->IsHostValidated() || !counts)
        return Fail(error, L"Probe the bridge before reading UART counters.");
    counts->clear();
    host_link::Response response;
    if (!link_->Request(host_link::Command::GetUartStats, {}, &response, error)) return false;
    if (response.payload.size() != 24U) return Fail(error, L"Invalid UART counter response.");
    for (std::size_t at = 0U; at < 24U; at += 4U) counts->push_back(
        static_cast<std::uint32_t>(Le16(response.payload.data() + at)) |
        (static_cast<std::uint32_t>(Le16(response.payload.data() + at + 2U)) << 16));
    return true;
}
} // namespace firmware_upgrade::uart_host
