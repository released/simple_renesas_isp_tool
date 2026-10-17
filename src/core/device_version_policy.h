#pragma once
#include "firmware_update_profile.h"
#include "f13_boot_profile.h"

namespace firmware_upgrade::device_version {
// Public maintenance confirmation, not authentication or a device secret.
inline bool PasswordMatches(const std::wstring& value) { return value==L"3036"; }
inline bool IsOlder(const firmware_update::ImageInfo& image,const f13_boot::TargetInfo& target) {
    return (target.version_policy&1U) && image.version<target.app_version;
}
inline bool IsBlocked(const firmware_update::ImageInfo& image,const f13_boot::TargetInfo& target) {
    return target.version_policy==3U && IsOlder(image,target);
}
inline bool Check(const firmware_update::ImageInfo& image,const f13_boot::TargetInfo& target,
                  bool force,std::wstring* error) {
    if(force && !IsBlocked(image,target)) {
        if(error) *error=L"Force operation requires an older image blocked by the installed policy. Use Start Device Update otherwise.";
        return false;
    }
    if(!force && IsBlocked(image,target)) {
        if(error) *error=L"Older firmware is blocked by the installed App version policy. Use force upgrade old version firmware for a confirmed one-time downgrade.";
        return false;
    }
    return true;
}
}
