// Compiled production worker and MFC failure paths; no COM handle is opened.
#include <afxcmn.h>
#include <afxwin.h>
#include <iostream>
#include "../src/main_frame.h"
#include "../src/resource.h"
#include "../src/core/application_instance.h"

CWinApp test_app;
namespace {
unsigned failures = 0U, cases = 0U, destructors = 0U;
void Check(bool ok, const char* name) {
    ++cases;
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
class FailedFrame : public CMainFrame {
public:
    explicit FailedFrame(bool before) : before_(before) {}
    ~FailedFrame() override { ++destructors; }
protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override {
        return before_ ? FALSE : CMainFrame::PreCreateWindow(cs);
    }
    afx_msg int OnCreate(LPCREATESTRUCT cs) {
        CMainFrame::OnCreate(cs);
        return -1;
    }
    DECLARE_MESSAGE_MAP()
private:
    bool before_;
};
BEGIN_MESSAGE_MAP(FailedFrame, CMainFrame)
    ON_WM_CREATE()
END_MESSAGE_MAP()

class CleanupWire : public firmware_upgrade::host_link::Transport {
public:
    bool fail = false, malformed = false;
    unsigned requests = 0U;
    std::vector<std::uint8_t> reply;
    bool IsOpen() const override { return true; }
    bool WriteAll(const std::uint8_t* bytes, std::size_t count, std::uint32_t, std::wstring*) override {
        using namespace firmware_upgrade::host_link;
        Frame input, output;
        if (DecodeFrame(bytes, count, &input) != DecodeResult::Ok) return false;
        ++requests;
        output.type = FrameType::Response;
        output.sequence = input.sequence;
        output.command = input.command;
        if (input.command == static_cast<std::uint8_t>(Command::GetHostInfo)) {
            output.payload = {1,1,1,7,0,2,2,0,1,0,0,0,0,0x41,0,0};
        } else if (input.command == static_cast<std::uint8_t>(Command::GetCapabilities)) {
            output.payload.assign(24U, 0U);
            output.payload[1] = 0x41U;
            output.payload[4] = 128U;
            output.payload[7] = 1U;
        } else {
            output.status = fail ? Status::Timeout : Status::Ok;
            if (malformed) output.payload = {1U};
        }
        return EncodeFrame(output, &reply);
    }
    bool ReadSome(std::vector<std::uint8_t>* out, std::size_t, std::uint32_t, std::wstring*) override {
        if (reply.empty()) return false;
        *out = std::move(reply);
        return true;
    }
};
}

struct WorkerResponsivenessFixture {
static int Run() {
    using namespace firmware_upgrade;
    if (!AfxWinInit(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), SW_HIDE) || !AfxInitRichEdit2()) return 2;
    for (bool before : {true, false}) {
        const auto count = destructors;
        auto* failed = new FailedFrame(before);
        Check(!failed->Create(nullptr, L"Owned startup failure", WS_OVERLAPPEDWINDOW, CRect(0,0,1366,728)), "actual MFC Create failure");
        Check(destructors == count + 1U, "MFC failure deletes the production-derived frame exactly once");
    }
    auto* frame = new CMainFrame();
    if (!frame->Create(nullptr, L"Owned follow-up fixture", WS_OVERLAPPEDWINDOW, CRect(0,0,1366,728))) return 3;
    test_app.m_pMainWnd = frame;
    frame->worker_session_.StopAndJoin();
    frame->worker_started_ = false;
    ui::WorkerSession session;
    std::wstring error;
    session.cancel_requested_ = true;
    ui::WorkerSession::Command reopen;
    reopen.type = ui::WorkerSession::CommandType::OpenAndProbe;
    Check(!session.ProcessOpenAndProbe(reopen, &error) && session.IsCancelRequested() &&
        error.find(L"canceled") != std::wstring::npos, "cancel before reconnect survives disconnect and prevents any COM open");
    try {
        ScopedFlag cleanup(session.cleanup_in_progress_);
        Check(!session.IsCancelRequested(), "cleanup can finish without consuming Cancel");
        { ScopedFlag nested(session.cleanup_in_progress_); }
        Check(session.cleanup_in_progress_, "nested cleanup preserves outer suppression");
        throw 1;
    } catch (int) {}
    Check(session.IsCancelRequested() && !session.cleanup_in_progress_, "exception restores cancel visibility");
    session.stopping_ = true;
    session.cancel_requested_ = false;
    Check(session.IsCancelRequested(), "shutdown remains canceled independently of the request latch");
    session.stopping_ = false;
    session.started_ = true;
    session.command_pending_or_busy_ = true;
    Check(session.RequestCancel(), "actual cancel request accepted");
    session.ProcessDisconnect();
    Check(session.IsCancelRequested(), "disconnect never consumes an accepted cancellation");
    session.command_pending_or_busy_ = false;
    Check(session.Submit(reopen) && !session.IsCancelRequested(), "next explicit operation resets the previous cancellation");
    session.started_ = false;
    session.commands_.clear();
    session.command_pending_or_busy_ = false;
    CleanupWire wire;
    session.host_link_client_.SetTransport(&wire);
    Check(session.host_link_client_.Probe(0U, nullptr, nullptr, &error), "actual Host Link client uses a synthetic identified host");
    const auto wait_completion = [&](ui::WorkerEvent* terminal) {
        for (unsigned attempt = 0U; attempt < 200U; ++attempt) {
            for (auto& event : session.DrainEvents()) {
                if (event.kind == ui::WorkerEventKind::OperationComplete) {
                    *terminal = event;
                    return true;
                }
            }
            Sleep(5U);
        }
        return false;
    };
    session.state_.uart_cleanup_unconfirmed = true;
    const auto before_requests = wire.requests;
    Check(session.Start(frame->GetSafeHwnd(), WM_APP + 1U) && session.ResetHost(), "actual worker accepts a queued command for state-gate verification");
    ui::WorkerEvent terminal;
    Check(wait_completion(&terminal) && !terminal.success && terminal.text.find(L"cleanup is not confirmed") != std::wstring::npos &&
        wire.requests == before_requests, "actual dispatcher blocks unknown reservation before any Host command");
    session.StopAndJoin();
    Check(session.Start(frame->GetSafeHwnd(), WM_APP + 1U), "worker restarts with clean ownership");
    {
        std::lock_guard<std::mutex> lock(session.command_mutex_);
        session.commands_.push_back(reopen);
        session.command_pending_or_busy_ = true;
        session.cancel_requested_ = true;
    }
    session.command_ready_.notify_one();
    Check(wait_completion(&terminal) && !terminal.success && terminal.text.find(L"canceled") != std::wstring::npos &&
        wire.requests == before_requests, "cancel latched before real dispatch survives reconnect without COM/probe activity");
    session.StopAndJoin();
    for (bool uart : {true, false}) {
        for (unsigned fault = 0U; fault < 3U; ++fault) {
            ScopedFlag cleanup(session.cleanup_in_progress_);
            session.state_ = {};
            wire.fail = fault == 1U;
            wire.malformed = fault == 2U;
            const bool ack = uart ? session.uart_client_.Abort(&error) : session.i2c_client_.Abort(&error);
            session.RecordBusCleanup(uart, ack, error);
            Check(session.state_.SerialCleanupUnconfirmed() == (fault != 0U), "returned/malformed ABORT propagates actual client result to worker state");
            if (!ack) {
                session.RecordBusCleanup(uart, true, L"");
                Check(session.state_.SerialCleanupUnconfirmed(), "late cleanup ACK does not silently unlock an uncertain session");
                frame->session_state_ = session.state_;
                frame->session_state_.host_validated = true;
                frame->session_state_.serial_open = true;
                for (int tab = 0; tab < 4; ++tab) {
                    frame->bus_tabs_.SetCurSel(tab);
                    frame->UpdateUi();
                    CString text;
                    frame->firmware_prerequisites_label_.GetWindowTextW(text);
                    Check(!frame->bus_tabs_.IsWindowEnabled() && !frame->start_firmware_update_btn_.IsWindowEnabled() &&
                        !frame->connect_target_btn_.IsWindowEnabled() && text.Find(L"Close COM") >= 0,
                        "actual CAN/I2C/UART/Misc UI blocks unconfirmed cleanup and explains recovery");
                }
                session.ResetSessionState();
                Check(!session.state_.SerialCleanupUnconfirmed(), "new session does not inherit an old reservation");
            }
        }
    }
    session.state_ = {};
    frame->session_state_ = {};
    frame->bus_tabs_.SetCurSel(0);
    ui::WorkerEvent report;
    report.report_pending = true;
    report.report_operation_id = 1U;
    frame->SaveUpdateReport(report);
    report.report_pending = false;
    report.report_json_path = L"prior.json";
    report.report_html_path = L"prior.html";
    frame->SaveUpdateReport(report);
    report.report_operation_id = 2U;
    report.report_pending = true;
    frame->SaveUpdateReport(report);
    report.report_pending = false;
    report.report_json_path.clear(); report.report_html_path.clear(); report.text = L"HTML access denied";
    frame->SaveUpdateReport(report);
    CString caption;
    frame->open_report_btn_.GetWindowTextW(caption);
    Check(caption == L"Open Previous Report" && frame->report_status_.operation == 2U &&
        frame->report_status_.error == L"HTML access denied", "actual UI identifies the previous report after a current save failure");
    frame->closing_ = true;
    ui::WorkerEvent complete;
    complete.kind = ui::WorkerEventKind::OperationComplete;
    complete.success = true;
    complete.report_operation_id = 2U;
    complete.update_result = feedback::Result::DeviceAppConfirmed;
    frame->ApplyWorkerEvent(complete);
    Check(frame->last_operation_error_.empty() && frame->last_update_result_ == feedback::Result::DeviceAppConfirmed,
        "report failure does not turn confirmed firmware success into an update failure");
    frame->closing_ = false;
    const auto activation = frame->SendMessage(application_instance::kActivateMessage);
    Check(activation == 1 && frame->report_status_.operation == 2U, "production activation handler preserves current workflow");
    CDialog popup;
    Check(popup.Create(IDD_METADATA_DIFFERENCE, frame) != FALSE, "native owned dialog created");
    popup.ShowWindow(SW_SHOWNOACTIVATE);
    frame->EnableWindow(FALSE);
    popup.SetActiveWindow();
    frame->SendMessage(WM_APP + 2U);
    Check(popup.GetSafeHwnd() && popup.IsWindowVisible() && !frame->IsWindowEnabled() &&
        frame->report_status_.operation == 2U,
        "deferred activation preserves the owned popup, disabled owner and operation state");
    popup.DestroyWindow();
    frame->EnableWindow(TRUE);
    frame->DestroyWindow();
    test_app.m_pMainWnd = nullptr;
    std::cout << "Follow-up MFC: " << cases << " checks, " << failures << " failures; no hardware\n";
    return failures ? 1 : 0;
}
};
int wmain() { return WorkerResponsivenessFixture::Run(); }
