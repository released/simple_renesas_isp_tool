#include "i2c_host_client.h"
#include <algorithm>

namespace firmware_upgrade::i2c_host {
namespace {
void Put16(std::vector<std::uint8_t>* p, std::uint16_t n) {
    p->push_back(static_cast<std::uint8_t>(n));
    p->push_back(static_cast<std::uint8_t>(n >> 8));
}
bool Fail(std::wstring* error, const wchar_t* text) {
    if (error != nullptr) *error = text;
    return false;
}
}
void Client::SetHostLinkClient(host_link::Client* client) {
    link_ = client; Reset();
}
void Client::Reset() { configured_ = false; }

bool Client::Configure(const Configuration& configuration,
                       const std::vector<bridge::Port>& ports,
                       std::wstring* error) {
    configured_ = false;
    const auto found = std::find_if(ports.begin(), ports.end(),
        [&configuration](const bridge::Port& p) {
            return p.bus == bridge::Bus::I2c && p.id == configuration.port;
        });
    if (link_ == nullptr || !link_->IsHostValidated() ||
        found == ports.end() || (found->flags & 1U) == 0U ||
        configuration.speed > 1U ||
        (found->rates & (1UL << configuration.speed)) == 0U)
        return Fail(error, L"Selected I2C port/speed is not supported by the bridge.");
    const std::vector<std::uint8_t> payload =
        {configuration.port, configuration.speed, 0U, 0U};
    host_link::Response response;
    if (!link_->Request(host_link::Command::SetI2cConfig, payload, &response,
                         error)) return false;
    if (response.payload != payload)
        return Fail(error, L"I2C configuration acknowledgement mismatch.");
    configuration_ = configuration; port_ = *found; configured_ = true;
    return true;
}

bool Client::Transfer(const Transaction& transaction,
                      std::vector<std::uint8_t>* receive,
                      std::wstring* error) {
    if (link_ == nullptr || !link_->IsHostValidated() || !configured_)
        return Fail(error, L"Probe and configure the I2C bridge first.");
    if (receive == nullptr || transaction.address < 8U ||
        transaction.address > 0x77U || transaction.transmit.size() > 512U ||
        transaction.transmit.size() > port_.maximum_tx ||
        transaction.receive_length > 512U ||
        transaction.receive_length > port_.maximum_rx ||
        (transaction.transmit.empty() && transaction.receive_length == 0U) ||
        transaction.timeout_ms == 0U || transaction.timeout_ms > 30000U ||
        transaction.response_gap_ms > transaction.timeout_ms ||
        (transaction.repeated_start &&
         ((port_.flags & 2U) == 0U || transaction.transmit.empty() ||
          transaction.receive_length == 0U || transaction.response_gap_ms != 0U)))
        return Fail(error, L"Invalid I2C address, length, timeout or repeated-start settings.");
    receive->clear();
    std::vector<std::uint8_t> begin = {0U, configuration_.port,
        transaction.address, static_cast<std::uint8_t>(transaction.repeated_start)};
    Put16(&begin, static_cast<std::uint16_t>(transaction.transmit.size()));
    Put16(&begin, transaction.receive_length);
    Put16(&begin, transaction.timeout_ms);
    Put16(&begin, transaction.response_gap_ms);
    host_link::Response response;
    if (!link_->Request(host_link::Command::I2cTransaction, begin, &response,
                         error)) return false;
    if (!response.payload.empty()) return Fail(error, L"Invalid I2C BEGIN response.");
    for (std::size_t offset = 0U; offset < transaction.transmit.size();) {
        const std::size_t count = (std::min)(static_cast<unsigned>(host_link::kMaximumPayloadSize-3U),
            static_cast<unsigned>(transaction.transmit.size() - offset));
        std::vector<std::uint8_t> chunk = {1U};
        Put16(&chunk, static_cast<std::uint16_t>(offset));
        chunk.insert(chunk.end(), transaction.transmit.begin() + offset,
                     transaction.transmit.begin() + offset + count);
        if (!link_->Request(host_link::Command::I2cTransaction, chunk, &response,
                             error)) return false;
        if (!response.payload.empty()) return Fail(error, L"Invalid I2C CHUNK response.");
        offset += count;
    }
    if (!link_->RequestWithPolicy(host_link::Command::I2cTransaction, {2U},
        static_cast<std::uint32_t>(transaction.timeout_ms) + 500U, 2U,
        &response, error)) return false;
    if (response.payload.size() != 2U ||
        static_cast<std::uint16_t>(response.payload[0] |
            (response.payload[1] << 8)) != transaction.receive_length)
        return Fail(error, L"I2C completion length mismatch; do not retry the target command.");
    std::vector<std::uint8_t> result;
    for (std::uint16_t offset = 0U; offset < transaction.receive_length;) {
        const auto count = static_cast<std::uint8_t>((std::min)(static_cast<unsigned>(host_link::kMaximumPayloadSize-3U),
            static_cast<unsigned>(transaction.receive_length - offset)));
        std::vector<std::uint8_t> fetch = {3U}; Put16(&fetch, offset);
        fetch.push_back(count);
        if (!link_->Request(host_link::Command::I2cTransaction, fetch,
                             &response, error)) return false;
        if (response.payload.size() != static_cast<std::size_t>(count) + 2U ||
            response.payload[0] != static_cast<std::uint8_t>(offset) ||
            response.payload[1] != static_cast<std::uint8_t>(offset >> 8))
            return Fail(error, L"I2C receive chunk offset/length mismatch.");
        result.insert(result.end(), response.payload.begin() + 2U,
                      response.payload.end());
        offset = static_cast<std::uint16_t>(offset + count);
    }
    *receive = std::move(result);
    return true;
}

bool Client::Abort(std::wstring* error) {
    if (link_ == nullptr || !link_->IsHostValidated()) return true;
    host_link::Response response;
    return link_->RequestWithPolicy(host_link::Command::I2cTransaction,
        {4U}, 500U, 1U, &response, error) && response.payload.empty();
}
} // namespace firmware_upgrade::i2c_host
