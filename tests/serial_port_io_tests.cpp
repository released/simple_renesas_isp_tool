#include "../src/core/serial_port.h"
#include "host_link_client.h"
#include <algorithm>
#include <deque>
#include <iostream>
#include <sstream>

// Compile the production SerialPort against a deterministic Win32 serial model.
// No real COM handle, driver setting, sleep, or hardware operation is used.
namespace serial_io_test {
struct Fragment {
    std::uint64_t at;
    std::deque<std::uint8_t> bytes;
};
struct Driver {
    std::uint64_t now = 0;
    DWORD error = ERROR_DEVICE_NOT_CONNECTED;
    COMMTIMEOUTS timeouts = {};
    std::deque<Fragment> incoming;
    std::vector<COMMTIMEOUTS> timeout_history;
    std::vector<std::uint8_t> written;
    bool fail_read = false, fail_timeouts = false;
    bool respond = false, fragment_reply = false, noisy_reply = false;
    unsigned reads = 0, writes = 0, sleeps = 0, closes = 0;
} driver;

void Schedule(const std::vector<std::uint8_t>& bytes, std::uint64_t at) {
    driver.incoming.push_back({at, {bytes.begin(), bytes.end()}});
}
DWORD Collect(void* output, DWORD capacity) {
    auto* bytes = static_cast<std::uint8_t*>(output);
    DWORD count = 0;
    while (!driver.incoming.empty() && driver.incoming.front().at <= driver.now && count < capacity) {
        auto& data = driver.incoming.front().bytes;
        while (!data.empty() && count < capacity) {
            bytes[count++] = data.front(); data.pop_front();
        }
        if (data.empty()) driver.incoming.pop_front();
    }
    return count;
}
HANDLE WINAPI Open(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE) {
    return reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(1U));
}
BOOL WINAPI GetState(HANDLE, LPDCB state) { state->DCBlength = sizeof(*state); return TRUE; }
BOOL WINAPI SetState(HANDLE, LPDCB) { return TRUE; }
BOOL WINAPI SetTimeouts(HANDLE, LPCOMMTIMEOUTS timeouts) {
    if (driver.fail_timeouts) return FALSE;
    driver.timeouts = *timeouts; driver.timeout_history.push_back(*timeouts); return TRUE;
}
BOOL WINAPI Setup(HANDLE, DWORD, DWORD) { return TRUE; }
BOOL WINAPI Purge(HANDLE, DWORD) { return TRUE; }
BOOL WINAPI Close(HANDLE) { ++driver.closes; return TRUE; }
DWORD WINAPI Tick() { return static_cast<DWORD>(driver.now); }
DWORD WINAPI Error() { return driver.error; }
void WINAPI SleepMs(DWORD ms) { ++driver.sleeps; driver.now += ms; }
BOOL WINAPI Read(HANDLE, LPVOID output, DWORD capacity, LPDWORD read, LPOVERLAPPED) {
    ++driver.reads; *read = 0;
    if (driver.fail_read) return FALSE;
    const auto& timeouts = driver.timeouts;
    const bool first_byte = timeouts.ReadIntervalTimeout == MAXDWORD &&
        timeouts.ReadTotalTimeoutMultiplier == MAXDWORD &&
        timeouts.ReadTotalTimeoutConstant > 0U && timeouts.ReadTotalTimeoutConstant < MAXDWORD;
    const bool immediate = timeouts.ReadIntervalTimeout == MAXDWORD &&
        timeouts.ReadTotalTimeoutMultiplier == 0U && timeouts.ReadTotalTimeoutConstant == 0U;
    const auto deadline = driver.now + timeouts.ReadTotalTimeoutConstant;
    *read = Collect(output, capacity);
    if (immediate || (first_byte && *read)) return TRUE;
    if (first_byte && !driver.incoming.empty()) {
        driver.now = (std::min)(deadline, driver.incoming.front().at);
        *read = Collect(output, capacity);
        return TRUE;
    }
    // The old 20ms/full-count mode waits even when a complete short frame exists.
    if (*read < capacity) {
        driver.now = deadline;
        *read += Collect(static_cast<std::uint8_t*>(output) + *read, capacity - *read);
    }
    return TRUE;
}
BOOL WINAPI Write(HANDLE, LPCVOID input, DWORD count, LPDWORD written, LPOVERLAPPED) {
    ++driver.writes; *written = count;
    const auto* bytes = static_cast<const std::uint8_t*>(input);
    driver.written.assign(bytes, bytes + count);
    if (!driver.respond) return TRUE;
    using namespace firmware_upgrade::host_link;
    Frame request, response;
    if (DecodeFrame(bytes, count, &request) != DecodeResult::Ok) return FALSE;
    response.type = FrameType::Response; response.sequence = request.sequence;
    response.command = request.command; response.status = Status::Ok;
    response.payload.assign(20U, 0xA5U);
    std::vector<std::uint8_t> wire; EncodeFrame(response, &wire);
    driver.now += 5U; // Fixed model TX cost, not a physical baud/USB measurement.
    if (driver.noisy_reply) {
        auto corrupt = wire; corrupt.back() ^= 1U;
        Schedule(corrupt, driver.now + 1U);
    }
    if (driver.fragment_reply) {
        Schedule({wire.begin(), wire.begin() + 3}, driver.now + 3U);
        Schedule({wire.begin() + 3, wire.begin() + 12}, driver.now + 4U);
        Schedule({wire.begin() + 12, wire.end()}, driver.now + 5U);
    } else Schedule(wire, driver.now + 3U);
    return TRUE;
}
} // namespace serial_io_test

