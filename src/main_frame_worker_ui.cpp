#include "main_frame.h"
#include "core/text_file.h"


void CMainFrame::FlushWorkerLogBatch() {
    if(!worker_log_batch_.empty()) {
        firmware_upgrade::log_view::AppendBatch(log_edit_.GetSafeHwnd(),worker_log_batch_,log_auto_scroll_);
        worker_log_batch_.clear();
    }
    if (log_recording_ && log_record_fp_ != nullptr) {
        std::wstring error;
        if (!firmware_upgrade::text_file::Flush(log_record_fp_, &error)) HandleLogFailure(error);
    }
}

LRESULT CMainFrame::OnWorkerEvent(WPARAM,LPARAM) {
    // WM_APP only schedules work. A low-priority timer leaves paint/input time
    // even when detailed trace produces several events for each CAN block.
    if (!closing_) SetTimer(kWorkerEventPumpTimerId,40U,nullptr);
    return 0;
}

void CMainFrame::PumpWorkerEvents() {
    const auto events=worker_session_.DrainEvents(64U);
    batching_worker_events_=true;
    for(const auto& event:events) {
        if(event.kind!=firmware_upgrade::ui::WorkerEventKind::Log) {
            FlushWorkerLogBatch();
            // Inspection/report/state handlers may append several related lines.
            // Keep them in one RichEdit batch; terminal dialogs must flush first.
            batching_worker_events_=event.kind!=firmware_upgrade::ui::WorkerEventKind::OperationComplete;
            ApplyWorkerEvent(event);
            batching_worker_events_=true;
        } else {
            ApplyWorkerEvent(event);
        }
    }
    batching_worker_events_=false;
    FlushWorkerLogBatch();
    if(!worker_session_.HasPendingEvents() && !worker_session_.IsOperationPending())
        KillTimer(kWorkerEventPumpTimerId);
}

void CMainFrame::StopProgressActivity() {
    if(!progress_indeterminate_) return;
    progress_bar_.SetMarquee(FALSE,0U);
    progress_bar_.ModifyStyle(PBS_MARQUEE,0);
    progress_indeterminate_=false;
    progress_bar_.SetPos(0);
    progress_label_.SetWindowTextW((progress_operation_+L": stopped").c_str());
}

void CMainFrame::FinishWorkerShutdown() {
    KillTimer(kWorkerShutdownTimerId);
    // IsStopped is set only after the worker releases its transport resources.
    // The final join no longer waits for device I/O or UI message progress.
    worker_session_.StopAndJoin();
    StopLogRecording(false);
    CFrameWnd::OnClose();
}

void CMainFrame::ApplyWorkerEvent(
    const firmware_upgrade::ui::WorkerEvent& event) {
    using firmware_upgrade::ui::WorkerEventKind;
    const auto outcome = event.ResolvedOutcome();

    switch (event.kind) {
        case WorkerEventKind::Log:
            AppendLog(event.text);
            break;
        case WorkerEventKind::State:
            if (!session_state_.serial_open && event.state.serial_open) {
                SetProgress(0,L"Progress");
                last_update_result_=firmware_upgrade::feedback::Result::None;
            }
            session_state_ = event.state;
            if (event.state.busy)
                last_update_result_=firmware_upgrade::feedback::Result::None;
            if(event.state.host_validated && !last_probe_port_.empty() && event.state.port_name==last_probe_port_) {
                selected_com_instance_=pending_com_instance_;
                verified_com_port_=last_probe_port_;
                last_probe_port_.clear(); pending_com_instance_.clear(); com_device_notice_.clear(); SaveUiSettings();
            }
            if (!event.state.host_summary.empty()) {
                summary_host_ = event.state.host_summary;
            }
            if (!event.state.target_summary.empty()) {
                summary_target_ = event.state.target_summary;
            }
            if (!event.state.can_summary.empty()) {
                summary_can_ = event.state.can_summary;
            }
            if (!event.state.image_summary.empty()) {
                summary_image_ = event.state.image_summary;
            }
            summary_host_requests_ = event.state.host_requests;
            summary_host_retries_ = event.state.host_retries;
            summary_host_timeouts_ = event.state.host_timeouts;
            summary_unmatched_frames_ = event.state.unmatched_frames;
            summary_received_events_ = event.state.received_events;
            summary_dropped_events_ = event.state.dropped_events;
            summary_arbitration_lost_entries_ =
                event.state.arbitration_lost_entries;
            if (event.state.f24_stats_valid) {
                summary_f24_stats_ = event.state.f24_stats;
                summary_f24_stats_valid_ = true;
            }
            worker_command_pending_ = event.state.busy || worker_session_.IsOperationPending();
            UpdateUi();
            break;
        case WorkerEventKind::Progress:
            SetProgress(event.progress, event.text, event.indeterminate);
            break;
        case WorkerEventKind::UpdateReport:
            SaveUpdateReport(event);
            break;
        case WorkerEventKind::ImageLoaded:
            ApplyLoadedImage(event);
            break;
        case WorkerEventKind::OperationComplete:
            StopProgressActivity();
            if (image_loading_) {
                image_loading_ = false;
                // BIN inspection is offline preparation, not a completed update.
                // Keep its result in the cards/log and return the footer to idle.
                SetProgress(0, L"Progress");
                if (!event.success) {
                    file_path_label_.SetWindowTextW((L"Image load failed: " + binary_path_).c_str());
                }
            }
            worker_command_pending_ = false;
            last_update_result_=event.update_result;
            last_operation_error_ = event.success ? L"" : event.text;
            last_error_guidance_=event.success?L"":event.guidance;
            summary_elapsed_ms_ = operation_timer_running_ ?
                GetTickCount64() - operation_timer_start_ms_ : 0U;
            summary_result_ = firmware_upgrade::feedback::ReportResult(outcome,event.update_result) + L": " + event.text;
            StopOperationElapsedTimer();
            if (event.success) {
                AppendLog(L"Operation completed successfully: " + event.text);
            } else {
                AppendLog(std::wstring(outcome == firmware_upgrade::operation::Outcome::Canceled ?
                    L"Operation canceled: " : L"Operation failed: ") + event.text);
                const auto advice=event.advice.layer.empty()?firmware_upgrade::guidance::Explain(event.text,
                    firmware_upgrade::guidance::Transport::Unspecified,outcome):event.advice;
                AppendLog(L"Suggested checks ["+advice.layer+L"]: "+advice.next);
                if (!closing_ && outcome != firmware_upgrade::operation::Outcome::Canceled) {
                    MessageBoxW(
                        (event.guidance.empty()?firmware_upgrade::guidance::Format(event.text,L"",advice):event.guidance).c_str(), L"Operation Error / Suggested Checks",
                        MB_ICONERROR | MB_OK);
                }
            }
            UpdateUi();
            if (event.success && event.update_result!=firmware_upgrade::feedback::Result::None && !closing_) {
                auto feedback=firmware_upgrade::feedback::Describe(event.update_result);
                if (session_state_.SerialCleanupUnconfirmed()) {
                    feedback.next = L"Device bus cleanup is not confirmed. Next: Close COM, then Open + Probe Host and Discover Target. The confirmed App result is retained.";
                }
                if (event.report_operation_id && event.report_operation_id == report_status_.operation) {
                    feedback.next += L"\r\n\r\n" + report_status_.Details();
                }
                SetProgress(100,feedback.progress);
                MessageBoxW((feedback.status+L"\r\n\r\n"+feedback.next).c_str(),feedback.title.c_str(),
                    MB_OK | (feedback.warning ? MB_ICONWARNING : MB_ICONINFORMATION));
            }
            break;
    }
}
