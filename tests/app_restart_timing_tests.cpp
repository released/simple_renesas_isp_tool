#include "../src/core/app_confirmation_timing.h"
#include <cstdint>
#include <iostream>
#include <limits>

int main() {
    using namespace firmware_upgrade::evidence;
    int failures=0,checks=0;
    const auto check=[&](bool ok,const char* name) {
        ++checks;
        if (!ok) { ++failures; std::cerr<<"FAIL: "<<name<<'\n'; }
    };
    struct Sample { std::uint32_t bytes;std::uint64_t crc_ms;std::uint32_t quiet; };
    // Actual FD report size/END pairs; expectations are scheduling, not board success.
    const Sample samples[]={{4756,47,1000},{8192,79,1000},{24576,192,1000},
        {57344,416,1000},{122880,865,1460},{252928,1778,2476}};
    for (const auto& sample:samples) {
        const auto quiet=DeviceRestartQuietMs(sample.bytes,sample.crc_ms);
        check(quiet==sample.quiet,"real report size/END inputs select expected wait");
        AppConfirmationTiming timing(quiet);
        for (std::uint64_t now=0;now<quiet;++now)
            check(timing.Next(now,false)==AppConfirmationTiming::Action::Wait,"no probe before chosen quiet boundary");
        check(timing.Next(quiet,false)==AppConfirmationTiming::Action::Probe,"first probe at quiet boundary");
        std::uint64_t now=0;
        unsigned probes=0;
        const auto action=AwaitAppProbe(timing,[&]() {return now;},[]() {return false;},
            [&](std::uint32_t ms) {check(ms<=20,"bounded cancel slice");now+=ms;});
        if (action==AppConfirmationTiming::Action::Probe) ++probes;
        check(probes==1 && now>=quiet && now<quiet+20,"production wait loop reaches selected boundary without early probe");
        timing.ProbeFinished(now+70);
        check(timing.Next(now+169,false)==AppConfirmationTiming::Action::Wait &&
            timing.Next(now+170,false)==AppConfirmationTiming::Action::Probe,"100ms pacing from completed probe unchanged");
    }
    check(DeviceRestartQuietMs(119U*1024U,0)==1452,"F13 119KiB uses same generic policy");
    check(DeviceRestartQuietMs(120U*1024U,2000)==2500,"slower measured CRC overrides size estimate");
    check(DeviceRestartQuietMs(247U*1024U,1778,true)==4452,"ENTER reserves two installed-image checks");
    check(DeviceRestartQuietMs(4U*1024U,0,true)==1000,"small installed App retains minimum");
    check(DeviceRestartQuietMs(64U*1024U+1U,0)==1020,"size estimate rounds partial KiB up");
    check(DeviceRestartQuietMs(0)==6000 && DeviceRestartQuietMs(0xFFFFFFFFU)==6000,"unknown installed size uses bounded conservative fallback");
    check(DeviceRestartQuietMs(0xFFFFFFFEU,(std::numeric_limits<std::uint64_t>::max)(),true)==6000,"extreme inputs clamp without overflow");
    check(AppConfirmationTiming(0).QuietMs()==1000 && AppConfirmationTiming(7000).QuietMs()==6000,"constructor enforces shared limits");
    AppConfirmationTiming large(DeviceRestartQuietMs(247U*1024U,1778));
    std::uint64_t now=0;
    check(AwaitAppProbe(large,[&]() {return now;},[&]() {return now>=400;},
        [&](std::uint32_t ms) {now+=ms;})==AppConfirmationTiming::Action::Canceled && now==400,
        "cancel during long quiet interval starts no probe");
    large.ProbeFinished(11990);now=11990;
    check(AwaitAppProbe(large,[&]() {return now;},[]() {return false;},
        [&](std::uint32_t ms) {now+=ms;})==AppConfirmationTiming::Action::Expired,
        "long quiet interval does not extend 12-second probe budget");
    check(large.Next(12000,true)==AppConfirmationTiming::Action::Canceled,"cancel wins at budget boundary");
    std::cout<<"App restart timing: "<<(failures?"FAIL":"PASS")<<", "<<checks<<" checks; no COM/board I/O\n";
    return failures?1:0;
}
