#include "serial_port.h"

#include <sstream>

namespace firmware_upgrade::host_link {

namespace {

constexpr DWORD kMaximumReadWaitMs = 50U;

COMMTIMEOUTS SerialTimeouts(DWORD read_wait_ms) {
    COMMTIMEOUTS timeouts = {};
    // Return buffered data immediately, otherwise wait only for the first byte.
    // A zero deadline uses the documented nonblocking drain mode instead.
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = read_wait_ms ? MAXDWORD : 0U;
    timeouts.ReadTotalTimeoutConstant = read_wait_ms;
    timeouts.WriteTotalTimeoutConstant = 50U;
    return timeouts;
}

std::wstring WinErr(const wchar_t* where) {
    const DWORD code = GetLastError();
    std::wstringstream ss;
    ss << where << L" failed (GetLastError=" << code << L")";
    return ss.str();
}

} // namespace

SerialPort::SerialPort() : handle_(INVALID_HANDLE_VALUE), read_wait_ms_(MAXDWORD) {}

SerialPort::~SerialPort() {
    Close();
}

bool SerialPort::Open(const std::wstring& com_port, std::uint32_t baudrate, std::wstring* error) {
    Close();

    const std::wstring path = L"\\\\.\\" + com_port;
    handle_ = CreateFileW(path.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          nullptr,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        if (error != nullptr) {
            *error = WinErr(L"CreateFileW");
        }
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
        if (error != nullptr) {
            *error = WinErr(L"GetCommState");
        }
        Close();
        return false;
    }

    dcb.BaudRate = baudrate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    if (!SetCommState(handle_, &dcb)) {
        if (error != nullptr) {
            *error = WinErr(L"SetCommState");
        }
        Close();
        return false;
    }

    COMMTIMEOUTS to = SerialTimeouts(kMaximumReadWaitMs);
    if (!SetCommTimeouts(handle_, &to)) {
        if (error != nullptr) {
            *error = WinErr(L"SetCommTimeouts");
        }
        Close();
        return false;
    }
    read_wait_ms_ = kMaximumReadWaitMs;

    if (!SetupComm(handle_, 4096, 4096)) {
        if (error != nullptr) {
            *error = WinErr(L"SetupComm");
        }
        Close();
        return false;
    }

    if (!PurgeComm(handle_, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR)) {
        if (error != nullptr) {
            *error = WinErr(L"PurgeComm");
        }
        Close();
        return false;
    }

    return true;
}

bool SerialPort::ChangeBaud(std::uint32_t baudrate, std::wstring* error) {
    DCB dcb={}; dcb.DCBlength=sizeof(dcb);
    if (!IsOpen() || !GetCommState(handle_,&dcb)) {
        if (error) *error=WinErr(L"GetCommState for baud switch");
        return false;
    }
    dcb.BaudRate=baudrate;
    if (!SetCommState(handle_,&dcb)) {
        if (error) *error=WinErr(L"SetCommState for baud switch");
        return false;
    }
    Sleep(20U); // Allow peer TX completion and the FTDI control request to settle.
    return true;
}

void SerialPort::Close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    read_wait_ms_ = MAXDWORD;
}

bool SerialPort::IsOpen() const {
    return handle_ != INVALID_HANDLE_VALUE;
}

bool SerialPort::WriteAll(const std::uint8_t* data, std::size_t len, std::uint32_t timeout_ms, std::wstring* error) {
    if (!IsOpen()) {
        if (error != nullptr) {
            *error = L"Serial port is not open.";
        }
        return false;
    }
    if (len == 0) {
        return true;
    }

    const DWORD started = GetTickCount();
    std::size_t offset = 0;

    while (offset < len) {
        DWORD written = 0;
        if (!WriteFile(handle_, data + offset, static_cast<DWORD>(len - offset), &written, nullptr)) {
            if (error != nullptr) {
                *error = WinErr(L"WriteFile");
            }
            return false;
        }
        offset += written;

        if (GetTickCount() - started > timeout_ms) {
            if (error != nullptr) {
                *error = L"Write timeout.";
            }
            return false;
        }
    }

    return true;
}

bool SerialPort::ReadSome(std::vector<std::uint8_t>* out, std::size_t max_len, std::uint32_t timeout_ms, std::wstring* error) {
    if (out == nullptr || max_len == 0) {
        return false;
    }
    out->clear();

    if (!IsOpen()) {
        if (error != nullptr) {
            *error = L"Serial port is not open.";
        }
        return false;
    }

    const DWORD started = GetTickCount();
    std::vector<std::uint8_t> tmp(max_len, 0);

    while (true) {
        const DWORD elapsed = GetTickCount() - started;
        const DWORD remaining = elapsed < timeout_ms ? timeout_ms - elapsed : 0U;
        const DWORD wait_ms = remaining < kMaximumReadWaitMs ? remaining : kMaximumReadWaitMs;
        if (read_wait_ms_ != wait_ms) {
            COMMTIMEOUTS timeouts = SerialTimeouts(wait_ms);
            if (!SetCommTimeouts(handle_, &timeouts)) {
                if (error != nullptr) *error = WinErr(L"SetCommTimeouts for read");
                return false;
            }
            read_wait_ms_ = wait_ms;
        }
        DWORD read = 0;
        if (!ReadFile(handle_, tmp.data(), static_cast<DWORD>(tmp.size()), &read, nullptr)) {
            if (error != nullptr) {
                *error = WinErr(L"ReadFile");
            }
            return false;
        }

        if (read > 0) {
            out->assign(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(read));
            return true;
        }

        if (GetTickCount() - started >= timeout_ms) {
            if (error != nullptr) {
                *error = L"Read timeout.";
            }
            return false;
        }

    }
}

bool SerialPort::Purge(std::wstring* error) {
    if (!IsOpen()) {
        if (error != nullptr) {
            *error = L"Serial port is not open.";
        }
        return false;
    }

    if (!PurgeComm(handle_, PURGE_RXABORT | PURGE_RXCLEAR | PURGE_TXABORT | PURGE_TXCLEAR)) {
        if (error != nullptr) {
            *error = WinErr(L"PurgeComm");
        }
        return false;
    }

    return true;
}

} // namespace firmware_upgrade::host_link

