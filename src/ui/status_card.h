#pragma once
#include <afxwin.h>
#include <string>

namespace firmware_upgrade::visual {
enum class Tone { Neutral, Ready, Pending, Error, Working };
struct Palette { COLORREF background; COLORREF text; COLORREF border; };
Palette Colors(Tone tone);
}
class CStatusCard : public CStatic {
    friend struct CanPrerequisiteUiFixture;
public:
    void SetTone(firmware_upgrade::visual::Tone tone);
    void SetEmphasized(bool emphasized);
    void SetInfoStyle() { info_style_=true; }
    void SetContent(const std::wstring& heading, const std::wstring& value,
                    const std::wstring& detail);
protected:
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()
private:
    firmware_upgrade::visual::Tone tone_ = firmware_upgrade::visual::Tone::Neutral;
    std::wstring heading_, value_, detail_;
    bool info_style_=false;
    bool emphasized_=false;
};
