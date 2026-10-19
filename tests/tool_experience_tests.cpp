#include "update_report_view.h"
#include "update_report_store.h"
#include "operation_guidance.h"
#include "preflight_summary.h"
#include "bridge_port_catalog.h"
#include "../src/ui/update_feedback.h"
#include <iostream>

int TestToolExperience() {
    using namespace firmware_upgrade;
    int failures=0;
    const auto check=[&](bool ok,const char* name) {
        if(!ok) {std::cerr<<"FAIL Tool experience: "<<name<<'\n'; ++failures;}
    };
    check(evidence::HtmlEscape(L"<>&\"'")==L"&lt;&gt;&amp;&quot;&#39;","HTML metacharacters escaped");
    for(const wchar_t* result:{L"PASS",L"FAIL",L"CANCELED"}) {
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
        check(html.find(L"\u6e2c\u8a66")!=std::wstring::npos,"Unicode report preserved");
        const auto saved=evidence::SaveReportPair(L"{\"fixture\":true}",html);
        check(saved.error.empty() && !saved.html_path.empty() && !saved.json_path.empty(),"real JSON/HTML worker store pair");
        std::wstring error;
        check(!evidence::WriteReport(saved.json_path+L"\\blocked.json",L"test",&error) && !error.empty(),
            "report failure has an explicit error and cannot overwrite the existing JSON");
    }
    check(guidance::Explain(L"Host Link response timeout after retries.").layer==L"PC / Host Link","link timeout guidance");
    check(guidance::Explain(L"CAN transaction timeout.").layer==L"Host / device bus","CAN timeout guidance");
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
    for (const auto result : {Result::DeviceAppConfirmed,Result::DeviceAppConfirmedCanActive,
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
    for (bool host : {false,true}) for (bool discovered : {false,true}) for (bool boot : {false,true}) {
        const auto next=feedback::CanNextStep(host,discovered,boot);
        check(next.find(!host?L"Open + Probe Host":!discovered?L"Discover Target":boot?L"Enter Boot is not needed":L"Next: Enter Boot")!=std::wstring::npos,
              "next step follows host/discovery/Boot-App state");
    }
    std::cout<<"Tool experience tests: "<<(failures?"FAIL":"PASS")<<'\n';
    return failures;
}
