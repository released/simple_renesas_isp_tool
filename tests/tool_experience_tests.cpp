#include "update_report_view.h"
#include "update_report_store.h"
#include "operation_guidance.h"
#include "preflight_summary.h"
#include "bridge_port_catalog.h"
#include "app_confirmation_timing.h"
#include "host_counter_evidence.h"
#include "../src/ui/update_feedback.h"
#include <iostream>

int TestToolExperience() {
    using namespace firmware_upgrade;
    int failures=0;
    const auto check=[&](bool ok,const char* name) {
        if(!ok) {std::cerr<<"FAIL Tool experience: "<<name<<'\n'; ++failures;}
    };
    check(evidence::HtmlEscape(L"<>&\"'")==L"&lt;&gt;&amp;&quot;&#39;","HTML metacharacters escaped");
    for(const wchar_t* result:{L"PASS",L"PASS_WITH_WARNINGS",L"FAIL",L"CANCELED"}) {
        evidence::ReportView r;
        r.result=result; r.operation=L"Offline model fixture (no hardware)";
        r.tool=L"test"; r.build=L"test"; r.recorded_utc=L"2026-10-18T00:00:00Z";
        r.message=L"<script>alert('unsafe')</script>";
        r.fields={{L"Device <name>",L"a&b\n\u6e2c\u8a66"},{L"Details",L"END confirmed; App execution not requested"}};
        r.phases={{L"Metadata",5},{L"Transfer",1234},{L"END",9}};
        r.guidance=guidance::Format(L"Host Link response timeout after retries.",L"CAN device update | COM6 | 921600");
        const auto html=evidence::RenderReport(r);
        check(html.find(L"<script>")==std::wstring::npos,"runtime values cannot inject script markup");
        check(html.find(L"&lt;script&gt;")!=std::wstring::npos,"exact error remains readable after escaping");
        check(html.find(L"Content-Security-Policy")!=std::wstring::npos && html.find(L"<script")==std::wstring::npos,
            "offline report has restrictive CSP and no scripts");
        check(html.find(L"1234")!=std::wstring::npos && html.find(result)!=std::wstring::npos,"results and timings rendered");
        if (r.result==L"PASS_WITH_WARNINGS")
            check(html.find(L"card canceled")!=std::wstring::npos,"successful update warning is amber, not green or failed");
        check(html.find(L"\u6e2c\u8a66")!=std::wstring::npos,"Unicode report preserved");
        const auto saved=evidence::SaveReportPair(L"{\"fixture\":true}",html);
        check(saved.error.empty() && !saved.html_path.empty() && !saved.json_path.empty(),"real JSON/HTML worker store pair");
        std::wstring error;
        check(!evidence::WriteReport(saved.json_path+L"\\blocked.json",L"test",&error) && !error.empty(),
            "report failure has an explicit error and cannot overwrite the existing JSON");
    }
    check(guidance::Explain(L"Host Link response timeout after retries.").layer==L"PC / Host Link","link timeout guidance");
    check(guidance::Explain(L"CAN transaction timeout.").layer==L"Host / device bus","CAN timeout guidance");
    const auto can_timeout=guidance::Explain(L"Host returned TIMEOUT for command 0x21.");
    check(can_timeout.layer==L"Host / device bus" && can_timeout.next.find(L"not a PC serial-response timeout")!=std::wstring::npos,
        "host-returned CAN timeout is not a serial response timeout");
    check(guidance::Explain(L"Metadata staging failed: Host returned TIMEOUT for command 0x21.").layer==L"Host / device bus",
        "CAN transaction status takes priority over operation-stage words");
    check(guidance::Explain(L"Host returned BAD_COMMAND for command 0x21.").layer==L"PC / Host Link",
        "other host statuses retain capability/protocol guidance");
    check(guidance::Explain(L"Image metadata mismatch.").layer==L"Image / device contract","image gate guidance");
    check(guidance::Explain(L"Older firmware downgrade blocked.").layer==L"Device version policy","version policy guidance");
    check(guidance::Explain(L"END CRC succeeded; APP_READY unconfirmed.").layer==L"App confirmation","unknown App result never implies rewrite");
    check(guidance::Explain(L"Operation canceled.").layer==L"Canceled","cancel cleanup advice");
    check(guidance::Explain(L"Unknown error 0xA1.").layer==L"Operation / state","unknown errors not guessed as hardware root cause");
    check(guidance::Explain(L"SetCommState failed (GetLastError=87)").layer==L"PC / Host Link","Win32 serial errors classified using actual transport format");
    check(guidance::Explain(L"Host Link requires128. Reflash matching bridge firmware.").layer==L"PC / Host Link","firmware compatibility advice is not a device Flash operation failure");
    check(guidance::Explain(L"Cannot finish the operation.").layer==L"Operation / state","cannot is not a CAN bus diagnosis");
    std::vector<evidence::Difference> rows;
    evidence::Gate(rows,L"Host",false,L"Not probed",L"Probe explicitly");
    evidence::Gate(rows,L"CRC",true,L"MATCH",L"Valid image");
    check(evidence::PreflightTitle(rows).find(L"1 blocked")!=std::wstring::npos,"preflight blocked count");
    rows[0].result=L"MATCH";
    check(evidence::PreflightTitle(rows).find(L"0 blocked")!=std::wstring::npos,"preflight satisfied count");
    check(evidence::PreflightTitle(rows).find(L"worker rechecks")!=std::wstring::npos,"snapshot does not authorize writes");
    host_link::Capabilities caps; caps.feature_flags=bridge::kFeaturePortCatalog;
    bridge::Port port;port.bus=bridge::Bus::Can;port.id=2;port.flags=1;port.rates=4;
    check(bridge::CanPortSupports(caps,{port},2,2,false),"preflight and worker accept advertised Classical port/rate");
    check(!bridge::CanPortSupports(caps,{port},1,2,false),"unknown port cannot appear ready");
    check(!bridge::CanPortSupports(caps,{port},2,1,false),"unsupported port rate blocked");
    check(!bridge::CanPortSupports(caps,{port},2,32,false),"out-of-range rate cannot shift by32");
    check(!bridge::CanPortSupports(caps,{port},2,2,true),"Classical-only port cannot appear FD ready");
    port.flags=5;check(bridge::CanPortSupports(caps,{port},2,2,true),"advertised FD port accepted");
    using feedback::Result;
    for (const auto result : {Result::DeviceAppConfirmed,Result::DeviceAppConfirmedBusWarning,Result::DeviceAppConfirmedCanActive,
                              Result::HostAppWritten,Result::HostAppRunning}) {
        const auto view=feedback::Describe(result);
        check(!view.title.empty() && !view.next.empty() && !view.progress.empty(),"completion has title, next step and footer");
        check(!view.compact.empty() && view.compact.find(L"\r\n")!=std::wstring::npos,"compact persistent result separates outcome and next action");
        check(view.status.find(L"[COMPLETE")==0,"persistent completion is explicit, not an idle prerequisite error");
    }
    check(feedback::Describe(Result::None).status.empty(),"generic discovery/percentage cannot create success feedback");
    const auto device=feedback::Describe(Result::DeviceAppConfirmed);
    check(device.status.find(L"CAN stopped")!=std::wstring::npos && !device.warning,"confirmed device has stopped CAN and clear success");
    check(device.next.find(L"Discover Target > Enter Boot (if App mode)")!=std::wstring::npos,"another update requires discovery and conditional ENTER");
    const auto active=feedback::Describe(Result::DeviceAppConfirmedCanActive);
    check(active.warning && active.next.find(L"may still be active")!=std::wstring::npos,"failed CAN stop is a separate warning");
    check(active.next.find(L"Do not repeat the update")!=std::wstring::npos,"cleanup warning never requests another erase");
    const auto written=feedback::Describe(Result::HostAppWritten);
    check(written.status.find(L"remains in Boot")!=std::wstring::npos &&
          written.next.find(L"not been started")!=std::wstring::npos,"host written does not claim execution");
    check(feedback::Describe(Result::HostAppRunning).status.find(L"Same-COM")!=std::wstring::npos,"host RUN confirmation is separate");
    check(!feedback::IsDevice(Result::HostAppWritten) && feedback::IsDevice(Result::DeviceAppConfirmedCanActive),"completion destination is typed");
    const auto bus_warning=feedback::WithHostCounterWarning(Result::DeviceAppConfirmed,true);
    check(bus_warning==Result::DeviceAppConfirmedBusWarning && feedback::IsDevice(bus_warning) &&
          feedback::Describe(bus_warning).warning,"confirmed update with new Host errors has a typed warning");
    check(feedback::Describe(bus_warning).next.find(L"Do not repeat the update")!=std::wstring::npos,
          "counter warning does not suggest another erase");
    check(feedback::WithHostCounterWarning(Result::DeviceAppConfirmedCanActive,true)==Result::DeviceAppConfirmedCanActive &&
          feedback::WithHostCounterWarning(Result::HostAppWritten,true)==Result::HostAppWritten &&
          feedback::WithHostCounterWarning(Result::None,true)==Result::None,
          "counter warning preserves cleanup warnings, Misc results and unconfirmed operations");
    check(feedback::ReportResult(true,false,bus_warning)==L"PASS_WITH_WARNINGS" &&
          feedback::ReportResult(false,false,bus_warning)==L"FAIL" &&
          feedback::ReportResult(false,true,bus_warning)==L"CANCELED",
          "failure and cancel cannot inherit successful warning classification");
    {
        using Timing=evidence::AppConfirmationTiming;
        Timing timing;
        check(timing.Next(0,false)==Timing::Action::Wait && timing.Next(999,false)==Timing::Action::Wait &&
              timing.Next(1000,false)==Timing::Action::Probe,"no first CAN probe during reset quiet interval");
        std::uint64_t now=0;
        std::uint32_t sleeps=0;
        const auto sleep=[&](std::uint32_t ms) {check(ms<=20,"cancel sleep slice stays bounded");now+=ms;++sleeps;};
        check(evidence::AwaitAppProbe(timing,[&]() {return now;},[]() {return false;},sleep)==Timing::Action::Probe &&
              now==1000 && sleeps==50,"production wait loop reaches quiet boundary without a CAN callback");
        now=1150;timing.ProbeFinished(now);
        check(timing.Next(1249,false)==Timing::Action::Wait && timing.Next(1250,false)==Timing::Action::Probe,
              "next read-only probe is paced from the completed attempt");
        now=0;sleeps=0;
        check(evidence::AwaitAppProbe(Timing{},[&]() {return now;},[&]() {return now>=400;},sleep)==Timing::Action::Canceled &&
              now==400,"cancel during quiet interval returns without a probe");
        timing.ProbeFinished(11990);now=11990;
        check(evidence::AwaitAppProbe(timing,[&]() {return now;},[]() {return false;},sleep)==Timing::Action::Expired,
              "quiet wait and probe pacing share the original total budget");
        check(timing.Next(12000,false)==Timing::Action::Expired && timing.Next(12000,true)==Timing::Action::Canceled,
              "expired or canceled wait never starts another probe");
    }
    {
        host_link::HostStats before,after;
        before.can_rx_frames=217;before.can_tx_frames=217;
        after=before;after.can_rx_frames=417;after.can_tx_frames=567;
        check(evidence::ObserveHostCounters(before,after,true).Status()==L"NO_RECORDED_ERRORS",
              "normal frame-count increases are not error warnings");
        before.can_warning_entries=6;before.can_error_passive_entries=8;
        after.can_warning_entries=7;after.can_error_passive_entries=9;
        const auto observed=evidence::ObserveHostCounters(before,after,true);
        check(observed.Status()==L"WARNING" && observed.deltas.find(L"CAN warning=1")!=std::wstring::npos &&
              observed.deltas.find(L"CAN error-passive=1")!=std::wstring::npos,
              "recorded reset-window warning/passive increases are not hidden by zero lost/bus-off/TX failures");
        after=before;
        check(evidence::ObserveHostCounters(before,after,true).Status()==L"NO_RECORDED_ERRORS",
              "historical cumulative errors alone do not warn on a later clean update");
        check(evidence::ObserveHostCounters(before,after,false).Status()==L"UNAVAILABLE",
              "missing snapshot or Misc reset is not a zero delta");
        after.can_warning_entries=0;
        check(evidence::ObserveHostCounters(before,after,true).Status()==L"UNAVAILABLE",
              "counter reset cannot silently wrap");
        after=before;after.uart_rx_overflows=65535;
        check(evidence::ObserveHostCounters(before,after,true).Status()==L"UNAVAILABLE",
              "16-bit UART saturation is not a measured delta");
        after=before;after.can_warning_entries=0xFFFFFFFFU;
        check(evidence::ObserveHostCounters(before,after,true).Status()==L"UNAVAILABLE",
              "32-bit CAN saturation is not a measured delta");
        after.can_error_passive_entries=before.can_error_passive_entries+1;
        const auto partial=evidence::ObserveHostCounters(before,after,true);
        check(partial.Status()==L"WARNING" && !partial.complete,"known error increase is retained beside unavailable counters");
    }
    for (bool host : {false,true}) for (bool discovered : {false,true}) for (bool boot : {false,true}) {
        const auto next=feedback::CanNextStep(host,discovered,boot);
        check(next.find(!host?L"Open + Probe Host":!discovered?L"Discover Target":boot?L"Enter Boot is not needed":L"Next: Enter Boot")!=std::wstring::npos,
              "next step follows host/discovery/Boot-App state");
    }
    std::cout<<"Tool experience tests: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures;
}
