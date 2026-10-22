"""Source integration guards; native tests execute the feedback model."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8-sig")


class UpdateCompletion(unittest.TestCase):
    def test_restart_wait_precedes_read_only_health_and_shares_budget(self):
        source = read("src/worker_can_workflows.cpp")
        confirm = source.split("bool WorkerSession::ConfirmDeviceApp", 1)[1]
        self.assertLess(confirm.index("AwaitAppProbe"), confirm.index("GetAppHealth"))
        self.assertIn("timing.ProbeFinished(elapsed())", confirm)
        self.assertIn("app_reset_quiet_ms", confirm)
        self.assertIn("app_health_probes", confirm)
        self.assertIn("DeviceRestartQuietMs(image.image_size,end_crc_ms)", confirm)
        self.assertIn("timing.QuietMs()", confirm)
        self.assertIn("ConfirmDeviceApp(metadata,report.end_ms,error)", source)
        self.assertIn("ConfirmApp(image,live,health)", confirm)
        for forbidden in ("Program(", "EnterBootloader(", "serial_port_.Close", "SetTransactionPolicy"):
            self.assertNotIn(forbidden, confirm)

    def test_counter_assessment_precedes_reports_and_completion(self):
        source = read("src/worker_session.cpp")
        report = source.split("if(report_update) {", 1)[1]
        self.assertLess(report.index("RefreshHostStatsBestEffort"), report.index("ObserveHostCounters"))
        self.assertLess(report.index("WithHostCounterWarning"), report.index("ReportResult"))
        self.assertLess(report.index("ReportResult"), report.index("SaveReportPair"))
        for field in ("host_counter_assessment", "host_counter_note", "host_deltas"):
            self.assertIn(field, report)
        self.assertIn("view.result=report_result", report)
        self.assertIn("completion.update_result=success ? update_result_", report)

    def test_can_stop_is_after_app_confirmation_and_retains_com(self):
        source = read("src/worker_can_workflows.cpp")
        finish = source.split("const bool confirmed=ConfirmDeviceApp(metadata,report.end_ms,error);", 1)[1]
        finish = finish.split("bool WorkerSession::ConfirmDeviceApp", 1)[0]
        self.assertIn("} else {\n        const bool stopped=StopCanAfterAppConfirmation();", finish)
        stop = finish.split("bool WorkerSession::StopCanAfterAppConfirmation()", 1)[1]
        self.assertIn("can_client_.Stop(&controller_state,&stop_error)", stop)
        self.assertLess(stop.index("if (!stopped)"), stop.index("state_.can_started=false"))
        self.assertIn("return false;", stop.split("state_.can_started=false", 1)[0])
        for forbidden in ("serial_port_.Close", "ProcessDisconnect", "RestoreLinkBaud"):
            self.assertNotIn(forbidden, stop)
        self.assertIn("state_.target_metadata={}", stop)
        self.assertIn("DeviceAppConfirmedCanActive", finish)

    def test_typed_success_event_and_reports(self):
        source = read("src/worker_session.cpp")
        self.assertIn("update_result_=feedback::Result::None;", source)
        self.assertIn("completion.update_result=success ? update_result_ : feedback::Result::None;", source)
        self.assertIn("evidence::JsonString(result_message)", source)
        self.assertIn("view.message=result_message", source)
        ui = read("src/main_frame.cpp")
        completion = ui.split("case WorkerEventKind::OperationComplete:", 1)[1].split("void CMainFrame::StartOperationElapsedTimer", 1)[0]
        self.assertIn("event.success && event.update_result!=", completion)
        self.assertIn("&& !closing_", completion)
        self.assertIn("MessageBoxW((feedback.status", completion)
        self.assertIn("SetProgress(100,feedback.progress)", completion)

    def test_persistent_result_does_not_enable_programming_or_survive_new_work(self):
        feedback = read("src/main_frame_evidence.cpp").split("void CMainFrame::UpdateCompletionFeedback()", 1)[1].split("void CMainFrame::OnForceOldFirmware", 1)[0]
        self.assertNotIn("EnableWindow", feedback)
        self.assertIn("session_state_.busy", feedback)
        self.assertIn("IsI2cTab()", feedback)
        self.assertIn("SelectedProfile()!=L\"CAN Upgrade\"", feedback)
        workflow = read("src/main_frame_workflow.cpp")
        self.assertIn("UpdateBusUi();\n    if (!boot_profile || image_blocker.empty()) UpdateCompletionFeedback();", workflow)
        self.assertIn("UpdateMiscUi(); UpdateCompletionFeedback();", workflow)
        for path in ("src/main_frame.cpp", "src/main_frame_image.cpp", "src/main_frame_bus.cpp"):
            self.assertIn("last_update_result_=firmware_upgrade::feedback::Result::None;", read(path))

    def test_enter_requires_discovered_app_and_misc_does_not_run_automatically(self):
        workflow = read("src/main_frame_workflow.cpp")
        enter = workflow.split("enter_bootloader_btn_.EnableWindow(", 1)[1].split("transfer_test_btn_", 1)[0]
        self.assertIn("can_started && update_target_ready && !session_state_.f13_bootloader_ready", enter)
        self.assertNotIn("!can_started && !target_ready", enter)
        self.assertIn("CanNextStep(host_ready,update_target_ready", workflow)
        bridge = read("src/worker_bridge_update.cpp")
        self.assertIn("update_result_=feedback::Result::HostAppRunning", bridge)
        program = bridge.split("if (command.type!=CommandType::BridgeProgram)", 1)[1]
        self.assertIn("update_result_=feedback::Result::HostAppWritten", program)
        self.assertNotIn("BU_RUN_COMMAND", program)

    def test_enter_wait_uses_installed_image_not_incoming_bin(self):
        source=read("src/worker_can_workflows.cpp")
        enter=source.split("bool WorkerSession::ProcessEnterF13Bootloader",1)[1].split("bool WorkerSession::ValidateF13BootTarget",1)[0]
        self.assertIn("installed_bytes=state_.target_metadata.installed_app_size",enter)
        self.assertIn("DeviceRestartQuietMs(installed_bytes,0U,true)",enter)
        self.assertNotIn("command.image",enter)
        self.assertLess(enter.index("EnterBootloader(error)"),enter.index("AwaitAppProbe"))
        self.assertLess(enter.index("AwaitAppProbe"),enter.index("ValidateF13BootTarget(error)"))
        self.assertIn("IsCancelRequested",enter)

    def test_reconnect_resets_progress_and_enter_action_is_emphasized(self):
        ui = read("src/main_frame.cpp")
        state = ui.split("case WorkerEventKind::State:", 1)[1].split("case WorkerEventKind::Progress:", 1)[0]
        self.assertIn("if (!session_state_.serial_open && event.state.serial_open)", state)
        self.assertIn('SetProgress(0,L"Progress");', state)
        workflow = read("src/main_frame_workflow.cpp")
        queued_open = workflow.split("bool CMainFrame::QueueOpenSelectedCom()", 1)[1].split("void CMainFrame::QueueDisconnect()", 1)[0]
        self.assertLess(queued_open.index("worker_session_.OpenAndProbe"), queued_open.index('SetProgress(0,L"Progress")'))
        self.assertIn("!busy && enter_bootloader_btn_.IsWindowEnabled()", workflow)
        self.assertIn("Next: Enter Boot. App mode detected", read("src/ui/update_feedback.h"))
        guide = read("docs/UPDATE_VALIDATION_zh-TW.md")
        self.assertIn("進度條與階段標籤歸零", guide)
        self.assertIn("粗體黑字 `Next: Enter Boot.", guide)

    def test_public_guide_explains_each_completion_and_both_discovery_modes(self):
        guide = read("docs/UPDATE_VALIDATION_zh-TW.md")
        model = read("src/ui/update_feedback.h")
        for title in ("Device App update complete", "Device App updated / Host counter warning", "Device App updated / CAN stop not confirmed",
                      "Host App update complete - RUN required", "Host App startup confirmed"):
            self.assertIn(title, guide)
            self.assertIn(title, model)
        for phrase in ('<a id="update-completion"></a>', "Discover 回報 Boot", "Discover 回報 App",
                       "APP_READY 與 live INFO 核對完成後才停止", "停止後清除當前 live target 快照"):
            self.assertIn(phrase, guide)


if __name__ == "__main__":
    unittest.main()
