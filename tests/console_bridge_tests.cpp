#include "../src/ui/console_geometry.h"
#include "../src/core/bridge_update_profile.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <string>
#include <windows.h>

int TestConsoleBridge() {
    int failures=0;
    const auto check=[&](bool ok,const char* name) { if (!ok) { ++failures; std::cerr<<"FAIL: "<<name<<'\n'; } };
    namespace c=firmware_upgrade::console;
    for (const auto size:{std::pair<int,int>{1350,690},{1260,625},{1904,990}}) {
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
                     L"Transfer test",L"Firmware update"}) {
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
    check(Bu_ParseInfo(wire,&decoded)!=0,"boot descriptor round trip");
    wire[6]=63; check(!Bu_ParseInfo(wire,&decoded),"unaligned data limit rejected");
    p[128]=0; checksum(); check(!b::Inspect(bytes).valid,"reserved bridge extension rejected with correct CRC"); p[128]=0xFF;
    p[4]=1; checksum(); check(!b::Inspect(bytes).valid,"bridge contract1 rejected"); p[4]=BU_PROTOCOL;
    p[64]='X'; checksum(); image=b::Inspect(bytes);
    check(image.valid && !Bu_MatchesDevice(&image.metadata,&device),"wrong part blocked even with correct CRC");
    p[6]=1; checksum(); check(!b::Inspect(bytes).valid,"external device role cannot update bridge");
    p[6]=2; checksum(); bytes[10]^=1; check(!b::Inspect(bytes).valid,"CRC mismatch rejected");
    std::cout<<"Console geometry / bridge contract tests: "<<(failures ? "FAIL" : "PASS")<<'\n';
    return failures;
}
