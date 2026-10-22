// Actual Tool and paired Boot parsers, without peripheral/Flash mocks.
#include "firmware_update_profile.h"
#include "image_metadata.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
int wmain(int argc,wchar_t** argv) {
    if(argc<2) return 2;
    for(int file=1;file<argc;++file) {
        std::ifstream stream(std::filesystem::path(argv[file]),std::ios::binary);
        if(!stream.is_open()) return 3;
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)),std::istreambuf_iterator<char>());
        firmware_upgrade::firmware_update::ImageInfo info;
        std::wstring error;
        if(!firmware_upgrade::firmware_update::ValidateImage(
            firmware_upgrade::firmware_update::Profile::CanUpgrade,bytes,&info,&error)) {
            std::wcerr<<L"Tool validation: "<<error<<L'\n';return 4;
        }
        std::array<unsigned long,64> words{};
        for(unsigned i=0;i<64;++i) {
            if(!ImageMetadataRead32(bytes.data()+256,256,static_cast<unsigned short>(i*4U),&words[i])) return 5;
        }
        const auto status=ImageMetadataValidate(words.data());
        if(status) { std::cerr<<"Boot metadata status="<<static_cast<unsigned>(status)<<'\n';return 6; }
        const auto originalSize=words[0x18U/4U];
        const auto capacity=words[0x20U/4U];
        words[0x18U/4U]=capacity+4UL;
        if(ImageMetadataValidate(words.data())==0U) return 7;
        words[0x18U/4U]=originalSize;
        words[0x20U/4U]=capacity+1024UL;
        if(ImageMetadataValidate(words.data())==0U) return 8;
    }
    std::cout<<"PASS actual Tool/Boot parsers: "<<argc-1<<" images\n";
    return 0;
}
