#pragma once

#include <afxcmn.h>
#include <afxwin.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "core/firmware_update_profile.h"
#include "core/serial_port_catalog.h"
#include "worker_session.h"
#include "ui/status_card.h"

class CMainFrame : public CFrameWnd {
public:
    CMainFrame();
    ~CMainFrame() override;

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    BOOL PreTranslateMessage(MSG* message) override;
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* info);

    afx_msg void OnBnClickedRefreshCom();
    afx_msg void OnBnClickedOpenCom();
    afx_msg void OnBnClickedCloseCom();
    afx_msg void OnBnClickedResetHost();
    afx_msg void OnBnClickedLoadBinary();
    afx_msg void OnBnClickedConnectTarget();
    afx_msg void OnBnClickedEnterBootloader();
    afx_msg void OnBnClickedTransferTest();
    afx_msg void OnBnClickedCanTxTest();
    afx_msg void OnBnClickedStartFirmwareUpdate();
    afx_msg void OnForceOldFirmware();
    afx_msg void OnBnClickedDisconnect();
    afx_msg void OnBnClickedClearLog();
    afx_msg void OnBnClickedStartLog();
    afx_msg void OnBnClickedStopLog();
    afx_msg void OnBnClickedExportSummary();
    afx_msg void OnCancelOperation();
    afx_msg void OnCompareMetadata();
    afx_msg void OnBnClickedDetailedTrace();
    afx_msg void OnCbnSelChangeCom();
    afx_msg void OnCbnSelChangeBaud();
    afx_msg void OnCbnSelChangeCanBitrate();
    afx_msg void OnCbnSelChangeCanMode();
    afx_msg void OnCbnSelChangeCanFilter();
    afx_msg void OnCbnSelChangeProfile();
    afx_msg void OnEnChangeApplicationStart();
    afx_msg HBRUSH OnCtlColor(CDC* dc, CWnd* window, UINT control_type);
    afx_msg BOOL OnEraseBkgnd(CDC* dc);
    afx_msg void OnBusTabChanged(NMHDR* header, LRESULT* result);
    afx_msg void OnBusPortChanged();
    afx_msg void OnI2cProbe();
    afx_msg void OnI2cSettingsChanged();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnClose();
    afx_msg LRESULT OnWorkerEvent(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    enum FirmwareFieldIndex : std::size_t {
        kFirmwareFieldBinSize = 0U,
        kFirmwareFieldCalculatedCrc,
        kFirmwareFieldStoredCrc,
        kFirmwareFieldChecksumResult,
        kFirmwareFieldMetadataStatus,
        kFirmwareFieldMcu,
        kFirmwareFieldAppVersion,
        kFirmwareFieldBuild,
        kFirmwareFieldTarget,
        kFirmwareFieldProtocolFormat,
        kFirmwareFieldAppRange,
        kFirmwareFieldImageCapacity,
        kFirmwareFieldFlashBoot,
        kFirmwareFieldMinimumErase,
        kFirmwareFieldCanRates,
        kFirmwareFieldCanIds,
        kFirmwareFieldUpdatePolicy,
        kFirmwareFieldBootAbi,
        kFirmwareFieldVersionControl,
        kFirmwareFieldInstalledVersionPolicy,
        kFirmwareFieldCount
    };

    void LayoutControls(int cx, int cy);
    void RefreshComPorts();
    bool QueueOpenSelectedCom();
    void QueueDisconnect();
    void AppendLog(const std::wstring& text);
    void UpdateUi();
    void StartDeviceUpdate(bool force_old_version);
    bool ConfirmForceOldFirmware();
    bool InitializeBusUi();
    bool IsI2cTab() const;
    bool IsMiscTab() const;
    bool IsI2cSelectionCurrent() const;
    void UpdateMiscUi();
    void UpdateBridgeImageFields();
    void OnBridgeAction(unsigned action);
    void UpdateBusUi();
    bool InitializeOverviewUi();
    void UpdateOverviewUi(bool i2c_current);
    void RefreshBusPorts();
    void RefreshI2cRates();
    bool SelectedI2cSettings(firmware_upgrade::i2c_host::Configuration* configuration,
                            std::uint8_t* address) const;
    void UpdateI2cImageFields(bool target_current);
    void LayoutInterfaceControls(int x, int y, int width, int height, int row);
    void QueueCanConfiguration();
    void RefreshNominalChoices();
    bool IsComOpen() const;
    std::wstring SelectedComPort() const;
    DWORD SelectedBaudrate() const;
    std::wstring SelectedCanBitrate() const;
    bool SelectedCanSettings(firmware_upgrade::ui::CanSettings* settings,
                             std::wstring* error) const;
    std::wstring SelectedProfile() const;
    bool SelectedApplicationStart(std::uint32_t* application_start,
                                  std::wstring* error) const;
    void SetProgress(int value, const std::wstring& label);
    void ApplyWorkerEvent(const firmware_upgrade::ui::WorkerEvent& event);
    void InitializeSettingsFile();
    void LoadUiSettings();
    void SaveUiSettings();
    std::wstring ReadIniValue(const wchar_t* key,
                              const wchar_t* fallback) const;
    void WriteIniValue(const wchar_t* key,
                       const std::wstring& value);
    std::wstring BuildI2cSummary() const;
    std::wstring BuildBridgeSummary() const;
    void ApplyComboSelection(CComboBox& combo, const std::wstring& value);
    void StartLogRecording();
    void StopLogRecording(bool emit_log);
    void ExportSessionSummary();
    std::wstring BuildSessionSummary() const;
    void SaveUpdateReport(const std::wstring& json);
    bool LoadBinaryFromPath(const std::wstring& path, bool silent);
    void ValidateLoadedFirmwareImage(bool emit_log);
    void UpdateFirmwareImageInfoText();
    void UpdateTargetMetadataText();
    void StartOperationElapsedTimer();
    void StopOperationElapsedTimer();
    void UpdateOperationElapsedText();

private:
    CFont ui_font_;

    CStatic com_label_;
    CComboBox com_combo_;
    std::vector<firmware_upgrade::serial_ports::PortInfo> com_ports_;
    CStatic baud_label_;
    CComboBox baud_combo_;
    CButton refresh_com_btn_;
    CButton open_com_btn_;
    CButton close_com_btn_;
    CButton reset_host_btn_;
    CButton cancel_btn_;
    CButton compare_metadata_btn_;
    CStatic build_info_label_;

    CStatusCard host_info_label_;
    CStatusCard target_info_label_;
    std::array<CStatusCard, 3> firmware_overview_cards_;
    CToolTipCtrl overview_tooltip_;
    CStatic target_metadata_label_;
    CStatusCard interface_heading_;
    CStatusCard image_heading_;
    CStatusCard log_heading_;

    CButton load_binary_btn_;
    CStatic file_path_label_;
    CStatic app_start_label_;
    CEdit app_start_edit_;
    std::array<CStatic, kFirmwareFieldCount> firmware_field_labels_;
    std::array<CStatic, kFirmwareFieldCount> firmware_field_values_;

    CStatic can_bitrate_label_;
    CComboBox can_bitrate_combo_;
    std::uint32_t displayed_nominal_mask_ = 0U;
    CStatic can_mode_label_;
    CComboBox can_mode_combo_;
    CStatic can_data_label_;
    CComboBox can_data_combo_;
    CButton can_tx_test_btn_;
    CStatic can_tx_test_help_;
    CToolTipCtrl can_tx_test_tooltip_;
    CStatic can_filter_label_;
    CComboBox can_filter_combo_;
    CStatic can_filter_id_label_;
    CEdit can_filter_id_edit_;
    CStatic can_filter_mask_label_;
    CEdit can_filter_mask_edit_;
    CStatic profile_label_;
    CComboBox profile_combo_;

    CButton connect_target_btn_;
    CButton enter_bootloader_btn_;
    CButton transfer_test_btn_;
    CButton disconnect_btn_;
    CStatic operation_timer_label_;

    CButton start_firmware_update_btn_;
    CButton force_old_firmware_btn_;
    CStatusCard firmware_prerequisites_label_;
    CStatusCard operation_status_card_;
    CTabCtrl bus_tabs_;
    CStatic bus_port_label_;
    CComboBox bus_port_combo_;
    CStatic i2c_speed_label_;
    CComboBox i2c_speed_combo_;
    CStatic i2c_address_label_;
    CEdit i2c_address_edit_;
    CButton i2c_probe_btn_;
    CStatusCard i2c_notes_;
    std::vector<firmware_upgrade::bridge::Port> displayed_ports_;
    bool displayed_i2c_ = false;
    int active_tab_ = 0;
    std::uint8_t preferred_can_port_ = 0U;
    std::uint8_t preferred_i2c_port_ = 0U;
    std::uint8_t preferred_i2c_speed_ = 0U;
    std::wstring last_operation_error_;
    firmware_upgrade::g16_i2c::ImageInspection binary_i2c_inspection_;
    firmware_upgrade::bridge_update::Image binary_bridge_inspection_;
    CBrush page_background_brush_;
    CBrush ready_background_brush_;
    CBrush error_background_brush_;
    CBrush pending_background_brush_;
    CBrush neutral_background_brush_;

    CProgressCtrl progress_bar_;
    CStatic progress_label_;
    std::wstring progress_operation_ = L"Progress";

    CRichEditCtrl log_edit_;
    CButton clear_log_btn_;
    CButton start_log_btn_;
    CButton stop_log_btn_;
    CButton export_summary_btn_;
    CButton detailed_trace_check_;

    std::wstring binary_path_;
    std::wstring preferred_com_port_;
    std::wstring ini_path_;
    bool loading_ui_settings_ = true;
    bool ini_write_error_reported_ = false;
    std::wstring log_record_path_;
    FILE* log_record_fp_ = nullptr;
    bool log_recording_ = false;
    std::vector<std::uint8_t> binary_data_;
    firmware_upgrade::firmware_update::ImageInspection binary_inspection_;
    bool binary_image_valid_ = false;
    std::wstring binary_image_validation_error_;
    std::wstring summary_host_;
    std::wstring summary_target_;
    std::wstring summary_can_;
    std::wstring summary_image_;
    std::wstring summary_result_ = L"No operation completed.";
    std::uint64_t summary_host_requests_ = 0U;
    std::uint64_t summary_host_retries_ = 0U;
    std::uint64_t summary_host_timeouts_ = 0U;
    std::uint64_t summary_unmatched_frames_ = 0U;
    std::uint64_t summary_received_events_ = 0U;
    std::uint64_t summary_dropped_events_ = 0U;
    std::uint32_t summary_arbitration_lost_entries_ = 0U;
    bool summary_f24_stats_valid_ = false;
    firmware_upgrade::host_link::HostStats summary_f24_stats_;
    ULONGLONG summary_elapsed_ms_ = 0U;

    firmware_upgrade::ui::WorkerSession worker_session_;
    firmware_upgrade::ui::SessionState session_state_;
    bool worker_started_ = false;
    bool worker_command_pending_ = false;
    bool closing_ = false;
    bool operation_timer_running_ = false;
    bool layout_in_progress_ = false;
    ULONGLONG operation_timer_start_ms_ = 0U;
};
