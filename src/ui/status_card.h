#pragma once
#include <afxwin.h>
#include <string>
#include <vector>

namespace firmware_upgrade::visual {
enum class Tone { Neutral, Ready, Pending, Error, Working };
enum class Provenance { None, HostLive, DeviceLive, ImageFile };
struct Palette { COLORREF background; COLORREF text; COLORREF border; };
Palette Colors(Tone tone);
Palette SourceColors(Provenance source);
}
class CStatusCard : public CStatic {
    friend struct CanPrerequisiteUiFixture;
public:
    void SetTone(firmware_upgrade::visual::Tone tone);
    void SetEmphasized(bool emphasized);
    void SetInfoStyle() { info_style_=true; }
    void SetCompactDetail() { compact_detail_=true; }
    void SetProvenance(firmware_upgrade::visual::Provenance source);
    void SetContent(const std::wstring& heading, const std::wstring& value,
                    const std::wstring& detail);
protected:
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
private:
    std::vector<std::wstring> CompactDetailLines() const;
    firmware_upgrade::visual::Tone tone_ = firmware_upgrade::visual::Tone::Neutral;
    std::wstring heading_, value_, detail_;
    bool info_style_=false;
    bool compact_detail_=false;
    bool emphasized_=false;
    firmware_upgrade::visual::Provenance provenance_=firmware_upgrade::visual::Provenance::None;
};
