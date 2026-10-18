#pragma once
#include "update_evidence.h"

namespace firmware_upgrade::evidence {
// A read-only snapshot for explanation; worker/device gates remain authoritative.
inline void Gate(std::vector<Difference>& rows,const wchar_t* field,bool passed,
                 const std::wstring& current,const std::wstring& action) {
    rows.push_back({field,current,action,passed?L"MATCH":L"MISMATCH"});
}
inline std::wstring PreflightTitle(const std::vector<Difference>& rows) {
    std::size_t blocked=0;
    for(const auto& r:rows) if(r.result==L"MISMATCH") ++blocked;
    return L"Preflight snapshot | "+std::to_wstring(blocked)+L" blocked | worker rechecks before programming";
}
}
