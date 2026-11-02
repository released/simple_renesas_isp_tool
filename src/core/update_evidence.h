#pragma once
#include "firmware_update_profile.h"
#include "f13_boot_profile.h"
#include "bridge_update_profile.h"
#include "can_image_contract.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <type_traits>

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
    const auto text = [](const auto& value, bool live) -> std::wstring {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, std::wstring>) return value;
        else return live && value == 0xFFFFFFFFU ? L"not installed" : Hex(value);
    };
    can_contract::Visit(i, t, [&](const wchar_t* field, const auto& a, const auto& b, bool passed, bool info) {
        r.push_back({field, text(a, false), text(b, true), passed ? L"MATCH" : info ? L"CHANGE" : L"MISMATCH"});
    });
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
