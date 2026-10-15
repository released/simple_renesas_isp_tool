#pragma once
#include "host_link_client.h"
#include "../../firmware/common/host_link_baud.h"

namespace firmware_upgrade::host_link {
struct LinkBaudInfo {
    std::uint32_t bootstrap=0, current=0, supported_mask=0, actual=0;
};
bool ReadLinkBaud(Client& client, LinkBaudInfo* info, std::wstring* error);
bool SwitchLinkBaud(Client& client, std::uint32_t desired,
                    const std::function<bool(std::uint32_t,std::wstring*)>& set_pc,
                    LinkBaudInfo* info, std::wstring* error);
}
