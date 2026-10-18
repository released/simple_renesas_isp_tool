#include "serial_port_catalog.h"

#include <iostream>

int TestSerialPortCatalog() {
    using firmware_upgrade::serial_ports::BuildCatalog;
    using firmware_upgrade::serial_ports::PortInfo;
    int failures = 0;
    const auto check = [&failures](bool condition, const char* name) {
        if (!condition) {
            std::cerr << "FAIL: " << name << '\n';
            ++failures;
        }
    };
    const auto ports = BuildCatalog(
        {L"COM12", L"COM8", L"COM6", L"com6", L"COM257", L"LPT1",
         L"COM0", L"COM", L"COM6: USB Serial Port", L"COM4294967296"},
        {{L"COM6", L"USB Serial Port (COM6)"},
         {L"COM6", L"Duplicate name must not replace first"},
         {L"COM8", L""}, {L"COM8", L"Nu-Link Virtual Com Port (COM8)"},
         {L"COM99", L"Disconnected device"}});
    check(ports.size() == 4U, "COM catalog validates and deduplicates port names");
    if (ports.size() == 4U) {
        check(ports[0].port_name == L"COM6" && ports[1].port_name == L"COM8" &&
                  ports[2].port_name == L"COM12" && ports[3].port_name == L"COM257",
              "COM catalog sorts numerically and supports numbers above 256");
        check(ports[0].DisplayName() == L"COM6: USB Serial Port (COM6)",
              "COM display uses Windows friendly name");
        check(ports[0].port_name == L"COM6",
              "COM display cannot change transport or INI port name");
        check(ports[1].DisplayName() == L"COM8: Nu-Link Virtual Com Port (COM8)",
              "COM catalog fills missing metadata without hiding a valid port");
        check(ports[2].DisplayName() == L"COM12",
              "COM catalog falls back to plain name without device metadata");
    }
    check(BuildCatalog({}, {{L"COM6", L"Stale device"}}).empty(),
          "COM catalog does not list absent device names");
    using firmware_upgrade::serial_ports::DescribePreviousDevice;
    const std::vector<PortInfo> hints={{L"COM6",L"Adapter",L"USB\\A"},{L"COM9",L"Adapter",L"USB\\B"}};
    check(DescribePreviousDevice(hints,L"COM6",L"USB\\A").empty(),"same instance/port does not imply a new connection");
    check(DescribePreviousDevice(hints,L"COM8",L"USB\\A").find(L"COM6")!=std::wstring::npos,"moved adapter is only a hint");
    check(DescribePreviousDevice(hints,L"COM6",L"USB\\B").find(L"changed")!=std::wstring::npos,"reused COM with different instance flagged");
    check(DescribePreviousDevice({},L"COM6",L"USB\\A").find(L"absent")!=std::wstring::npos,"unplugged adapter flagged");
    check(DescribePreviousDevice({hints[0],{L"COM7",L"Adapter",L"USB\\A"}},L"COM6",L"USB\\A").find(L"ambiguous")!=std::wstring::npos,"duplicate instance hints are not trusted");
    check(DescribePreviousDevice(hints,L"COM6",L"").empty(),"missing PnP hint does not invent an identity");
    check(PortInfo{L"COM7", L"JLink CDC UART Port (COM7)"}.DisplayName() ==
              L"COM7: JLink CDC UART Port (COM7)",
          "COM catalog preserves driver descriptions verbatim");
    check(PortInfo{L"COM9", L"\u6e2c\u8a66\u88dd\u7f6e (COM9)"}.DisplayName() ==
              L"COM9: \u6e2c\u8a66\u88dd\u7f6e (COM9)",
          "COM catalog preserves Unicode descriptions");

    // Read-only integration check; never opens or sends bytes to any COM port.
    const auto actual = firmware_upgrade::serial_ports::Enumerate();
    for (const auto& port : actual) {
        check(port.DisplayName().find(port.port_name) == 0U,
              "Enumerated COM display begins with canonical port name");
        std::wcout << L"COM catalog: " << port.DisplayName() << L'\n';
    }
    std::cout << "COM catalog checks complete; enumerated " << actual.size()
              << " local port(s)\n";
    return failures;
}
