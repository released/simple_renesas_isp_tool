#include "main_frame.h"
#include "resource.h"
#include "ui/overview_model.h"

bool CMainFrame::InitializeOverviewUi() {
    const UINT ids[] = {ID_BOOT_OVERVIEW, ID_INSTALLED_APP_OVERVIEW, ID_BIN_OVERVIEW};
    for (std::size_t i = 0U; i < firmware_overview_cards_.size(); ++i) {
        if (!firmware_overview_cards_[i].Create(L"", WS_CHILD | WS_VISIBLE, CRect(), this, ids[i])) return false;
        firmware_overview_cards_[i].SetFont(&ui_font_);
    }
    if (!overview_tooltip_.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX)) return false;
    overview_tooltip_.SetMaxTipWidth(600);
    CStatusCard* cards[] = {&host_info_label_, &target_info_label_,
        &firmware_overview_cards_[0], &firmware_overview_cards_[1], &firmware_overview_cards_[2]};
    for (auto* card : cards) {
        card->SetInfoStyle();
        card->ModifyStyle(0, SS_NOTIFY);
        overview_tooltip_.AddTool(card, L"N/A");
    }
    CWnd* shared[]={&file_path_label_,&target_metadata_label_,&firmware_prerequisites_label_,&i2c_notes_};
    for (auto* control:shared) { control->ModifyStyle(0,SS_NOTIFY); overview_tooltip_.AddTool(control,L"N/A"); }
    for (auto& control:firmware_field_values_) {
        control.ModifyStyle(0,SS_NOTIFY); overview_tooltip_.AddTool(&control,L"N/A");
    }
    return true;
}
void CMainFrame::UpdateOverviewUi(bool i2c_current) {
    namespace overview = firmware_upgrade::overview;
    overview::Input input;
    input.i2c = IsI2cTab();
    input.misc=IsMiscTab(); input.bridge_current=session_state_.bridge_info_valid;
    input.bridge=session_state_.bridge_info; input.bridge_image=binary_bridge_inspection_;
    input.host_current = session_state_.host_validated;
    input.host = session_state_.host_info;
    input.port = session_state_.port_name;
    input.target_current = input.i2c ? i2c_current : session_state_.target_validated;
    input.kind = input.i2c ? overview::TargetKind::G16Reference :
        (session_state_.target_profile == firmware_upgrade::ui::TargetProfile::F13Transfer ?
            overview::TargetKind::Transfer : overview::TargetKind::CanUpgrade);
    input.target = session_state_.target_metadata;
    input.transfer = session_state_.transfer_metadata;
    input.g16 = session_state_.i2c_metadata;
    input.bin_size = binary_data_.size();
    input.image = binary_inspection_;
    input.i2c_image = binary_i2c_inspection_;
    const auto cards = overview::Build(input);
    CStatusCard* controls[] = {&host_info_label_, &target_info_label_,
        &firmware_overview_cards_[0], &firmware_overview_cards_[1], &firmware_overview_cards_[2]};
    for (std::size_t i = 0U; i < cards.size(); ++i) {
        const auto& card = cards[i];
        using firmware_upgrade::visual::Tone;
        Tone tone = Tone::Neutral;
        switch (card.signal) {
        case overview::Signal::Verified: tone = Tone::Ready; break;
        case overview::Signal::Reported: tone = Tone::Pending; break;
        case overview::Signal::Offline: tone = Tone::Working; break;
        case overview::Signal::Invalid: tone = Tone::Error; break;
        default: break;
        }
        controls[i]->SetContent(card.heading, card.value, card.detail);
        controls[i]->SetTone(tone);
        const std::wstring full_text = card.heading + L"\r\n" + card.value + L"\r\n" + card.detail;
        overview_tooltip_.UpdateTipText(full_text.c_str(), controls[i]);
    }
    CWnd* shared[]={&file_path_label_,&target_metadata_label_,&firmware_prerequisites_label_,&i2c_notes_};
    for (auto* control:shared) { CString text; control->GetWindowTextW(text); overview_tooltip_.UpdateTipText(text,control); }
    for (auto& control:firmware_field_values_) {
        CString text; control.GetWindowTextW(text); overview_tooltip_.UpdateTipText(text,&control);
    }
}
