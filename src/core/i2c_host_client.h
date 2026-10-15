#pragma once
#include "bridge_port_catalog.h"

namespace firmware_upgrade::i2c_host {
struct Configuration {
    std::uint8_t port = 0U;
    std::uint8_t speed = 0U;
};
struct Transaction {
    std::uint8_t address = 0x09U;
    bool repeated_start = false;
    std::uint16_t receive_length = 0U;
    std::uint16_t timeout_ms = 1000U;
    std::uint16_t response_gap_ms = 0U;
    std::vector<std::uint8_t> transmit;
};
class Client {
public:
    void SetHostLinkClient(host_link::Client* client);
    void Reset();
    bool Configure(const Configuration& configuration,
                   const std::vector<bridge::Port>& ports, std::wstring* error);
    bool Transfer(const Transaction& transaction,
                  std::vector<std::uint8_t>* receive, std::wstring* error);
    bool Abort(std::wstring* error);
private:
    host_link::Client* link_ = nullptr;
    bool configured_ = false;
    Configuration configuration_;
    bridge::Port port_;
};
} // namespace firmware_upgrade::i2c_host
