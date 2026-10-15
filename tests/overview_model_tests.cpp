#include "../src/ui/overview_model.h"
#include <iostream>
#include <cstring>
int TestOverviewModel() {
    using namespace firmware_upgrade::overview;
    int failures = 0;
    const auto check = [&failures](bool value, const char* name) {
        if (!value) { std::cerr << "FAIL overview: " << name << '\n'; ++failures; }
    };
    Input in;
    auto cards = Build(in);
    for (const auto& card : cards) check(card.signal == Signal::Unavailable, "initial N/A");
    in.host_current = true; in.host.board_id = 0x0101U; in.port = L"COM6";
    in.host.firmware_build = 44U;
    cards = Build(in);
    check(cards[0].value == L"RL78/F24 | COM6" && cards[0].detail.find(L"44") != std::wstring::npos,
          "bridge identity from live host info");
    in.host.board_id = 0x7777U; cards = Build(in);
    check(cards[0].value.find(L"RL78/F24") == std::wstring::npos, "unknown host is not guessed");
    in.target_current = true; in.kind = TargetKind::Transfer;
    in.transfer.part_number = "R5F10BMG"; in.transfer.target_id = 0xF13BU;
    cards = Build(in);
    check(cards[1].value == L"R5F10BMG" && cards[2].signal == Signal::Unavailable &&
          cards[3].signal == Signal::Unavailable, "benchmark never invents boot/app versions");
    in.kind = TargetKind::CanUpgrade; in.target.mcu_family = L"RL78/F13";
    in.target.mcu_part = L"R5F10BMG"; in.target.boot_version = 0x010203U;
    in.target.app_version = 0x010101U; in.target.installed_app_size = 19160U;
    in.target.state = firmware_upgrade::f13_boot::DeviceState::Bootloader;
    cards = Build(in);
    check(cards[1].heading.find(L"BOOTLOADER") != std::wstring::npos && cards[2].value == L"v1.2.3" &&
          cards[3].detail.find(L"19160") != std::wstring::npos, "independent live boot/app");
    in.target.app_version = in.target.installed_app_size = 0xFFFFFFFFU;
    cards = Build(in);
    check(cards[3].signal == Signal::Reported && cards[3].value == L"N/A - not installed", "empty app sentinel");
    in.target_current = false; cards = Build(in);
    check(cards[1].signal == Signal::Unavailable && cards[2].value == L"N/A", "changed selection invalidates live info");
    in.target_current = true; in.host_current = false; cards = Build(in);
    check(cards[1].signal == Signal::Unavailable && cards[3].value == L"N/A", "disconnect hides stale target snapshot");
    in.bin_size = 32768U; cards = Build(in);
    check(cards[4].signal == Signal::Invalid && cards[4].value == L"32768 bytes", "raw corrupt BIN visible offline");
    in.image.metadata_valid = in.image.checksum_available = in.image.checksum_matches = true;
    in.image.metadata.version = 0x010200U;
    cards = Build(in);
    check(cards[4].signal == Signal::Offline && cards[4].value.find(L"v1.2.0") == 0U &&
          cards[2].value == L"N/A", "loaded BIN never replaces live metadata");
    in.i2c = true; in.host_current = in.target_current = true; in.kind = TargetKind::G16Reference;
    in.g16.label = L"RL78_G16_FPB"; in.g16.boot_major = 1U;
    in.i2c_image.available = in.i2c_image.matches = true;
    cards = Build(in);
    check(cards[1].signal == Signal::Reported && cards[2].signal == Signal::Reported &&
          cards[3].signal == Signal::Reported && cards[4].signal == Signal::Offline,
          "G16 reported constants are not verified installed firmware");
    in.misc=true; in.bridge_current=true; in.bridge.mode=BU_MODE_BOOT;
    in.bridge.boot_version=0x10000; in.bridge.app_version=0x10100; in.bridge.flags=BU_FLAG_VALID_APP;
    in.bridge.installed_size=37640; std::memcpy(in.bridge.part,"R7F124FLJ3xFB",14);
    std::memcpy(in.bridge.family,"RL78/F24",9);
    cards=Build(in);
    check(cards[1].heading.find(L"HOST BRIDGE")!=std::wstring::npos && cards[3].value==L"v1.1.0" &&
        cards[0].value.find(L"R7F124FLJ3xFB")!=std::wstring::npos,"Misc displays HOST not stale G16/F13 metadata");
    in.misc=false; cards=Build(in);
    check(cards[0].value.find(L"R7F124FLJ3xFB")!=std::wstring::npos,"normal transport uses live optional bridge part descriptor");
    in.misc=true; in.host_current=false; cards=Build(in);
    check(cards[2].value==L"N/A" && cards[3].value==L"N/A","Misc disconnect hides stale bridge firmware");
    in.image.metadata_available=true;
    in.image.metadata.policy.role=IP_ROLE_DEVICE_APP; in.image.metadata.policy.transports=IP_CAN;
    in.image.metadata.policy.irq_abi=IP_F13_IRQ_ABI;
    const auto loaded_version=Build(in)[4].value;
    check(Build(in)[4].signal==Signal::Invalid,"CAN device BIN cannot update host via Misc");
    in.misc=false; in.i2c=true; cards=Build(in);
    check(cards[4].value==loaded_version && cards[4].heading.find(L"WRONG ROUTE")!=std::wstring::npos &&
        cards[4].detail.find(L"allowed CAN")!=std::wstring::npos,"I2C preserves CAN BIN identity and shows policy mismatch");
    in.i2c=false; cards=Build(in);
    check(cards[4].value==loaded_version && cards[4].signal==Signal::Offline,"switch back CAN keeps original BIN");
    if (failures == 0) std::cout << "Provenance-aware overview state tests passed\n";
    return failures;
}
