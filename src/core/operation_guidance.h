#pragma once
#include <algorithm>
#include <cwctype>
#include <string>

namespace firmware_upgrade::guidance {
struct Advice {
    std::wstring layer;
    std::wstring next;
};
// These are inspection suggestions, not a diagnosis of the physical bus.
inline Advice Explain(const std::wstring& message) {
    std::wstring s=message;
    std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    const auto has=[&](const wchar_t* text){return s.find(text)!=std::wstring::npos;};
    if(has(L"cancel")) return {L"Canceled",L"Wait for worker cleanup. Probe/discover again before another update; interrupted App flash may require Boot recovery."};
    if(has(L"app_ready") || has(L"app reconnect") || has(L"run returned") || has(L"run sent"))
        return {L"App confirmation",L"Keep the current image and report. Check power and App initialization, then read live INFO. Do not repeat erase or RUN to resolve an unknown result."};
    if(has(L"host returned timeout for command 0x21."))
        return {L"Host / device bus",L"Host UART returned a CAN transaction timeout, not a PC serial-response timeout. Check attempts/last TX result, then compare the CAN request/reply with device UART FIFO/TX snapshots and analyzer trace. Check mode/BRS/rates/IDs and transceiver wiring. TX_END does not prove device software processed the request. Save the failure before resetting; do not erase or increase timeouts as a diagnosis."};
    if(has(L"downgrade") || (has(L"version") && has(L"older")))
        return {L"Device version policy",L"Use the same/newer App version, or the explicit force upgrade old version firmware maintenance action. Other image checks remain required."};
    if(has(L"metadata") || has(L"image") || has(L"irq abi") || has(L"geometry") || has(L"policy") || has(L"crc32"))
        return {L"Image / device contract",L"Open Preflight and Compare Metadata. Check destination, MCU, partition, update interface, IRQ ABI and CRC. Generate the BIN from the matching project; do not patch its bytes."};
    if(has(L"flash") && !has(L"reflash")) return {L"Device Flash",L"Keep the device in Boot recovery. Check the returned Flash status and supply, then re-probe the target before retrying. Save the report and trace."};
    if(has(L"can_error") || has(L"clock failed"))
        return {L"Host / CAN controller",L"Check the returned CAN error detail, bridge oscillator/clock setup, transceiver power and selected bus settings. Save the exact command/status; do not infer a device Flash failure from a bridge error."};
    if(has(L"host returned") || has(L"host link") || has(L"serial") || has(L"com port") || has(L"createfile") || has(L"windows error") || has(L"getlasterror="))
        return {L"PC / Host Link",L"Check the selected COM, another terminal using it, host power and Host Baud. Host Boot uses 115200. Check returned command/status and advertised firmware capabilities. Refresh and explicitly Open + Probe Host; a USB description is not MCU identity."};
    if(has(L"i2c") || has(L"nack")) return {L"Host / I2C device",L"Check the selected port, 7-bit address, SCL rate, pull-ups and device power. Repeat only the supported read-only discovery."};
    if(has(L"can ") || has(L"can/") || has(L"can:") || has(L"bus-off") || has(L"target") || has(L"response timeout"))
        return {L"Host / device bus",L"Check the selected bridge port, device power, CAN H/L, termination, transceiver supply, bitrate and request/response IDs. Compare Tool log with analyzer evidence; then discover the target again."};
    return {L"Operation / state",L"Save the exact error and session summary. Check Preflight, refresh live identity and verify the current operation stage before retrying."};
}
inline std::wstring Format(const std::wstring& message,const std::wstring& context=L"") {
    const auto advice=Explain(message);
    return L"Layer: "+advice.layer+(context.empty()?L"":L"\nContext: "+context)+
        L"\n\nExact error:\n"+message+L"\n\nSuggested checks:\n"+advice.next;
}
} // namespace firmware_upgrade::guidance
