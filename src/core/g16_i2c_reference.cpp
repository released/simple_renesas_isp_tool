#include "g16_i2c_reference.h"

namespace firmware_upgrade::g16_i2c {
std::vector<std::uint8_t> EncodePacket(std::uint8_t start, std::uint8_t command,
                                     const std::vector<std::uint8_t>& data) {
    if ((start != 1U && start != 0x81U) || data.size() > 256U) return {};
    const auto length = static_cast<std::uint16_t>(data.size() + 1U);
    std::vector<std::uint8_t> result = {start,
        static_cast<std::uint8_t>(length >> 8),
        static_cast<std::uint8_t>(length), command};
    result.insert(result.end(), data.begin(), data.end());
    std::uint8_t sum = 0U;
    for (std::size_t i = 1U; i < result.size(); ++i)
        sum = static_cast<std::uint8_t>(sum + result[i]);
    result.push_back(static_cast<std::uint8_t>(0U - sum));
    result.push_back(3U);
    return result;
}
bool ValidatePacket(const std::vector<std::uint8_t>& data,
                    std::uint8_t command) {
    if (data.size() < 6U || data.size() > 262U || data[0] != 0x81U || data.back() != 3U ||
        data[3] != command ||
        static_cast<std::size_t>((data[1] << 8) | data[2]) != data.size() - 5U)
        return false;
    std::uint8_t sum = 0U;
    for (std::size_t i = 1U; i + 1U < data.size(); ++i)
        sum = static_cast<std::uint8_t>(sum + data[i]);
    return sum == 0U;
}
bool Probe(i2c_host::Client& client, std::uint8_t address, std::wstring* error) {
    i2c_host::Transaction transaction;
    transaction.address = address;
    std::vector<std::uint8_t> receive;
    transaction.transmit = {0U};
    if (!client.Transfer(transaction, &receive, error)) return false;
    transaction.transmit = {0x55U};
    transaction.repeated_start = true;
    transaction.receive_length = 1U;
    if (!client.Transfer(transaction, &receive, error)) return false;
    if (receive != std::vector<std::uint8_t>{0xC3U}) {
        if (error != nullptr) *error = L"G16 boot-mode check expected C3; slave protocol is not confirmed.";
        return false;
    }
    transaction.transmit = EncodePacket(1U, 0U, {});
    transaction.receive_length = 7U;
    if (!client.Transfer(transaction, &receive, error)) return false;
    if (receive != EncodePacket(0x81U, 0U, {0U})) {
        if (error != nullptr) *error = L"G16 inquiry packet/status/checksum mismatch.";
        return false;
    }
    return true;
}
} // namespace firmware_upgrade::g16_i2c
