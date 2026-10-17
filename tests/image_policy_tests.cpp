#include "firmware_update_profile.h"
#include "bridge_update_profile.h"
#include "device_version_policy.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

/* Execute the exact C90 metadata/CRC routines linked into the F13 boot. */
#define __far
#define F13_CRC_HOST_TEST
#include "can_boot_transport.h"
#include "sw_crc32.c"
#include "can_boot_metadata.inc"
#include "can_boot_verify.inc"
#undef __far

int TestImagePolicy() {
    static_assert(sizeof(unsigned long)==4,"Real RL78 validation requires32-bit words");
    using namespace firmware_upgrade;
    int failures=0;
    const auto check=[&](bool ok,const char* name) {
        if (!ok) { ++failures; std::cerr<<"FAIL policy: "<<name<<'\n'; }
    };
    check(F13_Crc32Bytes(reinterpret_cast<const unsigned char*>("123456789"),9)==0xCBF43926UL,"real F13 CRC known vector");
    std::vector<std::uint8_t> image(3072,0xA6);
    std::fill_n(image.data()+256,256,static_cast<std::uint8_t>(0xFF));
    std::memcpy(image.data()+256,g_manifest_contract,128);
    auto* region=image.data()+256;
    Bu_Write32(region+16,0x10400); Bu_Write32(region+20,20261015); Bu_Write32(region+24,3072);
    Bu_Write32(region+108,0x10400);
    Bu_Write32(image.data()+image.size()-4,firmware_update::Crc32(image.data(),image.size()-4));
    const auto region_crc=firmware_update::Crc32(region,256);
    image_policy::Policy p;
    check(image_policy::Parse(region,&p),"common policy parse");
    std::wstring error;
    auto uart_metadata=std::vector<std::uint8_t>(region,region+256);
    uart_metadata[100]=IP_DEVICE_UART_VERSION; uart_metadata[103]=IP_UART;
    Bu_Write32(uart_metadata.data()+112,921600U);
    image_policy::Policy uart_policy;
    check(image_policy::Parse(uart_metadata.data(),&uart_policy) && uart_policy.device_uart_baud==921600U,
        "future device UART profile declares compile-time baud");
    check(image_policy::AllowsDeviceUartBaud(uart_policy,921600U,&error) &&
        !image_policy::AllowsDeviceUartBaud(uart_policy,115200U,&error),"device UART baud gate independent of Host Link");
    Bu_Write32(uart_metadata.data()+112,0U);
    check(!image_policy::Parse(uart_metadata.data(),&uart_policy),"UART profile cannot omit fixed baud");
    uart_metadata[100]=IP_VERSION;
    check(!image_policy::Parse(uart_metadata.data(),&uart_policy),"legacy CAN policy is not a UART device contract");
    for (std::uint8_t mask=1;mask<=7;++mask) {
        p.transports=mask;
        for (const auto route:{IP_CAN,IP_I2C,IP_UART}) {
            check(image_policy::Allows(p,IP_ROLE_DEVICE_APP,static_cast<std::uint8_t>(route),&error)==((mask&route)!=0),"transport allow/deny matrix");
            check(!image_policy::Allows(p,IP_ROLE_BRIDGE_APP,static_cast<std::uint8_t>(route),&error),"device image never targets host");
        }
    }
    firmware_update::ImageInfo info;
    check(firmware_update::ValidateImage(firmware_update::Profile::CanUpgrade,image,&info,&error),"shared parser accepts current F13 contract");
    unsigned long verified_size=0, verified_crc=0;
    const auto before_verify=image;
    check(VerifyImage(reinterpret_cast<const unsigned long*>(image.data()),&verified_size,&verified_crc)==CAN_BOOT_STATUS_OK &&
        verified_size==image.size() && verified_crc==info.stored_crc32 && image==before_verify,
        "production read-only VERIFY range/CRC and no image mutation");
    auto corrupt=image;corrupt[520]^=1U;
    check(VerifyImage(reinterpret_cast<const unsigned long*>(corrupt.data()),&verified_size,&verified_crc)==CAN_BOOT_STATUS_CRC_ERROR,
        "production VERIFY detects flash corruption");
    for(const auto mismatch:{std::pair<unsigned,unsigned>{28U,CAN_BOOT_STATUS_LAYOUT},
        {100U,CAN_BOOT_STATUS_POLICY},{104U,CAN_BOOT_STATUS_IRQ_ABI},{128U,CAN_BOOT_STATUS_METADATA}}) {
        corrupt=image;corrupt[256U+mismatch.first]^=0x80U;
        check(VerifyImage(reinterpret_cast<const unsigned long*>(corrupt.data()),&verified_size,&verified_crc)==mismatch.second,
            "production diagnostic categories before CRC / Flash");
    }
    for (const auto route:{IP_I2C,IP_UART}) {
        auto wrong=image; wrong[256+103]=static_cast<std::uint8_t>(route);
        if (route==IP_UART) { wrong[256+100]=IP_DEVICE_UART_VERSION; Bu_Write32(wrong.data()+256+112,460800U); }
        Bu_Write32(wrong.data()+wrong.size()-4,firmware_update::Crc32(wrong.data(),wrong.size()-4));
        firmware_update::ImageInfo rejected;
        if (route==IP_UART) {
            const auto offline=firmware_update::InspectImage(wrong);
            check(offline.metadata_valid && offline.metadata.policy.device_uart_baud==460800U &&
                image_policy::DeviceUartText(offline.metadata.policy)==L" @460800",
                "offline UI retains fixed UART metadata while CAN programming is blocked");
        }
        check(!firmware_update::ValidateImage(firmware_update::Profile::CanUpgrade,wrong,&rejected,&error) &&
            error.find(L"policy mismatch")!=std::wstring::npos,"correct CRC cannot bypass wrong update transport");
    }
    auto wrong_role=image; wrong_role[256+102]=IP_ROLE_BRIDGE_APP;
    Bu_Write32(wrong_role.data()+wrong_role.size()-4,firmware_update::Crc32(wrong_role.data(),wrong_role.size()-4));
    firmware_update::ImageInfo rejected_role;
    check(!firmware_update::ValidateImage(firmware_update::Profile::CanUpgrade,wrong_role,&rejected_role,&error),"mixed CAN format / host image role rejected");
    for (const unsigned offset:{0U,4U,8U,12U,28U,32U,36U,40U,44U,48U,56U,64U,80U,92U,96U,100U,101U,102U,103U,104U,112U,128U,252U}) {
        auto wrong=image; wrong[256+offset]^=0x80U;
        check(!MetadataValid(reinterpret_cast<const unsigned long*>(wrong.data()+256)),"real F13 rejects incompatible metadata");
    }
    MetadataReset();
    check(MetadataSeal(region_crc)!=CAN_BOOT_STATUS_OK,"incomplete staging blocks seal");
    check(MetadataStage(4,region+4)==CAN_BOOT_STATUS_BAD_SEQUENCE,"out of order metadata rejected");
    for (unsigned short at=0;at<256;at+=4) {
        check(MetadataStage(at,region+at)==CAN_BOOT_STATUS_OK,"stage contiguous metadata");
        check(MetadataStage(at,region+at)==CAN_BOOT_STATUS_OK && g_metadata_staged==at+4,"identical retry has no side effect");
    }
    std::array<std::uint8_t,4> conflict={0,0,0,0};
    check(MetadataStage(0,conflict.data())==CAN_BOOT_STATUS_BAD_IMAGE,"conflicting replay rejected");
    check(MetadataSeal(region_crc^1U)==CAN_BOOT_STATUS_CRC_ERROR && !g_metadata_sealed,"wrong metadata CRC blocks START");
    check(MetadataSeal(region_crc)==CAN_BOOT_STATUS_OK && g_metadata_sealed,"complete metadata sealed");
    check(MetadataSeal(region_crc)==CAN_BOOT_STATUS_OK,"seal retry idempotent");
    MetadataReset();
    check(MetadataSeal(region_crc)!=CAN_BOOT_STATUS_OK,"reset/disconnect cleanup cannot reuse stale staging");
    // Simulated power removal after every four-byte write, with exact boot checks.
    // This is a flash model, not physical FSL / power-loss evidence.
    std::vector<std::uint8_t> flash(image.size(),0xFF);
    unsigned cuts=0;
    for (unsigned at=0;at<image.size();at+=4) {
        std::copy_n(image.data()+at,4,flash.data()+at);
        const bool valid=Bu_Read32(flash.data())!=0xFFFFFFFFU &&
            MetadataValid(reinterpret_cast<const unsigned long*>(flash.data()+256)) &&
            F13_Crc32Bytes(flash.data(),static_cast<unsigned long>(flash.size()-4))==Bu_Read32(flash.data()+flash.size()-4);
        check(valid==(at+4==image.size()),"partial image never passes boot manifest/CRC");
        ++cuts;
    }
    check(F13_Crc32Bytes(image.data(),static_cast<unsigned long>(image.size()-4))==info.stored_crc32,"real F13 CRC equals PC image CRC");
    for (const unsigned size:{32768U,61440U,81920U,122880U}) {
        std::vector<std::uint8_t> large(size);
        for (unsigned i=0;i<size;++i) large[i]=static_cast<std::uint8_t>(i*37U+(i>>8U));
        check(F13_Crc32Bytes(large.data(),size)==firmware_update::Crc32(large.data(),size),"maximum-size real F13/PC CRC equivalence");
    }
    // Model replacement of an existing valid app, including erased block holes.
    flash=image;
    for (unsigned i=512;i<flash.size()-4;++i) flash[i]^=0x5A;
    Bu_Write32(flash.data()+flash.size()-4,firmware_update::Crc32(flash.data(),flash.size()-4));
    check(MetadataValid(reinterpret_cast<const unsigned long*>(flash.data()+256)),"old image valid before update");
    for (unsigned at=0;at<image.size();at+=4) {
        if (at%1024==0) std::fill_n(flash.data()+at,1024,static_cast<std::uint8_t>(0xFF));
        std::copy_n(image.data()+at,4,flash.data()+at);
        const bool valid=MetadataValid(reinterpret_cast<const unsigned long*>(flash.data()+256)) &&
            F13_Crc32Bytes(flash.data(),static_cast<unsigned long>(flash.size()-4))==Bu_Read32(flash.data()+flash.size()-4);
        check(valid==(at+4==image.size()),"mixed old/new app fails boot CRC until complete");
    }
    for (unsigned policy : {0U,1U,3U}) for (unsigned version : {0x10500U,0x10600U,0x10700U}) {
        g_metadata_words[4]=version;
        check(VersionAllowed(policy,0x10600UL,CAN_BOOT_CMD_START)==(policy!=3U || version>=0x10600U),
            "real Boot version matrix: invalid recovery/disabled/same/newer/older");
        check(VersionAllowed(policy,0x10600UL,CAN_BOOT_CMD_FORCE_START),"force START is request-local version override");
    }
    auto enabled=image;
    Bu_Write32(enabled.data()+0x174U,1U);
    check(MetadataValid(reinterpret_cast<const unsigned long*>(enabled.data()+256)),"version control1 accepted");
    Bu_Write32(enabled.data()+0x174U,2U);
    check(!MetadataValid(reinterpret_cast<const unsigned long*>(enabled.data()+256)),"unknown version policy rejected");
    check(device_version::PasswordMatches(L"3036") && !device_version::PasswordMatches(L"") &&
        !device_version::PasswordMatches(L"3036 ") && !device_version::PasswordMatches(L"03036"),"exact public maintenance password gate");
    f13_boot::TargetInfo version_target;
    version_target.app_version=0x10600U;
    for(unsigned policy:{0U,1U,3U}) for(unsigned version:{0x10500U,0x10600U,0x10700U}) {
        version_target.version_policy=policy; info.version=version;
        const bool blocked=policy==3U && version<version_target.app_version;
        check(device_version::Check(info,version_target,false,&error)==!blocked,"Tool normal version matrix");
        check(device_version::Check(info,version_target,true,&error)==blocked,"Tool force only for an actual blocked downgrade");
    }
    std::cout<<"Common policy / real F13 metadata / retry / reset / "<<cuts<<" simulated power cuts: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures;
}
