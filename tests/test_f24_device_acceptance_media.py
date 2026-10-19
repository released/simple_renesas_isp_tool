"""Normal F24 update acceptance and publication copies; no board operations."""
from pathlib import Path
import hashlib
import subprocess
import tempfile
import unittest
from PIL import Image
from pypdf import PdfReader
import pdfplumber
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
WEB = companion_path('Web')


class F24DeviceAcceptanceMedia(unittest.TestCase):
    def test_two_public_pngs_and_actual_state_captions(self):
        text = (ROOT / 'docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        section = text.split('<a id="f24-device-update-example"></a>', 1)[1].split('<!-- TOOL_DEVICE_MEDIA_END -->', 1)[0]
        for phrase in ('已確認可完整執行正常 CAN App 更新', 'APP_READY', 'Target before operation',
                       'log(timer)-V1', '250K／1M', '斷電復原'):
            self.assertIn(phrase, section)
        for name, size in (('MCU_F24_Device_app_log', (1560, 624)), ('firmware_update_report', (1907, 873))):
            relative = 'images/tool_operation/' + name + '.png'
            local = ROOT / 'docs' / relative
            public = WEB / 'slide_isp_tool_for_RL78' / relative
            self.assertEqual(local.read_bytes(), public.read_bytes())
            self.assertIn('](' + relative + ')', section)
            with Image.open(local) as image:
                self.assertEqual(image.format, 'PNG')
                self.assertEqual(image.size, size)
                self.assertFalse(image.getexif())
                self.assertFalse(image.info)
            ignored = subprocess.run(['git', 'check-ignore', '--', name + '.jpg'], cwd=ROOT, capture_output=True)
            self.assertEqual(ignored.returncode, 0)
        other = (WEB / 'slide_boot_loader_for_RL78/index.md').read_text(encoding='utf-8-sig')
        self.assertNotIn('<a id="f24-device-update-example"></a>', other)
        self.assertIn('#f24-device-update-example', other)

    def test_current_status_matrix_and_all_public_pdfs(self):
        design = (ROOT / 'docs/F24_CAN_DEVICE_DESIGN_zh-TW.md').read_text(encoding='utf-8-sig')
        self.assertIn('正常 CAN App 更新已完成實板驗證', design)
        self.assertIn('UART timer counter 已確認', design)
        matrix = (ROOT / 'docs/UPDATE_VALIDATION_zh-TW.md').read_text(encoding='utf-8-sig')
        row = next(line for line in matrix.splitlines() if line.startswith('| 正常 CAN device App 更新 |'))
        self.assertIn('F13／F24', row)
        self.assertTrue(row.endswith('| PASS |'))
        for topic in ('| 拔線／重連 |', '| CAN 故障 |', '| Device 斷電 |'):
            self.assertIn('NOT RUN', next(line for line in matrix.splitlines() if line.startswith(topic)))
        texts = {}
        for path in sorted((ROOT / 'docs').glob('*.pdf')):
            texts[path.name] = '\n'.join(page.extract_text() or '' for page in PdfReader(path).pages)
            self.assertNotRegex(texts[path.name], r'F24\s*device.{0,60}(?:尚未完成實板|hardware acceptance is still required)')
        for name in ('FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf',
                     'UPDATE_VALIDATION_zh-TW.pdf', 'DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.pdf',
                     'FirmwareUpgradeTool_UI_Identity_Guide_zh-TW.pdf'):
            self.assertIn('實板驗證', texts[name])
        self.assertNotIn('native pipe', texts['FirmwareUpgradeTool_UI_Identity_Guide_zh-TW.pdf'])
        reader = PdfReader(ROOT / 'docs/FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf')
        self.assertEqual(sum(len(page.images) for page in reader.pages), 20)
        sizes = {(image.image.width, image.image.height) for page in reader.pages for image in page.images}
        self.assertTrue({(1560, 624), (1907, 873)} <= sizes)

    def test_png_hash_mismatch_fails_without_writes(self):
        with tempfile.TemporaryDirectory(prefix='f24_device_media_') as temporary:
            folder = Path(temporary)
            page = folder / 'slide_isp_tool_for_RL78/index.md'
            page.parent.mkdir()
            page.write_text('\n'.join('<!-- ' + name + '_BEGIN -->stale<!-- ' + name + '_END -->' for name in
                            ('TOOL_CONNECT_MEDIA', 'TOOL_DEVICE_MEDIA', 'TOOL_BRIDGE_MEDIA')), encoding='utf-8')
            args = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    str(ROOT / 'scripts/sync_tool_operation_media.ps1'), '-WebRoot', str(folder)]
            self.assertEqual(subprocess.run(args, capture_output=True).returncode, 0)
            image = page.parent / 'images/tool_operation/firmware_update_report.png'
            image.write_bytes(image.read_bytes() + b'test mismatch')
            before = {str(p): hashlib.sha256(p.read_bytes()).digest() for p in page.parent.rglob('*') if p.is_file()}
            self.assertNotEqual(subprocess.run(args + ['-CheckOnly'], capture_output=True).returncode, 0)
            after = {str(p): hashlib.sha256(p.read_bytes()).digest() for p in page.parent.rglob('*') if p.is_file()}
            self.assertEqual(before, after)

    def test_refreshed_pdf_text_stays_within_page_bounds(self):
        for name in ('FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf',
                     'UPDATE_VALIDATION_zh-TW.pdf', 'DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.pdf',
                     'FirmwareUpgradeTool_UI_Identity_Guide_zh-TW.pdf',
                     'FirmwareUpgradeTool_MultiBus_Guide_zh-TW.pdf'):
            with pdfplumber.open(ROOT / 'docs' / name) as pdf:
                for number, page in enumerate(pdf.pages, 1):
                    self.assertTrue(page.extract_text().strip(), (name, number))
                    for char in page.chars:
                        if char['text'].strip():
                            self.assertGreaterEqual(char['x0'], -0.5, (name, number, char))
                            self.assertLessEqual(char['x1'], page.width + 0.5, (name, number, char))
                            self.assertGreaterEqual(char['top'], -0.5, (name, number, char))
                            self.assertLessEqual(char['bottom'], page.height + 0.5, (name, number, char))


if __name__ == '__main__':
    unittest.main(verbosity=2)