#define CreateFileW serial_io_test::Open
#define GetCommState serial_io_test::GetState
#define SetCommState serial_io_test::SetState
#define SetCommTimeouts serial_io_test::SetTimeouts
#define SetupComm serial_io_test::Setup
#define PurgeComm serial_io_test::Purge
#define CloseHandle serial_io_test::Close
#define GetTickCount serial_io_test::Tick
#define GetLastError serial_io_test::Error
#define Sleep serial_io_test::SleepMs
#define ReadFile serial_io_test::Read
#define WriteFile serial_io_test::Write
#include "../src/core/serial_port.cpp"
#undef CreateFileW
#undef GetCommState
#undef SetCommState
#undef SetCommTimeouts
#undef SetupComm
#undef PurgeComm
#undef CloseHandle
#undef GetTickCount
#undef GetLastError
#undef Sleep
#undef ReadFile
#undef WriteFile

int TestSerialPortIo() {
    using namespace firmware_upgrade::host_link;
    using namespace serial_io_test;
    int failures = 0;
    const auto check = [&](bool pass, const char* name) {
        if (!pass) { ++failures; std::cerr << "FAIL: SerialPort " << name << '\n'; }
    };
    const auto reset = [&]() { driver = Driver{}; };
    std::wstring error; std::vector<std::uint8_t> bytes;
    {
        reset(); SerialPort port; check(port.Open(L"MOCK", 115200U, &error), "mock open");
        Schedule({1U, 2U, 3U}, 0U);
        check(port.ReadSome(&bytes, 512U, 50U, &error) && bytes.size() == 3U && driver.now == 0U,
              "buffered short reply returns without waiting20ms/full512bytes");
        Schedule({4U, 5U}, 7U);
        check(port.ReadSome(&bytes, 512U, 50U, &error) && bytes.size() == 2U && driver.now == 7U,
              "first arriving fragment returns immediately");
        check(driver.timeouts.ReadIntervalTimeout == MAXDWORD &&
              driver.timeouts.ReadTotalTimeoutMultiplier == MAXDWORD, "documented first-byte mode");
        const auto configured = driver.timeout_history.size();
        Schedule({6U}, driver.now);
        port.ReadSome(&bytes, 512U, 50U, &error);
        check(driver.timeout_history.size() == configured, "unchanged read timeout is cached");
        check(!port.ReadSome(&bytes, 512U, 7U, &error) && error == L"Read timeout." &&
              driver.now == 14U && bytes.empty(), "short caller deadline honored");
        const auto before = driver.now;
        check(!port.ReadSome(&bytes, 512U, 0U, &error) && driver.now == before,
              "zero deadline is nonblocking when empty");
        Schedule({7U}, driver.now);
        check(port.ReadSome(&bytes, 512U, 0U, &error) && bytes.size() == 1U && driver.now == before,
              "zero deadline drains already buffered bytes");
        driver.now = MAXDWORD - 3ULL;
        const auto wrap_start = driver.now;
        check(!port.ReadSome(&bytes, 512U, 9U, &error) && driver.now - wrap_start == 9U,
              "DWORD tick wrap preserves timeout");
        check(driver.sleeps == 0U, "no Sleep polling or busy-wait model");
        driver.fail_timeouts = true;
        check(!port.ReadSome(&bytes, 512U, 11U, &error) &&
              error.find(L"SetCommTimeouts") != std::wstring::npos, "timeout setup error visible");
        driver.fail_timeouts = false; driver.fail_read = true;
        check(!port.ReadSome(&bytes, 512U, 11U, &error) &&
              error.find(L"ReadFile") != std::wstring::npos, "unplug/read failure visible");
        port.Close();
        check(!port.ReadSome(&bytes, 512U, 10U, &error) && error == L"Serial port is not open.",
              "closed port rejected");
        check(!port.ReadSome(nullptr, 512U, 10U, &error) &&
              !port.ReadSome(&bytes, 0U, 10U, &error), "invalid output/capacity rejected");
    }
    {
        reset(); SerialPort port; port.Open(L"MOCK", 921600U, &error);
        const auto started = driver.now;
        check(!port.ReadSome(&bytes, 512U, 123U, &error) && driver.now - started == 123U &&
              driver.reads == 3U, "long idle read split into bounded50/50/23ms waits");
        check(driver.timeout_history.size() == 2U &&
              driver.timeouts.ReadTotalTimeoutConstant == 23U &&
              driver.timeouts.WriteTotalTimeoutConstant == 50U, "write timeout unchanged");
    }
    {
        reset(); SerialPort port; port.Open(L"MOCK", 115200U, &error);
        driver.respond = driver.fragment_reply = driver.noisy_reply = true;
        Client client; client.SetTransport(&port); Response response;
        check(client.RequestWithPolicy(Command::CanTxWaitRx, std::vector<std::uint8_t>(36U),
                  500U, 1U, &response, &error) && response.payload == std::vector<std::uint8_t>(20U, 0xA5U),
              "real Host Link parser reassembles partial response after CRC-bad frame");
        check(client.Stats().retries == 0U && driver.writes == 1U,
              "partial reads do not resend request");
        check(driver.now == 10U && driver.reads == 4U, "fragments returned at1/3/4/5ms arrival");
    }
    {
        reset(); SerialPort port; port.Open(L"MOCK", 115200U, &error);
        Frame first, second;
        first.type = FrameType::Event; first.sequence = 0U; first.command = 0xE0U;
        first.payload.assign(kMaximumPayloadSize, 0xA5U);
        second = first; second.type = FrameType::Response; second.sequence = 1U;
        second.payload.assign(kMaximumPayloadSize, 0x5AU);
        std::vector<std::uint8_t> wire, tail;
        check(EncodeFrame(first, &wire) && EncodeFrame(second, &tail), "maximum frame fixtures encode");
        wire.insert(wire.end(), tail.begin(), tail.end()); Schedule(wire, 0U);
        FrameStreamParser parser; bool ok = true;
        while (!driver.incoming.empty() && ok) {
            ok = port.ReadSome(&bytes, 17U, 50U, &error) && bytes.size() <= 17U;
            parser.Push(bytes.data(), bytes.size());
        }
        Frame a, b, extra;
        check(ok && driver.now == 0U && parser.TryPop(&a) && parser.TryPop(&b) &&
              !parser.TryPop(&extra) && a.payload == first.payload && b.payload == second.payload,
              "coalesced maximum128-byte frames survive bounded17-byte partial reads");
    }
    {
        reset(); SerialPort port; port.Open(L"MOCK", 460800U, &error);
        driver.respond = true; Client client; client.SetTransport(&port);
        client.SetCancelCheck([] { return false; });
        Response response; bool ok = true;
        for (unsigned i = 0; i < 4806U && ok; ++i)
            ok = client.RequestWithPolicy(Command::CanTxWaitRx, std::vector<std::uint8_t>(36U),
                                          500U, 1U, &response, &error);
        check(ok && client.Stats().requests == 4806U && !client.Stats().retries &&
              !client.Stats().response_timeouts, "4806 real SerialPort/Host Link exchanges");
        check(driver.now == 4806ULL * 8U && driver.sleeps == 0U && driver.timeout_history.size() == 1U,
              "4806 transactions add no fixed read-delay in deterministic model");
    }
    {
        reset(); SerialPort port; port.Open(L"MOCK", 115200U, &error);
        Client client; client.SetTransport(&port);
        client.SetCancelCheck([] { return driver.now >= 50U; });
        Response response;
        check(!client.RequestWithPolicy(Command::Ping, {}, 1000U, 1U, &response, &error) &&
              error.find(L"canceled") != std::wstring::npos && driver.now == 50U && driver.writes == 1U,
              "cancellation is checked after bounded idle wait without retry");
    }
    std::cout << "Real SerialPort / Win32 I/O model / fragmented parser /4806 exchanges: "
              << (failures ? "FAIL" : "PASS") << '\n';
    return failures;
}
