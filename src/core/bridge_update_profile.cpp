#include "bridge_update_profile.h"
#include <algorithm>

namespace firmware_upgrade::bridge_update {
namespace {
bool Send(host_link::Client& link, std::uint8_t command, const std::vector<std::uint8_t>& data,
          host_link::Response* reply, std::wstring* error, unsigned attempts=2U) {
    return link.RequestWithPolicy(static_cast<host_link::Command>(command),data,5000U,attempts,reply,error);
}
}
Image Inspect(const std::vector<std::uint8_t>& bytes) {
    Image out; out.size=bytes.size();
    if (bytes.size()<BU_MANIFEST_OFFSET+BU_METADATA_REGION_SIZE+4U) { out.error=L"Bridge app BIN is too short."; return out; }
    out.available=Bu_Read32(bytes.data()+BU_MANIFEST_OFFSET)==BU_IMAGE_MAGIC;
    out.calculated_crc=firmware_update::Crc32(bytes.data(),bytes.size()-4U);
    out.stored_crc=Bu_Read32(bytes.data()+bytes.size()-4U);
    out.crc_matches=out.calculated_crc==out.stored_crc;
    if (!Bu_ParseMetadataRegion(bytes.data()+BU_MANIFEST_OFFSET,&out.metadata))
        out.error=L"Not a bridge app manifest (BUR1). CAN device / G16 / raw BIN cannot update a host.";
    else if (out.metadata.image_size!=bytes.size()) out.error=L"Bridge manifest length does not match BIN size.";
    else if (!out.crc_matches) out.error=L"Bridge app CRC32 MISMATCH.";
    else out.valid=true;
    return out;
}
bool ReadInfo(host_link::Client& link, bu_info_t* info, std::wstring* error) {
    host_link::Response reply;
    if (!Send(link,BU_INFO_COMMAND,{},&reply,error)) return false;
    if (reply.payload.size()!=BU_INFO_SIZE || !Bu_ParseInfo(reply.payload.data(),info)) {
        *error=L"Bridge update descriptor is invalid."; return false;
    }
    if (info->flags&BU_FLAG_VALID_APP) {
        std::vector<std::uint8_t> region; bu_manifest_t manifest={};
        if (!ReadMetadata(link,&region,error)) return false;
        if (!Bu_ParseMetadataRegion(region.data(),&manifest) || !Bu_MatchesDevice(&manifest,info) ||
            manifest.version!=info->app_version || manifest.build!=info->app_build || manifest.image_size!=info->installed_size) {
            *error=L"Live256-byte bridge metadata differs from its descriptor."; return false;
        }
    }
    return true;
}
bool ReadMetadata(host_link::Client& link, std::vector<std::uint8_t>* region, std::wstring* error) {
    region->clear();
    for (std::uint16_t offset=0U; offset<BU_METADATA_REGION_SIZE; offset+=BU_METADATA_CHUNK_SIZE) {
        std::vector<std::uint8_t> request(4U); Bu_Write16(request.data(),offset);
        Bu_Write16(request.data()+2U,BU_METADATA_CHUNK_SIZE);
        host_link::Response reply;
        if (!Send(link,BU_READ_METADATA_COMMAND,request,&reply,error)) { region->clear(); return false; }
        if (reply.payload.size()!=4U+BU_METADATA_CHUNK_SIZE || Bu_Read16(reply.payload.data())!=offset ||
            Bu_Read16(reply.payload.data()+2U)!=BU_METADATA_REGION_SIZE) {
            *error=L"Bridge metadata page offset/total/length mismatch."; region->clear(); return false;
        }
        region->insert(region->end(),reply.payload.begin()+4U,reply.payload.end());
    }
    return true;
}
bool EnterBoot(host_link::Client& link, std::uint16_t board, std::wstring* error) {
    std::vector<std::uint8_t> request(4U); Bu_Write16(request.data(),board);
    request[2]=BU_PROTOCOL; request[3]=0xB0U;
    host_link::Response reply;
    return Send(link,BU_ENTER_COMMAND,request,&reply,error,1U);
}
bool Program(host_link::Client& link, const bu_info_t& device,
             const std::vector<std::uint8_t>& bytes, const std::function<bool()>& cancelled,
             const std::function<void(int)>& progress,
             const std::function<void()>& abort_transaction, std::wstring* error,
             const std::function<void(unsigned)>& stage) {
    const auto image=Inspect(bytes);
    if (!image.valid || device.mode!=BU_MODE_BOOT || !(device.flags&BU_FLAG_FLASH_READY) ||
        device.max_data==0U || device.max_data>BU_MAX_DATA || device.max_data%4U!=0U ||
        !Bu_MatchesDevice(&image.metadata,&device)) {
        *error=image.valid ? L"Bridge boot/image identity, geometry or state mismatch." : image.error; return false;
    }
    host_link::Response reply;
    bool started=false;
    const auto abort=[&]() {
        if (started && abort_transaction) abort_transaction();
    };
    if (cancelled && cancelled()) { *error=L"Bridge update cancelled before erase."; return false; }
    const std::vector<std::uint8_t> manifest(bytes.begin()+BU_MANIFEST_OFFSET,
        bytes.begin()+BU_MANIFEST_OFFSET+BU_METADATA_REGION_SIZE);
    // A fresh ABORT clears stale/incomplete staging without erasing app flash.
    started=true;
    if(stage) stage(0U);
    if (!Send(link,BU_ABORT_COMMAND,{},&reply,error)) { abort(); return false; }
    for (std::uint16_t at=0U; at<BU_METADATA_REGION_SIZE; at+=BU_METADATA_CHUNK_SIZE) {
        if (cancelled && cancelled()) { *error=L"Metadata staging cancelled before erase."; abort(); return false; }
        std::vector<std::uint8_t> page(4U+BU_METADATA_CHUNK_SIZE);
        Bu_Write16(page.data(),at); Bu_Write16(page.data()+2U,BU_METADATA_REGION_SIZE);
        std::copy_n(manifest.data()+at,BU_METADATA_CHUNK_SIZE,page.data()+4U);
        if (!Send(link,BU_STAGE_METADATA_COMMAND,page,&reply,error) || reply.payload.size()!=4U ||
            Bu_Read16(reply.payload.data())!=at+BU_METADATA_CHUNK_SIZE ||
            Bu_Read16(reply.payload.data()+2U)!=BU_METADATA_REGION_SIZE) {
            if (error->empty()) *error=L"Bridge metadata staging ACK mismatch.";
            abort(); return false;
        }
    }
    std::vector<std::uint8_t> begin(6U); Bu_Write16(begin.data(),BU_METADATA_REGION_SIZE);
    Bu_Write32(begin.data()+2U,firmware_update::Crc32(manifest.data(),manifest.size()));
    if (cancelled && cancelled()) { *error=L"Metadata staging cancelled before erase."; abort(); return false; }
    // BEGIN may have taken effect even when its ACK is lost; abort on failure.
    started=true;
    if(stage) stage(1U);
    if (!Send(link,BU_BEGIN_COMMAND,begin,&reply,error)) { abort(); return false; }
    if (reply.payload.size()!=4U || Bu_Read32(reply.payload.data())!=0U) {
        *error=L"Bridge BEGIN returned a stale offset."; abort(); return false;
    }
    std::uint32_t offset=0U;
    if(stage) stage(2U);
    while (offset<bytes.size()) {
        if (cancelled && cancelled()) { *error=L"Bridge update cancelled; boot retained."; abort(); return false; }
        const auto count=(std::min)(static_cast<std::size_t>(device.max_data),bytes.size()-offset);
        std::vector<std::uint8_t> request(4U+count); Bu_Write32(request.data(),offset);
        std::copy_n(bytes.data()+offset,count,request.data()+4U);
        if (!Send(link,BU_DATA_COMMAND,request,&reply,error) || reply.payload.size()!=4U ||
            Bu_Read32(reply.payload.data())!=offset+count) {
            if (error->empty()) *error=L"Bridge DATA offset ACK mismatch.";
            abort(); return false;
        }
        offset+=static_cast<std::uint32_t>(count);
        if (progress) progress(static_cast<int>(offset*100ULL/bytes.size()));
    }
    std::vector<std::uint8_t> crc(4U); Bu_Write32(crc.data(),image.stored_crc);
    if(cancelled && cancelled()) { *error=L"Bridge update canceled before END."; abort(); return false; }
    if(stage) stage(3U);
    if (!Send(link,BU_END_COMMAND,crc,&reply,error) || reply.payload.size()!=4U ||
        Bu_Read32(reply.payload.data())!=image.stored_crc) {
        if (error->empty()) *error=L"Bridge END CRC acknowledgement mismatch.";
        abort(); return false;
    }
    // Stay in boot after END; RUN is a separate explicit action/transaction.
    std::vector<std::uint8_t> readback;
    if (!ReadMetadata(link,&readback,error) || readback!=manifest) {
        if (error->empty()) *error=L"Programmed bridge metadata readback mismatch.";
        abort(); return false;
    }
    return true;
}
} // namespace firmware_upgrade::bridge_update
