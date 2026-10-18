"""Read-only ownership guards; native tests execute production helpers."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
def read(name): return (ROOT/name).read_text(encoding='utf-8-sig')

class ToolExperience(unittest.TestCase):
    def test_reporting_stays_off_ui_thread(self):
        worker=read('src/worker_session.cpp')
        ui=read('src/main_frame_evidence.cpp')
        self.assertIn('SaveReportPair(document,evidence::RenderReport(view))',worker)
        self.assertNotIn('WriteReport(',ui)
        self.assertNotIn('SaveReportPair(',ui)
        self.assertIn('case WorkerEventKind::UpdateReport:',read('src/main_frame.cpp'))
        self.assertIn('Report generation failed',worker)

    def test_write_is_exclusive_and_published_after_flush(self):
        text=read('src/core/update_report_store.h')
        self.assertIn('CREATE_NEW',text)
        self.assertNotIn('CREATE_ALWAYS',text)
        self.assertNotIn('MOVEFILE_REPLACE_EXISTING',text)
        self.assertLess(text.index('FlushFileBuffers'),text.index('MoveFileExW'))
        self.assertIn('std::atomic<unsigned long long>',text)
        self.assertIn('saved.error',text)

    def test_pnp_is_hint_not_probe_or_reconnect(self):
        text=read('src/main_frame.cpp')
        hotplug=text.split('BOOL CMainFrame::OnDeviceChange(')[1].split('\n}',1)[0]
        for action in ('OpenAndProbe(', 'QueueOpenSelectedCom(', 'Close(', 'QueueDisconnect('):
            self.assertNotIn(action,hotplug)
        self.assertIn('SetTimer(kComRefreshTimerId',hotplug)
        timer=text.split('void CMainFrame::OnTimer(')[1].split('BOOL CMainFrame::OnDeviceChange')[0]
        self.assertIn('session_state_.serial_open',timer)
        self.assertIn('selected<0 && preferred.empty()?0:selected',text)
        self.assertIn('SetupDiGetDeviceInstanceIdW',read('src/core/serial_port_catalog.cpp'))

    def test_preflight_only_reads_snapshot(self):
        text=read('src/main_frame_evidence.cpp').split('void CMainFrame::OnPreflight()')[1]
        for action in ('OpenAndProbe(', 'BridgeAction(', 'ProgramF13Boot(', 'WriteReport(', 'Request('):
            self.assertNotIn(action,text)
        for label in ('Live host','Image / CRC32','Device Boot','Installed version policy','Flash backend'):
            self.assertIn(label,text)
        self.assertIn('evidence::Compare(',text)
        self.assertIn('worker rechecks',read('src/core/preflight_summary.h'))

    def test_single_console_and_no_editor(self):
        text=read('src/main_frame.cpp')
        self.assertIn('OnOpenLatestReport',text)
        self.assertIn('OnErrorDetails',text)
        self.assertNotIn('Metadata Editor',text)
        layout=read('src/main_frame_layout.cpp')
        self.assertIn('CalculateLog(log,row)',layout)
        self.assertIn('RDW_ALLCHILDREN',layout)

if __name__=='__main__': unittest.main()
