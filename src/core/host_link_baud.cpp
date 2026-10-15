#include "host_link_baud.h"
#include "image_policy.h"

namespace firmware_upgrade::host_link {
namespace {
std::vector<std::uint8_t> Rate(std::uint32_t n) {
    return {static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(n>>8),
            static_cast<std::uint8_t>(n>>16),static_cast<std::uint8_t>(n>>24)};
}
bool Fail(std::wstring* error,const wchar_t* text) {
    if (error) *error=text;
    return false;
}
}
bool ReadLinkBaud(Client& client, LinkBaudInfo* info, std::wstring* error) {
    Response reply;
    if (!client.RequestWithPolicy(Command::GetLinkBaud,{},800U,1U,&reply,error)) return false;
    if (reply.payload.size()!=HL_BAUD_INFO_SIZE || reply.payload[0]!=1U ||
        reply.payload[1]!=1U || reply.payload[2] || reply.payload[3])
        return Fail(error,L"Invalid UART Host Link baud descriptor.");
    const auto* p=reply.payload.data();
    info->bootstrap=image_policy::Read32(p+4); info->current=image_policy::Read32(p+8);
    info->supported_mask=image_policy::Read32(p+12); info->actual=image_policy::Read32(p+16);
    const auto bit=info->current==115200U?1U:info->current==460800U?2U:info->current==921600U?4U:0U;
    if (info->bootstrap!=HL_BAUD_DEFAULT || !bit || !(info->supported_mask&bit) ||
        (info->supported_mask&~HL_BAUD_MASK) || !info->actual)
        return Fail(error,L"Unsupported Host Link baud descriptor values.");
    return true;
}
bool SwitchLinkBaud(Client& client, std::uint32_t desired,
                    const std::function<bool(std::uint32_t,std::wstring*)>& set_pc,
                    LinkBaudInfo* info, std::wstring* error) {
    const auto bit=desired==115200U?1U:desired==460800U?2U:desired==921600U?4U:0U;
    if (!bit || !set_pc) return Fail(error,L"Unsupported PC-to-bridge baud rate.");
    if (!ReadLinkBaud(client,info,error)) return false;
    if (!(info->supported_mask&bit)) return Fail(error,L"Bridge does not advertise this Host Link rate.");
    if (info->current==desired) return true;
    const auto request=Rate(desired);
    Response reply;
    // Never retry SET at the old speed: the peer may already have switched.
    if (!client.RequestWithPolicy(Command::SetLinkBaud,request,800U,1U,&reply,error)) return false;
    if (reply.payload!=request) return Fail(error,L"Host Link SET baud echo mismatch.");
    if (!set_pc(desired,error)) return false;
    client.BeginSession();
    if (!client.RequestWithPolicy(Command::ConfirmLinkBaud,request,800U,1U,&reply,error) ||
        reply.payload!=request) return Fail(error,L"Host Link new-speed confirmation failed; recovery required.");
    if (!ReadLinkBaud(client,info,error) || info->current!=desired)
        return Fail(error,L"Host Link baud verification failed.");
    return true;
}
}
