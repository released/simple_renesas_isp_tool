#pragma once
#include "firmware_update_profile.h"
#include "f13_boot_profile.h"
#include "bridge_update_profile.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace firmware_upgrade::evidence {
struct Difference {
    std::wstring field, image, live, result;
};
inline std::wstring Hex(std::uint32_t n) {
    std::wostringstream s; s << L"0x" << std::hex << std::uppercase
        << std::setfill(L'0') << std::setw(8) << n; return s.str();
}
inline void Add(std::vector<Difference>& rows, const wchar_t* field,
                std::uint32_t image, std::uint32_t live, bool informational = false) {
    rows.push_back({field,Hex(image),live == 0xFFFFFFFFU ? L"not installed" : Hex(live),
        image == live ? L"MATCH" : informational ? L"CHANGE" : L"MISMATCH"});
}
inline std::vector<Difference> Compare(const firmware_update::ImageInfo& i,
                                      const f13_boot::TargetInfo& t) {
    std::vector<Difference> r;
    Add(r,L"Target",i.target_id,t.target_id);
    r.push_back({L"MCU",i.mcu_family+L" / "+i.mcu_part,t.mcu_family+L" / "+t.mcu_part,
        i.mcu_family==t.mcu_family && i.mcu_part==t.mcu_part ? L"MATCH" : L"MISMATCH"});
    Add(r,L"App version",i.version,t.app_version,true); Add(r,L"Build",i.build,t.app_build,true);
    Add(r,L"Version control",i.policy.version_control,t.policy.version_control,true);
    Add(r,L"Image bytes",i.image_size,t.installed_app_size,true);
    Add(r,L"App start",i.application_start,t.application_start);
    Add(r,L"Capacity",i.application_capacity,t.application_capacity);
    Add(r,L"Flash bytes",i.code_flash_size,t.code_flash_size);
    Add(r,L"Boot reserve",i.boot_reserved_size,t.boot_reserved_size);
    Add(r,L"Erase bytes",i.erase_block_size,t.erase_block_size);
    Add(r,L"Minimum image",i.minimum_image_size,t.minimum_image_size);
    Add(r,L"Protocol / format",(i.protocol_id<<16U)|(i.protocol_version<<8U)|i.format,
        (t.protocol_id<<16U)|(t.protocol_version<<8U)|t.image_format);
    Add(r,L"Checksum",i.checksum_type,t.checksum_type);
    Add(r,L"CAN mode / ID type",(i.can_mode_mask<<8U)|i.can_id_format,(t.can_mode_mask<<8U)|t.can_id_format);
    Add(r,L"Nominal mask",i.nominal_bitrate_mask,t.nominal_bitrate_mask);
    Add(r,L"Data mask",i.data_bitrate_mask,t.data_bitrate_mask);
    Add(r,L"Request ID",i.request_id,t.request_id); Add(r,L"Response ID",i.response_id,t.response_id);
    Add(r,L"Image role",i.policy.role,t.policy.role);
    r.push_back({L"Allowed CAN",Hex(i.policy.transports),Hex(t.policy.transports),
        (i.policy.transports&IP_CAN) && (t.policy.transports&IP_CAN) ? L"MATCH" : L"MISMATCH"});
    Add(r,L"IRQ ABI",i.policy.irq_abi,t.policy.irq_abi);
    r.push_back({L"Minimum Boot",Hex(i.policy.minimum_boot_version),Hex(t.boot_version),
        t.boot_version>=i.policy.minimum_boot_version ? L"MATCH" : L"MISMATCH"});
    return r;
}
inline bool ConfirmApp(const firmware_update::ImageInfo& image,
                       const f13_boot::TargetInfo& live, std::uint32_t health) {
    const auto rows=Compare(image,live);
    return live.state==f13_boot::DeviceState::Application && (health&0x07FFU)==0x0701U &&
        image.version==live.app_version && image.build==live.app_build &&
        image.image_size==live.installed_app_size &&
        image.policy.version_control==live.policy.version_control && (live.version_policy&1U) &&
        std::none_of(rows.begin(),rows.end(),[](const Difference& d){return d.result==L"MISMATCH";});
}
inline std::vector<Difference> Compare(const bu_manifest_t& i,const bu_info_t& t) {
    std::vector<Difference> r;
    Add(r,L"Board",i.board_id,t.board_id); Add(r,L"App start",i.app_start,t.app_start);
    Add(r,L"Capacity",i.capacity,t.capacity);
    Add(r,L"Flash bytes",i.flash_size,t.flash_size); Add(r,L"Boot reserve",i.boot_size,t.boot_size);
    Add(r,L"Erase bytes",i.erase_size,t.erase_size); Add(r,L"Minimum bytes",i.minimum_size,t.minimum_size);
    Add(r,L"IRQ ABI",i.irq_abi,t.irq_abi); Add(r,L"Update UART baud",i.uart_baud,t.uart_baud);
    Add(r,L"App version",i.version,t.app_version,true);
    Add(r,L"Build",i.build,t.app_build,true); Add(r,L"Image bytes",i.image_size,t.installed_size,true);
    r.push_back({L"Minimum Boot",Hex(i.minimum_boot_version),Hex(t.boot_version),
        t.boot_version>=i.minimum_boot_version ? L"MATCH" : L"MISMATCH"});
    return r;
}
inline bool ConfirmBridgeApp(const bu_info_t& expected,const bu_info_t& live) {
    return live.mode==BU_MODE_APP && (live.flags&BU_FLAG_VALID_APP) &&
        expected.board_id==live.board_id && expected.app_version==live.app_version &&
        expected.app_build==live.app_build && expected.installed_size==live.installed_size &&
        expected.app_start==live.app_start && expected.capacity==live.capacity &&
        expected.flash_size==live.flash_size && expected.boot_size==live.boot_size &&
        expected.erase_size==live.erase_size && expected.minimum_size==live.minimum_size &&
        expected.uart_baud==live.uart_baud && expected.irq_abi==live.irq_abi &&
        std::memcmp(expected.part,live.part,sizeof(live.part))==0 &&
        std::memcmp(expected.family,live.family,sizeof(live.family))==0;
}
// Saturating host counters cannot provide an exact delta after reset or saturation.
inline std::wstring Delta(std::uint32_t before,std::uint32_t after) {
    if(before==0xFFFFFFFFU || after==0xFFFFFFFFU) return L"unavailable (saturated)";
    if(after<before) return L"unavailable (reset)";
    return std::to_wstring(after-before);
}
inline std::wstring JsonString(const std::wstring& text) {
    std::wstring out=L"\"";
    for(wchar_t c:text) {
        if(c==L'"' || c==L'\\') {out+=L'\\';out+=c;}
        else if(c==L'\n') out+=L"\\n";
        else if(c==L'\r') out+=L"\\r";
        else if(c==L'\t') out+=L"\\t";
        else if(c<32) out+=L"?";
        else out+=c;
    }
    return out+L"\"";
}
} // namespace firmware_upgrade::evidence
