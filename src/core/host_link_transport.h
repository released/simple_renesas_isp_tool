#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace firmware_upgrade::host_link {

class Transport {
public:
    virtual ~Transport() = default;

    virtual bool IsOpen() const = 0;
    virtual bool WriteAll(const std::uint8_t* data,
                          std::size_t length,
                          std::uint32_t timeout_ms,
                          std::wstring* error) = 0;
    virtual bool ReadSome(std::vector<std::uint8_t>* output,
                          std::size_t maximum_length,
                          std::uint32_t timeout_ms,
                          std::wstring* error) = 0;
};

} // namespace firmware_upgrade::host_link
