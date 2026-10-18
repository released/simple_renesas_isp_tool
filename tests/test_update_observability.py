"""Read-only cross-layer checks; native C/C++ tests execute protocol helpers."""
from pathlib import Path
import json
import re
import subprocess
import unittest
from workspace_paths import companion_path

ROOT=Path(__file__).resolve().parents[1]

class UpdateObservability(unittest.TestCase):
    def test_cancel_keeps_worker_transport_ownership(self):
        text=(ROOT/'src/worker_session.cpp').read_text(encoding='utf-8-sig')
        cancel=text.split('bool WorkerSession::RequestCancel()')[1].split('bool WorkerSession::VerifyInstalled()')[0]
        self.assertIn('cancel_requested_=true',cancel)
        self.assertNotIn('Close(',cancel)
        self.assertNotIn('Disconnect',cancel)
        self.assertIn('if (commands_.empty()) cancel_requested_=false;',text)
        workflow=(ROOT/'src/worker_can_workflows.cpp').read_text(encoding='utf-8-sig')
        self.assertIn('callbacks.abort=',workflow)
        self.assertIn('cleanup_in_progress_=true',workflow)

    def test_explicit_misc_run_and_no_blind_rewrite(self):
        text=(ROOT/'src/worker_bridge_update.cpp').read_text(encoding='utf-8-sig')
        run=text.split('if (command.type==CommandType::BridgeRun)')[1].split('if (command.type!=CommandType::BridgeProgram)')[0]
        self.assertEqual(run.count('RequestWithPolicy('),1)
        self.assertIn('request,1000U,1U',run)
        self.assertIn('reopen.port_name=state_.port_name',run)
        self.assertIn('reopen.baudrate=115200U',run)
        self.assertIn('ConfirmBridgeApp(expected,live)',run)
        self.assertNotIn('bridge_update::Program',run)

    def test_public_one_owner_and_hardware_boundary(self):
        source=(ROOT/'docs/UPDATE_VALIDATION_zh-TW.md').read_text(encoding='utf-8-sig')
        self.assertEqual(source.count('```mermaid'),1)
        for phrase in ('APP_READY','Compare Metadata','Cancel','phases_ms','NOT RUN','921600','460800','Verify Installed'):
            self.assertIn(phrase,source)
        self.assertNotRegex(source,r'[A-Z]:[\\/]|_Avery|SourceCode')
        web=companion_path('Web')
        pattern=r'<!-- UPDATE_EVIDENCE_BEGIN -->.*?<!-- UPDATE_EVIDENCE_END -->'
        owner=(web/'slide_isp_tool_for_RL78/index.md').read_text(encoding='utf-8-sig')
        other=(web/'slide_boot_loader_for_RL78/index.md').read_text(encoding='utf-8-sig')
        self.assertEqual(re.findall(pattern,source,re.S),re.findall(pattern,owner,re.S))
        self.assertNotIn('phases_ms',re.findall(pattern,other,re.S)[0])
        catalog=json.loads((ROOT/'scripts/web_guide_sections.json').read_text(encoding='utf-8-sig'))
        self.assertEqual(catalog['UPDATE_EVIDENCE']['owner'],'slide_isp_tool_for_RL78')

    def test_f24_delivery_names(self):
        boot=companion_path('F13Boot')
        app=boot.parent/'RL78_F24_Boot_loader_CAN_app'
        for name in ('ftable.inc','app_identity.h','app_irq_table.inc','ftable_RL78_F24.asm',
                     'DefaultBuild/flash_app_variable_CRC.hex','boot_app.hex'):
            self.assertTrue((app/name).is_file(),name)

    def test_f13_boot_budget_when_map_available(self):
        boot=companion_path('F13Boot')
        if not (boot/'DefaultBuild/RL78_F13_Boot_loader_CAN.map').is_file():
            self.skipTest('F13 map not present in delivery-only checkout; firmware clean-build budget gate NOT RUN')
        result=subprocess.run(['powershell.exe','-NoProfile','-File',str(boot/'verify_boot_budget.ps1')],capture_output=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)

if __name__=='__main__':
    unittest.main()
