#include "main_frame.h"
#include <iomanip>
#include <sstream>

std::wstring CMainFrame::BuildI2cSummary() const {
    if(IsI2cIsp())return BuildUartSummary();
    if (!IsI2cTab()) return L"Bus TAB: CAN\r\n";
    std::wstringstream text;
    CString address; i2c_address_edit_.GetWindowTextW(address);
    text << L"Bus TAB: I2C\r\nI2C UI selection: port " << unsigned(preferred_i2c_port_)
         << L", speed preset " << unsigned(preferred_i2c_speed_) << L", 7-bit address "
         << address.GetString() << L"\r\n";
    firmware_upgrade::i2c_host::Configuration configuration;
    std::uint8_t selected_address = 0U;
    const bool current = session_state_.i2c_probe_validated &&
        SelectedI2cSettings(&configuration, &selected_address) &&
        configuration.port == session_state_.i2c_configuration.port &&
        configuration.speed == session_state_.i2c_configuration.speed &&
        selected_address == session_state_.i2c_address;
    text << L"I2C target (current settings): " << (current ? session_state_.i2c_summary : L"N/A - probe required") << L"\r\n";
    if (current) for (const auto& area : session_state_.i2c_metadata.areas)
        text << L"LIVE area " << unsigned(area.kind) << L": 0x" << std::hex
             << area.start << L"..0x" << area.end << std::dec << L", erase/write/read "
             << area.erase_unit << L"/" << area.write_unit << L"/" << area.read_unit << L" B\r\n";
    text << L"G16 BIN bytes: " << binary_data_.size() << L"\r\n";
    if (binary_i2c_inspection_.available)
        text << L"CRC16/XMODEM calculated/stored: 0x" << std::hex << std::uppercase
             << binary_i2c_inspection_.calculated_crc16 << L" / 0x"
             << binary_i2c_inspection_.stored_crc16 << L", "
             << (binary_i2c_inspection_.matches ? L"MATCH" : L"MISMATCH") << L"\r\n";
    else text << L"CRC16: N/A\r\n";
    text << L"BIN identity/version/start: N/A - no manifest\r\n"
         << L"Programming: BLOCKED - " << firmware_upgrade::g16_i2c::ProgrammingBlocker() << L"\r\n"
         << L"The following CAN fields are inactive on the I2C TAB.\r\n";
    return text.str();
}
