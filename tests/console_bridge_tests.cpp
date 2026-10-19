#include "../src/ui/console_geometry.h"
#include "../src/core/bridge_update_profile.h"
#include "../src/core/update_evidence.h"
#include "../src/core/update_report_store.h"
#include "../src/ui/update_feedback.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <string>
#include <windows.h>

int TestConsoleBridge() {
    int failures=0;
    const auto check=[&](bool ok,const char* name) { if (!ok) { ++failures; std::cerr<<"FAIL: "<<name<<'\n'; } };
    namespace c=firmware_upgrade::console;
    namespace ev=firmware_upgrade::evidence;
    {
    firmware_upgrade::firmware_update::ImageInfo image;
    firmware_upgrade::f13_boot::TargetInfo live;
    image.version=live.app_version=0x10600U;image.build=live.app_build=20261017U;
    image.image_size=live.installed_app_size=19456U;
    image.policy.transports=live.policy.transports=IP_CAN;
    live.state=firmware_upgrade::f13_boot::DeviceState::Application;
    live.version_policy=1U;
    check(ev::ConfirmApp(image,live,0x00030701U),"matching APP_READY and live App identity confirmed");
    check(ev::ConfirmApp(image,live,0x00030F01U),"additional App self-test bits do not change the fixed health interface");
    check(!ev::ConfirmApp(image,live,0x700U),"INFO or init-only health is not APP_READY");
    ++live.app_build;check(!ev::ConfirmApp(image,live,0x701U),"wrong build rejects post-update confirmation");--live.app_build;
    ++live.app_version;check(!ev::ConfirmApp(image,live,0x701U),"wrong version rejects confirmation");--live.app_version;
    ++live.installed_app_size;check(!ev::ConfirmApp(image,live,0x701U),"wrong installed size rejects confirmation");--live.installed_app_size;
    live.state=firmware_upgrade::f13_boot::DeviceState::Bootloader;
    check(!ev::ConfirmApp(image,live,0x701U),"Boot response cannot confirm App");
    live.state=firmware_upgrade::f13_boot::DeviceState::Application;
    ++live.policy.irq_abi;check(!ev::ConfirmApp(image,live,0x701U),"ABI mismatch rejects confirmation");
    check(ev::Delta(10U,13U)==L"3" && ev::Delta(10U,0U).find(L"reset")!=std::wstring::npos &&
        ev::Delta(10U,0xFFFFFFFFU).find(L"saturated")!=std::wstring::npos,"counter deltas never silently wrap");
    check(ev::JsonString(L"a\"\\\r\n\tb")==L"\"a\\\"\\\\\\r\\n\\tb\"","automatic JSON report escaping");
    wchar_t folder[MAX_PATH]={};GetTempPathW(MAX_PATH,folder);
    const std::wstring report_path=std::wstring(folder)+L"firmware_report_test_"+std::to_wstring(GetCurrentProcessId())+
        L"_"+std::to_wstring(GetTickCount64())+L".json";
    std::wstring report_error;
    check(ev::WriteReport(report_path,L"{\"result\":\"PASS\",\"text\":\"\u6e2c\u8a66\"}",&report_error),"production UTF-8 automatic report store");
    HANDLE stored=CreateFileW(report_path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    char recorded[128]={};DWORD recorded_size=0;
    const bool readable=stored!=INVALID_HANDLE_VALUE && ReadFile(stored,recorded,sizeof(recorded),&recorded_size,nullptr);
    if(stored!=INVALID_HANDLE_VALUE) CloseHandle(stored);
    check(readable && std::string(recorded,recorded_size)==u8"{\"result\":\"PASS\",\"text\":\"\u6e2c\u8a66\"}","automatic report contains exact UTF-8 JSON bytes");
    check(!ev::WriteReport(report_path,L"overwrite",&report_error),"automatic report never overwrites an existing file");
    DeleteFileW(report_path.c_str());
    bu_info_t boot={},app={}; app.mode=BU_MODE_APP;app.flags=BU_FLAG_VALID_APP;
    boot.board_id=app.board_id=0x101U;boot.app_version=app.app_version=0x10600U;
    check(ev::ConfirmBridgeApp(boot,app),"Misc RUN confirms same live App descriptor after complete metadata inspection");
    ++app.board_id;check(!ev::ConfirmBridgeApp(boot,app),"Misc RUN rejects a different host");--app.board_id;
    ++app.app_version;check(!ev::ConfirmBridgeApp(boot,app),"Misc RUN rejects a different app version");--app.app_version;
    ++app.app_start;check(!ev::ConfirmBridgeApp(boot,app),"Misc RUN rejects a changed live geometry");--app.app_start;
    app.mode=BU_MODE_BOOT;check(!ev::ConfirmBridgeApp(boot,app),"Misc RUN returning to Boot is not success");
    }
    for (const auto size:{std::pair<int,int>{1350,690},{1244,661},{1904,990}}) {
        const auto r=c::Calculate(size.first,size.second,16);
        check(r.fits,"console body fits at 1366 desktop / minimum / large");
        check(r.image_pane.x>=r.interface_pane.x+r.interface_pane.width,"interface and image do not overlap");
        check(r.log_pane.x>=r.image_pane.x+r.image_pane.width,"image and log do not overlap");
        check(r.tabs.x==r.interface_pane.x && r.tabs.width==r.interface_pane.width,
            "TAB strip is confined to interface column");
        check(r.tabs.x+r.tabs.width<r.image_pane.x && r.tabs.x+r.tabs.width<r.log_pane.x,
            "shared image/log are outside TAB bounds");
        check(r.image_heading.y==r.interface_heading.y && r.log_heading.y==r.interface_heading.y,
            "three independent headings share a row");
        check(r.image_pane.y==r.log_pane.y && r.image_pane.y<r.interface_pane.y,
            "shared panes do not inherit settings TAB content offset");
        check(r.interface_pane.y>=r.tabs.y+r.tabs.height,
            "interface settings start below TAB strip");
        check(r.footer.y+r.footer.height<=size.second,"footer inside client");
    }
    check(c::Calculate(1350,690,18).fits,"1366 desktop with larger125-percent text metrics");
    const auto inside=[](const c::Rect& child,const c::Rect& parent) {
        return child.x>=parent.x && child.y>=parent.y && child.width>=0 && child.height>=0 &&
            child.x+child.width<=parent.x+parent.width && child.y+child.height<=parent.y+parent.height;
    };
    for(const auto size:{std::pair<int,int>{1260,660},{1350,690},{1904,1000}}) {
        for(const int text_height:{14,18,21}) {
            const auto layout=c::Calculate(size.first,(std::max)(size.second,c::MinimumClientHeight(text_height)),text_height);
            const auto log=c::CalculateLog(layout.log_pane,layout.field_row_height);
            check(layout.fits,"font-aware minimum client size contains the console");
            check(inside(layout.connection,layout.connection_group),"connection controls stay within their frame");
            check(inside(layout.identities,layout.analysis_group) && inside(layout.firmware,layout.analysis_group),"live/offline cards stay in analysis frame");
            for (int group=0;group<4;++group) {
                check(inside(layout.metadata_groups[group],layout.analysis_group),"metadata group inside analysis frame");
                check(inside(layout.metadata_panes[group],layout.metadata_groups[group]),"metadata fields inside their semantic group");
                if(group) check(layout.metadata_groups[group-1].x+layout.metadata_groups[group-1].width<layout.metadata_groups[group].x,"metadata groups do not overlap");
            }
            check(inside(layout.interface_pane,layout.interface_group) && inside(layout.tabs,layout.interface_group) &&
                inside(layout.image_pane,layout.image_group) && inside(layout.log_pane,layout.log_group),"three lower panes stay within independent frames");
            for(const auto& b:log.buttons) check(inside(b,layout.log_pane),"report/error log buttons remain within pane at100/125/150-percent metrics");
            check(inside(log.trace,layout.log_pane) && inside(log.auto_scroll,layout.log_pane) && inside(log.history,layout.log_pane),"trace, auto-scroll and log remain in pane");
            check(log.history.height>=80,"shared log retains readable history at minimum client");
            check(log.buttons[4].y+log.buttons[4].height<=log.trace.y &&
                log.trace.y+log.trace.height<=log.auto_scroll.y &&
                log.auto_scroll.y+log.auto_scroll.height<log.history.y,"log tools do not overlap trace/auto-scroll/history");
        }
    }
    // Use real Segoe UI/GDI metrics, including the reported long Misc caption.
    HDC dc=CreateCompatibleDC(nullptr);
    check(dc!=nullptr,"footer font measurement DC created");
    for (const int dpi:{96,120,144}) {
        HFONT font=CreateFontW(-MulDiv(9,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
            DEFAULT_PITCH,L"Segoe UI");
        check(font!=nullptr,"footer font created");
        if (!dc || !font) { if (font) DeleteObject(font); continue; }
        HGDIOBJ old=SelectObject(dc,font);
        TEXTMETRICW metric={}; GetTextMetricsW(dc,&metric);
        const auto measure=[&](const std::wstring& text,int minimum) {
            SIZE extent={}; GetTextExtentPoint32W(dc,text.c_str(),static_cast<int>(text.size()),&extent);
            return (std::max)(minimum,static_cast<int>(extent.cx)+12);
        };
        RECT old_caption={0,0,145,0};
        DrawTextW(dc,L"Host bridge app update: 40%",-1,&old_caption,DT_WORDBREAK|DT_CALCRECT|DT_NOPREFIX);
        check(old_caption.bottom>(std::max)(24,static_cast<int>(metric.tmHeight)+8),
            "old145px footer reproduces the clipped second-line defect");
        std::cout<<"Footer GDI "<<dpi<<"dpi: old caption height="<<old_caption.bottom
            <<", single-line100% width="<<measure(L"Host bridge app update: 100%",145)<<'\n';
        for (const auto size:{std::pair<int,int>{1350,690},{1244,621},{1904,990}}) {
            const auto body=c::Calculate(size.first,size.second,metric.tmHeight);
            for (const wchar_t* label:{L"Progress",L"Host bridge UART",L"Host bridge app update",
                     L"Bridge board self test",L"Target discovery",L"Enter bootloader",
                     L"Transfer test",L"Firmware update",L"Device update complete",
                     L"Device App confirmed",L"Host update complete",L"Host App confirmed"}) {
                const int caption=measure(std::wstring(label)+L": 100%",145);
                const auto footer=c::CalculateFooter(body.footer,caption,
                    measure(L"Elapsed: 00:00.0",145),measure(L"Build 0.1.0.0078  2026-10-15 23:59:59",270));
                check(footer.fits,"all operation captions fit at minimum/1366/large and100/125/150-percent fonts");
                check(inside(footer.progress_label,body.footer) && inside(footer.progress_bar,body.footer) &&
                    inside(footer.elapsed,body.footer) && inside(footer.build,body.footer),"all footer children inside client/footer");
                check(footer.progress_label.x+footer.progress_label.width<footer.progress_bar.x &&
                    footer.progress_bar.x+footer.progress_bar.width<footer.elapsed.x &&
                    footer.elapsed.x+footer.elapsed.width<footer.build.x,"footer children do not overlap");
                for (const int percent:{0,9,40,99,100}) {
                    check(measure(std::wstring(label)+L": "+std::to_wstring(percent)+L"%",0)<=footer.progress_label.width,
                        "entire progress caption and percentage fit reserved100-percent width");
                }
            }
        }
        for (const auto result : {firmware_upgrade::feedback::Result::DeviceAppConfirmed,
                firmware_upgrade::feedback::Result::DeviceAppConfirmedCanActive,
                firmware_upgrade::feedback::Result::HostAppWritten,firmware_upgrade::feedback::Result::HostAppRunning}) {
            const auto view=firmware_upgrade::feedback::Describe(result);
            const auto newline=view.compact.find(L"\r\n");
            check(newline!=std::wstring::npos && measure(view.compact.substr(0,newline),0)<=600 &&
                measure(view.compact.substr(newline+2),0)<=600,"two-line completion hint fits narrow image pane at96/120/144dpi");
            check(measure(view.status,0)<=1220,"completion status fits minimum-width banner at96/120/144dpi");
        }
        SelectObject(dc,old); DeleteObject(font);
    }
    if (dc) DeleteDC(dc);
    for (const int width:{0,12,120,640}) {
        const c::Rect bounds={6,100,width,24};
        const auto footer=c::CalculateFooter(bounds,300,145,400);
        check(!footer.fits,"unsupported narrow footer reported");
        check(inside(footer.progress_label,bounds) && inside(footer.progress_bar,bounds) &&
            inside(footer.elapsed,bounds) && inside(footer.build,bounds),"narrow footer stays bounded without overlap");
    }
    namespace b=firmware_upgrade::bridge_update;
    std::vector<std::uint8_t> bytes(1024,0xFF);
    auto* p=bytes.data()+BU_MANIFEST_OFFSET;
    std::memset(p,0,BU_MANIFEST_SIZE); Bu_Write32(p,BU_IMAGE_MAGIC);
    p[4]=BU_PROTOCOL; p[5]=BU_MANIFEST_SIZE; p[6]=2; p[7]=1;
    Bu_Write16(p+8,0x0101); Bu_Write16(p+10,2); Bu_Write32(p+12,1024);
    Bu_Write32(p+16,0x010000); Bu_Write32(p+20,20261015);
    Bu_Write32(p+24,0x5000); Bu_Write32(p+28,0x3FC00-0x5000);
    Bu_Write32(p+32,0x40000); Bu_Write32(p+36,0x5000);
    Bu_Write32(p+40,1024); Bu_Write32(p+44,516); Bu_Write32(p+48,115200);
    std::memcpy(p+52,"RL78/F24",8); std::memcpy(p+64,"R7F124FLJ3xFB",13);
    Bu_Write32(p+80,0x010000);
    Bu_Write32(p+96,IP_MAGIC); p[100]=IP_VERSION; p[101]=IP_SIZE; p[102]=IP_ROLE_BRIDGE_APP; p[103]=IP_UART;
    Bu_Write32(p+104,IP_F24_IRQ_ABI); Bu_Write32(p+108,0x010000);
    const auto checksum=[&]() { Bu_Write32(bytes.data()+bytes.size()-4,
        firmware_upgrade::firmware_update::Crc32(bytes.data(),bytes.size()-4)); };
    checksum(); auto image=b::Inspect(bytes); check(image.valid,"BUR1 valid dynamic image");
    bu_info_t device={}; device.mode=BU_MODE_BOOT; device.max_data=64; device.board_id=0x0101;
    device.boot_version=0x010000; device.app_start=0x5000; device.capacity=0x3FC00-0x5000;
    device.flash_size=0x40000; device.boot_size=0x5000; device.erase_size=1024;
    device.minimum_size=516; device.uart_baud=115200; device.irq_abi=IP_F24_IRQ_ABI;
    std::memcpy(device.family,"RL78/F24",9); std::memcpy(device.part,"R7F124FLJ3xFB",14);
    check(Bu_MatchesDevice(&image.metadata,&device)!=0,"exact bridge descriptor match");
    device.board_id=0x0202; check(!Bu_MatchesDevice(&image.metadata,&device),"wrong host rejected"); device.board_id=0x0101;
    std::uint8_t wire[96]; bu_info_t decoded={}; Bu_EncodeInfo(&device,wire);
    check(Bu_ParseInfo(wire,BU_INFO_SIZE,&decoded)!=0,"boot descriptor round trip");
    wire[6]=63; check(!Bu_ParseInfo(wire,BU_INFO_SIZE,&decoded),"unaligned data limit rejected");
    p[128]=0; checksum(); check(!b::Inspect(bytes).valid,"reserved bridge extension rejected with correct CRC"); p[128]=0xFF;
    p[4]=1; checksum(); check(!b::Inspect(bytes).valid,"bridge contract1 rejected"); p[4]=BU_PROTOCOL;
    p[64]='X'; checksum(); image=b::Inspect(bytes);
    check(image.valid && !Bu_MatchesDevice(&image.metadata,&device),"wrong part blocked even with correct CRC");
    p[6]=1; checksum(); check(!b::Inspect(bytes).valid,"external device role cannot update bridge");
    p[6]=2; checksum(); bytes[10]^=1; check(!b::Inspect(bytes).valid,"CRC mismatch rejected");
    std::cout<<"Console geometry / bridge contract tests: "<<(failures ? "FAIL" : "PASS")<<'\n';
    return failures;
}
