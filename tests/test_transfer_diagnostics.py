"""Transfer source/doc contracts; native SFR/protocol models live with F24."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
BOOT = companion_path('F13Boot').parent / 'RL78_F24_Boot_loader_CAN'
TRANSFER = BOOT.parent / 'RL78_F24_CAN_Transfer'


class TransferDiagnostics(unittest.TestCase):
    def test_transfer_only_backend_and_main_loop_capture(self):
        source = (BOOT / 'common/device_can.c').read_text(encoding='utf-8-sig')
        block = source.split('#ifdef F24_DEVICE_TRANSFER', 1)[1].split('#else', 1)[0]
        self.assertIn('Device_CanPollErrors', block)
        self.assertIn('C0ERFLL & 0x7FFFU', block)
        self.assertIn('~flags & 0x7FFFU', block)
        self.assertNotIn('R_CAN_GetChBusErrFlag()', block)
        main = (TRANSFER / 'transfer_main.c').read_text(encoding='utf-8-sig')
        loop = main.split('for (;;) {', 1)[1]
        self.assertLess(loop.index('Device_CanPollErrors()'), loop.index('CAN_TransferProtocol_Process()'))
        self.assertIn('CAN_TransferProtocol_OnChannelError(errors)', loop)
        for label in ('clock status:', 'CAN init:', 'stage=', 'global_status=',
                      'channel_status=', 'filter=', 'diagnostics.standby'):
            self.assertIn(label, main)
        header = (TRANSFER / 'transfer_diagnostics.h').read_text(encoding='utf-8-sig')
        stages = re.findall(r'TRANSFER_CAN_STAGE_\w+\s+\((\d)U\)', header)
        self.assertEqual(stages, list('12345'))

    def test_protocol_driver_boundary_preserves_wire_and_commands(self):
        f13 = (BOOT.parent / 'RL78_F13_CAN_Transfer/can_transfer_protocol.c').read_text(encoding='utf-8-sig')
        f24 = (TRANSFER / 'can_transfer_protocol.c').read_text(encoding='utf-8-sig')
        for name, old, new in (
            ('include', '#include "inc_main.h"', '#include "platform.h"'),
            ('tick', '(60000U)', '(65536UL)'),
            ('tx', 'R_CAN_TrmByTxBuf(CAN_CH0, CAN_TXBUF0, &frame)', 'Device_CanStartTx(&frame)'),
            ('poll', 'R_CAN_CheckTxBufResult(CAN_CH0, CAN_TXBUF0)', 'Device_CanTxResult()'),
            ('abort', 'R_CAN_AbortTrm(CAN_CH0, CAN_TXBUF0)', 'Device_CanAbortTx()'),
            ('rx', 'R_CAN_ReadRxFIFO(CAN_RXFIFO0, &request)', 'Device_CanRead(&request)'),
        ):
            self.assertIn(old, f13, name)
            f13 = f13.replace(old, new)
        pattern = r'static uint8_t build_device_info\(.*?\nstatic uint8_t dispatch_request'
        # Identity is MCU-local. Everything outside that handler matches.
        self.assertEqual(re.sub(pattern, 'IDENTITY', f13, flags=re.S),
                         re.sub(pattern, 'IDENTITY', f24, flags=re.S))

    def test_sync_and_check_only_drift_preserve_source(self):
        block = re.search(r'<!-- TRANSFER_DIAGNOSTICS_BEGIN -->.*?<!-- TRANSFER_DIAGNOSTICS_END -->',
                          (ROOT / 'docs/MULTIBUS_TOOL_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig'), re.S).group()
        with tempfile.TemporaryDirectory() as temporary:
            page = Path(temporary) / 'slide_isp_tool_for_RL78/index.md'
            page.parent.mkdir()
            page.write_text('before\n<!-- TRANSFER_DIAGNOSTICS_BEGIN -->\nstale\n<!-- TRANSFER_DIAGNOSTICS_END -->\nafter', encoding='utf-8')
            args = ['powershell.exe', '-NoProfile', '-File', str(ROOT / 'scripts/sync_transfer_diagnostics.ps1'),
                    '-WebRoot', temporary]
            before = page.read_bytes()
            self.assertNotEqual(subprocess.run(args + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertEqual(page.read_bytes(), before)
            self.assertEqual(subprocess.run(args, capture_output=True).returncode, 0)
            self.assertEqual(page.read_text(encoding='utf-8-sig'), 'before\n' + block + '\nafter')
            self.assertEqual(subprocess.run(args + ['-CheckOnly'], capture_output=True).returncode, 0)
            page.write_text('missing marker', encoding='utf-8')
            before = page.read_bytes()
            self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
            self.assertEqual(page.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
