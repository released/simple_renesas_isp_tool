#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace firmware_upgrade::evidence {
struct ReportView {
    std::wstring operation, result, recorded_utc, tool, build, message, guidance;
    std::vector<std::pair<std::wstring,std::wstring>> fields;
    std::vector<std::pair<std::wstring,std::uint64_t>> phases;
};
inline std::wstring HtmlEscape(const std::wstring& value) {
    std::wstring out;
    for(wchar_t c:value) {
        switch(c) {
        case L'&': out+=L"&amp;"; break;
        case L'<': out+=L"&lt;"; break;
        case L'>': out+=L"&gt;"; break;
        case L'\"': out+=L"&quot;"; break;
        case L'\'': out+=L"&#39;"; break;
        default: if(c>=32 || c==L'\n' || c==L'\r' || c==L'\t') out+=c; break;
        }
    }
    return out;
}
inline std::wstring RenderReport(const ReportView& r) {
    const std::wstring tone=r.result==L"PASS"?L"pass":
        (r.result==L"CANCELED" || r.result==L"PASS_WITH_WARNINGS")?L"canceled":L"fail";
    std::wstring out=LR"(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'">
<title>Firmware update report</title><style>
body{font:16px/1.55 "Segoe UI",sans-serif;background:#f4f7fb;color:#243347;margin:0;padding:24px}
main{max-width:1120px;margin:auto}h1{margin:0 0 8px}h2{font-size:20px;margin-top:28px}
.card{background:white;border:1px solid #b8c6d8;border-radius:8px;padding:18px;margin:14px 0}
.pass{border-left:8px solid #00865a;background:#e7fff3}.fail{border-left:8px solid #c4003d;background:#fff0f3}
.canceled{border-left:8px solid #b47700;background:#fff7d8}table{border-collapse:collapse;width:100%;background:white}
th,td{padding:9px 12px;border:1px solid #c7d1de;text-align:left;vertical-align:top;overflow-wrap:anywhere}
th{background:#e5f4ff}td:first-child{width:26%}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0;font:inherit}
.note{color:#485d76} @media print{body{padding:0;background:white}.card,tr{break-inside:avoid}h2{break-after:avoid}}
</style></head><body><main><h1>Firmware update report</h1>)";
    out+=L"<section class=\"card "+tone+L"\"><h2>"+HtmlEscape(r.result)+L" | "+HtmlEscape(r.operation)+L"</h2><pre>"+
        HtmlEscape(r.message.empty()?L"Operation completed.":r.message)+L"</pre></section>";
    out+=L"<p class=\"note\">Recorded UTC: "+HtmlEscape(r.recorded_utc)+L"<br>Tool: "+HtmlEscape(r.tool)+
        L" | Build: "+HtmlEscape(r.build)+L"</p><h2>Session and evidence</h2><table><thead><tr><th>Field</th><th>Recorded value</th></tr></thead><tbody>";
    for(const auto& f:r.fields) out+=L"<tr><td>"+HtmlEscape(f.first)+L"</td><td><pre>"+HtmlEscape(f.second)+L"</pre></td></tr>";
    out+=L"</tbody></table><h2>Phase durations</h2><table><thead><tr><th>Phase</th><th>Elapsed (ms)</th></tr></thead><tbody>";
    for(const auto& p:r.phases) out+=L"<tr><td>"+HtmlEscape(p.first)+L"</td><td>"+std::to_wstring(p.second)+L"</td></tr>";
    if(r.phases.empty()) out+=L"<tr><td colspan=\"2\">No completed phase timing was recorded.</td></tr>";
    out+=L"</tbody></table>";
    if(!r.guidance.empty()) out+=L"<h2>Suggested checks</h2><div class=\"card\"><pre>"+HtmlEscape(r.guidance)+L"</pre></div>";
    out+=L"<p class=\"note\">This is an update-operation record, separate from the App compile report. END/CRC and App execution are separate evidence. Host App programming requires an explicit RUN. CRC and live INFO are not authentication. Durations are Tool measurements, not analyzer bus time. No external assets or scripts are required.</p></main></body></html>";
    return out;
}
} // namespace firmware_upgrade::evidence
