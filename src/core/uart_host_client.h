#pragma once
#include "bridge_port_catalog.h"

namespace firmware_upgrade::uart_host {
struct Configuration {
    std::uint8_t port = 0U;
    std::uint32_t baud = 115200U;
};
struct Transaction {
    std::vector<std::uint8_t> transmit;
    std::uint16_t receive_min = 1U;
    std::uint16_t receive_max = 110U;
    std::uint16_t timeout_ms = 1000U;
    std::uint16_t idle_ms = 5U;
    bool clear_receive = true;
};
// Worker-owned raw bridge client. It never interprets a device ISP/Xmodem frame.
// On failure the worker must call Abort with cancellation temporarily disabled;
// preserve the original error and never automatically repeat a device command.
class Client {
public:
    void SetHostLinkClient(host_link::Client* client);
    void Reset();
    bool Configure(const Configuration& configuration,
                   const std::vector<bridge::Port>& ports, std::wstring* error);
    bool Transfer(const Transaction& transaction,
                  std::vector<std::uint8_t>* receive, std::wstring* error);
    bool Abort(std::wstring* error);
    bool ReadStats(std::vector<std::uint32_t>* counts, std::wstring* error);
private:
    host_link::Client* link_ = nullptr;
    bool configured_ = false;
    Configuration configuration_;
    bridge::Port port_;
};
} // namespace firmware_upgrade::uart_host
