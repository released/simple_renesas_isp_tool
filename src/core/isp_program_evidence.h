#pragma once
#include "isp_program_state.h"
#include "update_evidence.h"

namespace firmware_upgrade::isp {
inline std::wstring ProgramEvidenceText(const ProgramState& state) {
    return std::wstring(L"last confirmed=")+StageName(state.last_confirmed)+
        L"; START="+OutcomeName(state.start)+L"; END/EOT="+OutcomeName(state.end)+
        L"; accepted bytes="+std::to_wstring(state.accepted_bytes)+
        L" (session acceptance); END/EOT ms="+std::to_wstring(state.end_elapsed_ms);
}
inline std::wstring ProgramEvidenceJson(const ProgramState& state) {
    return L"{\"last_confirmed\":"+evidence::JsonString(StageName(state.last_confirmed))+
        L",\"start\":"+evidence::JsonString(OutcomeName(state.start))+
        L",\"end\":"+evidence::JsonString(OutcomeName(state.end))+
        L",\"accepted_bytes\":"+std::to_wstring(state.accepted_bytes)+
        L",\"end_elapsed_ms\":"+std::to_wstring(state.end_elapsed_ms)+L"}";
}
} // namespace firmware_upgrade::isp
