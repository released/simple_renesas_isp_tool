#include "overview_model.h"
#include <iomanip>
#include <sstream>
#include <cstring>

namespace firmware_upgrade::overview {
namespace {
std::wstring Hex(std::uint32_t value) {
    std::wstringstream out;
    out << L"0x" << std::hex << std::uppercase << std::setfill(L'0') << std::setw(8) << value;
    return out.str();
}
std::wstring Version(std::uint32_t value) {
    if (value == 0xFFFFFFFFU) return L"N/A - not installed";
    return std::to_wstring((value >> 16U) & 0xFFU) + L"." +
        std::to_wstring((value >> 8U) & 0xFFU) + L"." + std::to_wstring(value & 0xFFU);
}
std::wstring Number(std::uint32_t value) {
    return value == 0xFFFFFFFFU ? L"N/A" : std::to_wstring(value);
}
std::wstring Board(std::uint16_t id) {
    switch (id) {
    case 0x0101U: return L"RL78/F24";
    case 0x0201U: return L"RA2L1";
    case 0x0202U: return L"RA6";
    default: return L"Bridge " + Hex(id);
    }
}
std::wstring Pair(std::uint8_t major, std::uint8_t minor) {
    return std::to_wstring(major) + L"." + std::to_wstring(minor);
}
void ApplyImagePolicy(const Input& in, Card* bin) {
    image_policy::Policy policy;
    bool known=false, valid=false;
    if (in.image.metadata_available) {
        known=true; valid=in.image.metadata_valid && in.image.checksum_matches;
        if (valid) {
            const auto& m=in.image.metadata; policy=m.policy;
            bin->value=L"v"+Version(m.version)+L" | "+std::to_wstring(in.bin_size)+L" B";
            bin->detail=m.mcu_family+L" / "+m.mcu_part+L" / build "+Number(m.build);
        } else bin->detail=in.image.metadata_error;
    } else if (in.bridge_image.available) {
        known=true; valid=in.bridge_image.valid;
        if (valid) {
            const auto& m=in.bridge_image.metadata;
            policy.role=m.image_role; policy.transports=m.update_transports;
            policy.irq_abi=m.irq_abi; policy.minimum_boot_version=m.minimum_boot_version;
            bin->value=L"v"+Version(m.version)+L" | "+std::to_wstring(in.bin_size)+L" B";
            bin->detail=std::wstring(m.part,m.part+strlen(m.part));
        } else bin->detail=in.bridge_image.error;
    }
    if (!known) return;
    bin->heading=L"LOADED APP BIN | OFFLINE";
    bin->signal=valid?Signal::Offline:Signal::Invalid;
    if (!valid) { bin->heading+=L" | INVALID"; return; }
    const auto route=static_cast<std::uint8_t>(in.misc?IP_UART:(in.i2c?IP_I2C:IP_CAN));
    std::wstring error;
    const bool allowed=image_policy::Allows(policy,in.misc?IP_ROLE_BRIDGE_APP:IP_ROLE_DEVICE_APP,route,&error);
    bin->detail+=L"\n"+std::wstring(policy.role==IP_ROLE_BRIDGE_APP?L"HOST APP":L"DEVICE APP")+
        L" | allowed "+image_policy::Transports(policy.transports)+image_policy::DeviceUartText(policy)+L" | IRQ ABI "+Hex(policy.irq_abi);
    if (!allowed) { bin->signal=Signal::Invalid; bin->heading+=L" | WRONG ROUTE"; bin->detail+=L"\n"+error; }
}
}
std::array<Card, 5> Build(const Input& in) {
    std::array<Card, 5> out = {{
        {L"HOST BRIDGE | REQUIRED", L"N/A - not verified", L"Open + Probe Host; COM description is not identity."},
        {L"DEVICE TARGET | REQUIRED", L"N/A - not discovered", L"Discover the selected target; BIN identity is offline only."},
        {L"BOOT CODE | LIVE INFO", L"N/A", L"Version / build / boot reserve require device discovery."},
        {L"INSTALLED APP | LIVE INFO", L"N/A", L"Version / build / installed size require device discovery."},
        {L"LOADED APP BIN | OFFLINE", L"N/A", L"Load Update Image; no file is loaded."}
    }};
    if (in.host_current) {
        out[0] = {L"HOST BRIDGE | VERIFIED LINK", Board(in.host.board_id) + L" | " + in.port,
            L"FW " + Pair(in.host.firmware_major, in.host.firmware_minor) + L"." +
            std::to_wstring(in.host.firmware_patch) + L" / build " + std::to_wstring(in.host.firmware_build) +
            L" / board " + Hex(in.host.board_id) + L" / Host Link v2", Signal::Verified};
        if (in.bridge_current) {
            out[0].value=std::wstring(in.bridge.family,in.bridge.family+strlen(in.bridge.family))+L" / "+
                std::wstring(in.bridge.part,in.bridge.part+strlen(in.bridge.part))+L" | "+in.port;
        }
    }
    if (in.misc) {
        out[1]={L"UPDATE DESTINATION | HOST BRIDGE",L"PC -> bridge via UART",
            L"Not the external CAN / I2C device. Boot flash is never updated here.",Signal::Reported};
        out[2]={L"HOST BOOT | LIVE INFO",L"N/A",L"Probe a self-update capable bridge."};
        out[3]={L"HOST APP | LIVE INFO",L"N/A",L"No bridge app descriptor available."};
        if (in.host_current && in.bridge_current) {
            const auto& b=in.bridge;
            out[1].value=b.mode==BU_MODE_BOOT ? L"HOST BRIDGE - BOOT mode" : L"HOST BRIDGE - APP mode";
            out[2]={L"HOST BOOT | LIVE INFO",L"v"+Version(b.boot_version),
                L"Build "+Number(b.boot_build)+L" / reserve "+Number(b.boot_size)+L" B",Signal::Verified};
            out[3]={L"HOST APP | LIVE CRC CHECK",(b.flags&BU_FLAG_VALID_APP) ? L"v"+Version(b.app_version) : L"N/A - invalid / absent",
                L"Build "+Number(b.app_build)+L" / installed "+Number(b.installed_size)+L" B / start "+Hex(b.app_start),
                (b.flags&BU_FLAG_VALID_APP) ? Signal::Verified : Signal::Reported};
        }
        if (in.bin_size) {
            const auto& image=in.bridge_image;
            out[4]={L"HOST APP BIN | OFFLINE",std::to_wstring(in.bin_size)+L" B",image.error,
                image.valid ? Signal::Offline : Signal::Invalid};
            if (image.valid) {
                out[4].value=L"v"+Version(image.metadata.version)+L" | "+std::to_wstring(in.bin_size)+L" B";
                out[4].detail=std::wstring(image.metadata.part,image.metadata.part+strlen(image.metadata.part))+
                    L" / build "+Number(image.metadata.build)+L" / BUR1 CRC32 MATCH";
            }
        }
        ApplyImagePolicy(in,&out[4]);
        return out;
    }
    // A stale snapshot must not survive disconnect or selection changes.
    if (in.host_current && in.target_current) {
        if (in.kind == TargetKind::CanUpgrade) {
            const auto& t = in.target;
            const bool boot = t.state == f13_boot::DeviceState::Bootloader;
            out[1] = {boot ? L"DEVICE TARGET | BOOTLOADER MODE" : L"DEVICE TARGET | APPLICATION MODE",
                t.mcu_family + L" / " + t.mcu_part,
                L"Target " + Hex(t.target_id) + L" / protocol " + std::to_wstring(t.protocol_version) +
                L" / live INFO (not authenticated)", Signal::Verified};
            out[2] = {L"BOOT CODE | LIVE INFO", L"v" + Version(t.boot_version),
                L"Build " + Number(t.boot_build) + L" / reserve " + Number(t.boot_reserved_size) +
                L" B\nApp start " + Hex(t.application_start), Signal::Verified};
            const bool installed = t.app_version != 0xFFFFFFFFU && t.installed_app_size != 0xFFFFFFFFU;
            out[3] = {L"INSTALLED APP | LIVE INFO", installed ? L"v" + Version(t.app_version) : L"N/A - not installed",
                L"Build " + Number(t.app_build) + L" / installed " + Number(t.installed_app_size) +
                L" B\nCapacity " + Number(t.application_capacity) + L" B; not read-back CRC",
                installed ? Signal::Verified : Signal::Reported};
        } else if (in.kind == TargetKind::Transfer) {
            out[1] = {L"DEVICE TARGET | CUSTOM TRANSFER", std::wstring(in.transfer.part_number.begin(), in.transfer.part_number.end()),
                L"Target " + Hex(in.transfer.target_id) + L" / protocol " + Pair(in.transfer.protocol_major, in.transfer.protocol_minor) +
                L" / non-destructive benchmark", Signal::Verified};
            out[2].detail = out[3].detail = L"N/A - Custom Transfer does not report boot/app metadata.";
        } else if (in.kind == TargetKind::G16Reference) {
            out[1] = {L"DEVICE TARGET | REFERENCE ONLY", in.g16.label,
                L"Reported sample label; not authenticated. Programming BLOCKED.", Signal::Reported};
            out[2] = {L"BOOT CODE | REPORTED CONSTANT", L"v" + Pair(in.g16.boot_major, in.g16.boot_minor),
                L"G16 reference signature; build / boot reserve N/A.", Signal::Reported};
            out[3] = {L"INSTALLED APP | REPORTED CONSTANT", L"v" + Pair(in.g16.app_major, in.g16.app_minor),
                L"G16 reference signature; actual installed size N/A.", Signal::Reported};
        }
    }
    if (in.bin_size != 0U) {
        auto& bin = out[4];
        bin.value = std::to_wstring(in.bin_size) + L" bytes";
        bin.signal = Signal::Offline;
        if (in.i2c) {
            const bool match = in.i2c_image.available && in.i2c_image.matches;
            bin.heading += match ? L" | CRC16 MATCH" : L" | CRC16 INVALID";
            bin.detail = L"Raw G16 BIN: MCU / version / start N/A.\nCRC match does not enable programming.";
            if (!match) bin.signal = Signal::Invalid;
        } else {
            const bool valid = in.image.metadata_valid && in.image.checksum_available && in.image.checksum_matches;
            bin.heading += valid ? L" | CRC32 MATCH" : L" | INVALID";
            if (!valid) bin.signal = Signal::Invalid;
            if (in.image.metadata_valid) {
                const auto& m = in.image.metadata;
                bin.value = L"v" + Version(m.version) + L" | " + std::to_wstring(in.bin_size) + L" B";
                bin.detail = m.mcu_family + L" / " + m.mcu_part + L" / build " + std::to_wstring(m.build) +
                    L"\nStart " + Hex(m.application_start) + L"; see validation fields below.";
            } else bin.detail = L"MCU / app version N/A - valid manifest required.\nSee CRC and metadata validation below.";
        }
    }
    ApplyImagePolicy(in,&out[4]);
    return out;
}
} // namespace firmware_upgrade::overview
