#include "worker_session.h"
#include "core/g16_i2c_reference.h"
#include <algorithm>
#include <sstream>

namespace firmware_upgrade::ui {
bool WorkerSession::RefreshBridgePorts(std::wstring* error) {
    i2c_client_.Reset();
    state_.i2c_probe_validated = false;
    state_.i2c_metadata = {};
    return bridge::GetPorts(host_link_client_, state_.host_capabilities,
               bridge::Bus::Can, &state_.can_ports, error) &&
           bridge::GetPorts(host_link_client_, state_.host_capabilities,
               bridge::Bus::I2c, &state_.i2c_ports, error);
}
bool WorkerSession::PrepareCanPort(const CanSettings& settings,
                                   std::wstring* error) {
    const auto found = std::find_if(state_.can_ports.begin(), state_.can_ports.end(),
        [&settings](const bridge::Port& p) { return p.id == settings.port; });
    if (!bridge::CanPortSupports(state_.host_capabilities,state_.can_ports,settings.port,
            settings.nominal_bitrate_preset,settings.mode==can_host::Mode::CanFd)) {
        *error = L"Selected CAN port does not support this mode/rate.";
        return false;
    }
    if (!bridge::SelectCanPort(host_link_client_, state_.host_capabilities,
                               state_.can_ports, settings.port, error)) return false;
    PostLog(L"Selected bridge CAN port: " + found->label + L".");
    return true;
}
bool WorkerSession::ProbeI2c(const i2c_host::Configuration& configuration,
                            std::uint8_t address) {
    Command command;
    command.type = CommandType::ProbeI2c;
    command.i2c_configuration = configuration;
    command.i2c_address = address;
    return Submit(std::move(command));
}
void WorkerSession::AbortI2cBestEffort() {
    if (!state_.host_validated || !serial_port_.IsOpen() ||
        (state_.host_capabilities.feature_flags & bridge::kFeatureI2c) == 0U)
        return;
    const bool was_cleanup = cleanup_in_progress_;
    cleanup_in_progress_ = true;
    std::wstring error;
    if (!i2c_client_.Abort(&error)) PostLog(L"I2C abort warning: " + error);
    cleanup_in_progress_ = was_cleanup;
    i2c_client_.Reset();
}
bool WorkerSession::ProcessProbeI2c(const Command& command, std::wstring* error) {
    state_.i2c_probe_validated = false;
    state_.i2c_summary.clear();
    state_.i2c_metadata = {};
    if (!state_.host_validated || state_.can_started ||
        (state_.host_capabilities.feature_flags & bridge::kFeatureI2c) == 0U ||
        command.i2c_address < 8U || command.i2c_address > 0x77U) {
        *error = L"I2C probe requires an I2C-capable bridge, CAN stopped and address08..77.";
        return false;
    }
    AbortI2cBestEffort();
    PostLog(L"I2C reference probe: explicitly selected address, no address scan, "
            L"no authentication/erase/write/app jump commands.");
    if (!i2c_client_.Configure(command.i2c_configuration, state_.i2c_ports, error) ||
        !g16_i2c::Discover(i2c_client_, command.i2c_address,
                           &state_.i2c_metadata, error)) {
        AbortI2cBestEffort();
        PostState();
        return false;
    }
    std::wstringstream summary;
    summary << L"G16 reference " << state_.i2c_metadata.label
            << L" (label, not authenticated MCU part), boot "
            << static_cast<unsigned>(state_.i2c_metadata.boot_major) << L"."
            << static_cast<unsigned>(state_.i2c_metadata.boot_minor)
            << L", port "
            << static_cast<unsigned>(command.i2c_configuration.port)
            << L", 7-bit address 0x" << std::hex
            << static_cast<unsigned>(command.i2c_address)
            << L"; geometry received, programming blocked.";
    state_.i2c_configuration = command.i2c_configuration;
    state_.i2c_address = command.i2c_address;
    state_.i2c_summary = summary.str();
    state_.i2c_probe_validated = true;
    PostLog(state_.i2c_summary);
    PostState();
    return true;
}
} // namespace firmware_upgrade::ui
