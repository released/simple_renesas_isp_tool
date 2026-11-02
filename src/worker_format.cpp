#include "worker_session.h"
#include "worker_format.h"
#include "core/firmware_update_profile.h"
#include "core/can_image_contract.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>
namespace firmware_upgrade::ui::worker_detail {
std::wstring Hex16(std::uint16_t value) {
    std::wstringstream stream;
    stream << L"0x" << std::hex << std::uppercase << std::setfill(L'0')
           << std::setw(4) << value;
    return stream.str();
}

std::wstring Hex32(std::uint32_t value) {
    std::wstringstream stream;
    stream << L"0x" << std::hex << std::uppercase << std::setfill(L'0')
           << std::setw(8) << value;
    return stream.str();
}

std::wstring NarrowAsciiToWide(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}

std::wstring HexBytes(const std::vector<std::uint8_t>& bytes) {
    std::wstringstream stream;
    stream << std::hex << std::uppercase << std::setfill(L'0');
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        if (index != 0U) {
            stream << L' ';
        }
        stream << std::setw(2) << static_cast<unsigned>(bytes[index]);
    }
    return stream.str();
}

std::wstring CanChannelSourcesText(std::uint16_t sources) {
    std::wstringstream stream;
    bool has_name = false;
    const std::uint16_t unknown = static_cast<std::uint16_t>(
        sources & ~host_link::kCanChannelSourceKnownMask);
    const auto append = [&stream, &has_name](const wchar_t* name) {
        if (has_name) {
            stream << L'|';
        }
        stream << name;
        has_name = true;
    };

    if ((sources & host_link::kCanChannelSourceBusError) != 0U) append(L"BUS_ERROR");
    if ((sources & host_link::kCanChannelSourceErrorWarning) != 0U) append(L"ERROR_WARNING");
    if ((sources & host_link::kCanChannelSourceErrorPassive) != 0U) append(L"ERROR_PASSIVE");
    if ((sources & host_link::kCanChannelSourceBusOffEntry) != 0U) append(L"BUS_OFF_ENTRY");
    if ((sources & host_link::kCanChannelSourceBusOffRecovery) != 0U) append(L"BUS_OFF_RECOVERY");
    if ((sources & host_link::kCanChannelSourceOverload) != 0U) append(L"OVERLOAD");
    if ((sources & host_link::kCanChannelSourceBusLock) != 0U) append(L"BUS_LOCK");
    if ((sources & host_link::kCanChannelSourceArbitrationLost) != 0U) append(L"ARBITRATION_LOST");
    if ((sources & host_link::kCanChannelSourceTdcViolation) != 0U) append(L"TDC_VIOLATION");
    if (unknown != 0U) {
        if (has_name) {
            stream << L'|';
        }
        stream << L"UNKNOWN(" << Hex16(unknown) << L')';
        has_name = true;
    }
    if (!has_name) {
        stream << L"NONE";
    }
    return stream.str();
}

std::wstring CanGlobalSourcesText(std::uint16_t sources) {
    std::wstringstream stream;
    bool has_name = false;
    const std::uint16_t unknown = static_cast<std::uint16_t>(
        sources & ~host_link::kCanGlobalSourceKnownMask);
    const auto append = [&stream, &has_name](const wchar_t* name) {
        if (has_name) {
            stream << L'|';
        }
        stream << name;
        has_name = true;
    };

    if ((sources & host_link::kCanGlobalSourceDlcError) != 0U) append(L"DLC_ERROR");
    if ((sources & host_link::kCanGlobalSourceMessageLost) != 0U) append(L"MESSAGE_LOST");
    if ((sources & host_link::kCanGlobalSourceTxHistoryLost) != 0U) append(L"TX_HISTORY_LOST");
    if ((sources & host_link::kCanGlobalSourcePayloadOverflow) != 0U) append(L"PAYLOAD_OVERFLOW");
    if ((sources & host_link::kCanGlobalSourceRxFifo0Lost) != 0U) append(L"RX_FIFO0_LOST");
    if ((sources & host_link::kCanGlobalSourceRxFifo1Lost) != 0U) append(L"RX_FIFO1_LOST");
    if ((sources & host_link::kCanGlobalSourceCommonFifoLost) != 0U) append(L"COMMON_FIFO_LOST");
    if (unknown != 0U) {
        if (has_name) {
            stream << L'|';
        }
        stream << L"UNKNOWN(" << Hex16(unknown) << L')';
        has_name = true;
    }
    if (!has_name) {
        stream << L"NONE";
    }
    return stream.str();
}

std::wstring VersionText(std::uint32_t version) {
    if (version == 0xFFFFFFFFU) {
        return L"not installed";
    }
    return std::to_wstring((version >> 16U) & 0xFFU) + L"." +
           std::to_wstring((version >> 8U) & 0xFFU) + L"." +
           std::to_wstring(version & 0xFFU);
}

bool ImageMatchesTarget(
    const firmware_update::ImageInfo& image,
    const f13_boot::TargetInfo& target,
    std::wstring* error) {
    return can_contract::Matches(image, target, error);
}

std::uint16_t ReadLe16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[0]) |
        (static_cast<std::uint16_t>(data[1]) << 8U));
}

std::uint32_t ReadLe32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           (static_cast<std::uint32_t>(data[1]) << 8U) |
           (static_cast<std::uint32_t>(data[2]) << 16U) |
           (static_cast<std::uint32_t>(data[3]) << 24U);
}

}

