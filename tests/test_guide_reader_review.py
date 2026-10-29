"""Read-only semantic guards for user-facing guide commands and format examples."""
import binascii
import os
from pathlib import Path
import re
import struct
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
WEB = Path(os.environ['FIRMWARE_WEB_ROOT']) if os.environ.get('FIRMWARE_WEB_ROOT') else None
MCU = companion_path('F13Boot').parent


@unittest.skipUnless(WEB, 'Set FIRMWARE_WEB_ROOT to the local Web workspace')
class GuideReaderReviewTests(unittest.TestCase):
    def doc(self, name):
        return (ROOT/'docs'/name).read_text(encoding='utf-8-sig')

    def test_app_commands_match_local_project_hooks(self):
        text = self.doc('DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md')
        build = text.split('### D. 產生與建置')[1].split('#### D3.')[0]
        self.assertNotIn('..\\RL78_', build)
        self.assertNotIn('-AppProject', build.split('# Rebuild this App')[1].split('```')[0])
        for mcu, hook in [('F13', 'generate_irq_contract.ps1'), ('F24', 'tools\\prepare_projects.ps1')]:
            project = MCU/f'RL78_{mcu}_Boot_loader_CAN_app'
            self.assertTrue((project/'build.ps1').is_file())
            self.assertTrue((project/hook).is_file())
            mtpj = next(project.glob('*.mtpj')).read_text(encoding='utf-8-sig')
            self.assertIn(hook, mtpj)
            self.assertNotIn('..\\RL78_', mtpj)
        self.assertIn('-File .\\build.ps1', build)
        self.assertIn('-CheckOnly', build)
        self.assertIn('-ArtifactsOnly', build)

    def test_publish_prose_matches_nine_repo_batch_scope(self):
        text = self.doc('MULTIBUS_TOOL_GUIDE_zh-TW.md')
        batch = (ROOT/'github_push.bat').read_text(encoding='utf-8-sig')
        self.assertIn('九個 repository', text)
        self.assertIn('Tool／Host、RH850、串列工程與網站', text)
        self.assertIn('Nine repositories only', batch)
        self.assertNotIn('App會引用相鄰Boot', text)

    def test_serial_request_reply_examples_have_independent_valid_crc(self):
        source = self.doc('ISP_SERIAL_TRANSPORT_GUIDE_zh-TW.md')
        samples = dict(re.findall(r'^(PING request|PING reply|Unknown request|Error reply):\s+([0-9A-F ]+)$', source, re.M))
        self.assertEqual(len(samples), 4)
        frames = {k: bytes.fromhex(v) for k, v in samples.items()}
        for frame in frames.values():
            self.assertEqual(frame[:3], b'IS\x01')
            self.assertEqual(len(frame), 14+struct.unpack_from('<H', frame, 8)[0])
            self.assertEqual(binascii.crc32(frame[:-4]), int.from_bytes(frame[-4:], 'little'))
        request, reply = frames['PING request'], frames['PING reply']
        self.assertEqual(request[4:7], reply[4:7])
        self.assertEqual(request[10:-4], reply[10:-4])
        self.assertEqual(reply[3:4]+reply[7:8], b'\x01\x00')
        self.assertEqual(frames['Error reply'][7], 1)
        self.assertIn('以Transfer角色為例', source)
        self.assertIn('A5`只表示已有完整回覆', source)

    def test_image_adapter_table_distinguishes_isp_from_reference(self):
        source = self.doc('UPDATE_VALIDATION_zh-TW.md')
        block = source.split('#### 更新介面與映像格式')[1].split('#### 常見組合')[0]
        for row in ['CAN device 更新', 'UART ISP／Xmodem 更新', 'I2C ISP 更新', 'Misc Host App 更新', 'I2C reference']:
            self.assertIn(row, block)
        self.assertIn('format6／CRC32', block)
        self.assertIn('CRC16/XMODEM', block)
        self.assertLess(source.index('### 完成提示與下一步'), source.index('### 重啟等待與 App 確認'))
        page = (WEB/'slide_simple_renesas_isp_tool/serial-isp.md').read_text(encoding='utf-8')
        self.assertLess(page.index('id="serial-device-operation"'), page.index('id="4-i2c-master"'))

    def test_paired_reader_paths_and_platform_scope(self):
        for platform in ['RL78', 'RH850']:
            root = WEB/('slide_boot_loader_for_'+platform)
            home = (root/'index.md').read_text(encoding='utf-8')
            for target in ['guide-app-development-2', 'guide-app-development-3', 'guide-app-development-4', 'guide-porting-2']:
                self.assertIn(target, home)
            app = (root/'app-development.md').read_text(encoding='utf-8')
            self.assertIn('#### C0. Boot／App 中斷入口與交接', app)
            self.assertIn('#### 依修改目的選擇設定入口', (root/'porting.md').read_text(encoding='utf-8'))
        isp = (WEB/'slide_simple_renesas_isp_tool/index.md').read_text(encoding='utf-8')
        self.assertIn('F13／F24／RH850', isp)
        self.assertIn('F24／RH850', isp)
        self.assertIn('slide_boot_loader_for_RH850/', isp)
        walk = self.doc('CAN_UPDATE_WALKTHROUGH_zh-TW.md')
        self.assertIn('**F13 Classical CAN**', walk)
        self.assertIn('RH850 使用256-byte Flash page buffer', walk)


if __name__ == '__main__':
    unittest.main(verbosity=2)
