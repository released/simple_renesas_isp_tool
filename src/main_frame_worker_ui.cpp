#include "main_frame.h"

void CMainFrame::FlushWorkerLogBatch() {
    if(!worker_log_batch_.empty()) {
        firmware_upgrade::log_view::AppendBatch(log_edit_.GetSafeHwnd(),worker_log_batch_,log_auto_scroll_);
        worker_log_batch_.clear();
    }
    if(log_recording_ && log_record_fp_!=nullptr) fflush(log_record_fp_);
}

LRESULT CMainFrame::OnWorkerEvent(WPARAM,LPARAM) {
    // WM_APP only schedules work. A low-priority timer leaves paint/input time
    // even when detailed trace produces several events for each CAN block.
    SetTimer(kWorkerEventPumpTimerId,40U,nullptr);
    return 0;
}

void CMainFrame::PumpWorkerEvents() {
    const auto events=worker_session_.DrainEvents(64U);
    batching_worker_events_=true;
    for(const auto& event:events) {
        if(event.kind!=firmware_upgrade::ui::WorkerEventKind::Log) {
            FlushWorkerLogBatch();
            batching_worker_events_=false;
            ApplyWorkerEvent(event);
            batching_worker_events_=true;
        } else {
            ApplyWorkerEvent(event);
        }
    }
    batching_worker_events_=false;
    FlushWorkerLogBatch();
    if(!worker_session_.HasPendingEvents()) KillTimer(kWorkerEventPumpTimerId);
}

void CMainFrame::StopProgressActivity() {
    if(!progress_indeterminate_) return;
    progress_bar_.SetMarquee(FALSE,0U);
    progress_bar_.ModifyStyle(PBS_MARQUEE,0);
    progress_indeterminate_=false;
    progress_bar_.SetPos(0);
    progress_label_.SetWindowTextW((progress_operation_+L": stopped").c_str());
}
