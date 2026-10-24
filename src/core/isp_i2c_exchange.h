#pragma once
#include "isp_device_client.h"
#include <algorithm>
#include <chrono>
#include <thread>
namespace firmware_upgrade::isp {
// Each raw transaction ends with STOP. A missing reply never resends a request.
using I2cOperation=std::function<bool(const std::vector<std::uint8_t>&,std::uint16_t,
    std::uint16_t,std::vector<std::uint8_t>*,std::wstring*)>;
inline bool MailboxExchange(const I2cOperation& operation,const std::function<bool()>& canceled,
    const std::vector<std::uint8_t>& request,std::uint32_t timeout,
    std::vector<std::uint8_t>* reply,std::wstring* error) {
    const auto fail=[&](const wchar_t* text){if(error)*error=text;return false;};
    if(!operation||!reply||request.size()<14U||request.size()>ISP_FRAME_MAX||!timeout||timeout>30000U)
        return fail(L"Invalid I2C ISP request or deadline.");
    const auto start=std::chrono::steady_clock::now();
    const auto elapsed=[&](){return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count());};
    std::vector<std::uint8_t> bytes;reply->clear();
    if(canceled&&canceled())return fail(L"I2C ISP canceled before request.");
    if(!operation(request,0U,static_cast<std::uint16_t>(timeout),&bytes,error))return false;
    while(elapsed()<timeout) {
        if(canceled&&canceled())return fail(L"I2C ISP canceled while polling a pending response.");
        const auto used=elapsed();if(used>=timeout)break;
        if(!operation({},111U,static_cast<std::uint16_t>(timeout-used),&bytes,error))return false;
        if(bytes.size()!=111U)return fail(L"I2C ISP mailbox must return exactly111 bytes.");
        if(bytes[0]==0U) {
            if(!std::all_of(bytes.begin()+1,bytes.end(),[](std::uint8_t b){return b==0xFFU;}))return fail(L"Invalid BUSY mailbox padding.");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;
        }
        if(bytes[0]!=0xA5U)return fail(L"Invalid I2C mailbox state byte.");
        const auto size=static_cast<std::size_t>(bytes[9]|(bytes[10]<<8))+14U;
        if(size>ISP_FRAME_MAX||!std::all_of(bytes.begin()+1+size,bytes.end(),[](std::uint8_t b){return b==0xFFU;}))return fail(L"Invalid I2C ready frame length/padding.");
        reply->assign(bytes.begin()+1,bytes.begin()+1+size);return true;
    }
    return fail(L"I2C reply deadline expired. Request was sent once; no Flash command was repeated.");
}
}
