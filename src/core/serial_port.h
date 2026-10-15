#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "host_link_transport.h"

namespace firmware_upgrade::host_link {

class SerialPort : public firmware_upgrade::host_link::Transport {
public:
    SerialPort();
    ~SerialPort();

    bool Open(const std::wstring& com_port, std::uint32_t baudrate, std::wstring* error);
    bool ChangeBaud(std::uint32_t baudrate, std::wstring* error);
    void Close();
    bool IsOpen() const override;

    bool WriteAll(const std::uint8_t* data, std::size_t len, std::uint32_t timeout_ms, std::wstring* error) override;
    bool ReadSome(std::vector<std::uint8_t>* out, std::size_t max_len, std::uint32_t timeout_ms, std::wstring* error) override;
    bool Purge(std::wstring* error);

private:
    HANDLE handle_;
    DWORD read_wait_ms_;
};

} // namespace firmware_upgrade::host_link
