"""Public F24 Transfer screenshots, single-owner text and safe sync checks."""
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from PIL import Image
from pypdf import PdfReader
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
WEB = companion_path('Web')
SOURCE = ROOT / 'docs/MULTIBUS_TOOL_GUIDE_zh-TW.md'
PATTERN = r'<!-- CUSTOM_TRANSFER_MEDIA_BEGIN -->.*?<!-- CUSTOM_TRANSFER_MEDIA_END -->'


class CustomTransferMedia(unittest.TestCase):
    def block(self):
        blocks = re.findall(PATTERN, SOURCE.read_text(encoding='utf-8-sig'), re.S)
        self.assertEqual(len(blocks), 1)
        return blocks[0]

    def test_four_public_images_and_single_owner(self):
        block = self.block()
        page = (WEB / 'slide_isp_tool_for_RL78/index.md').read_text(encoding='utf-8-sig')
        other = (WEB / 'slide_boot_loader_for_RL78/index.md').read_text(encoding='utf-8-sig')
        self.assertEqual(re.findall(PATTERN, page, re.S), [block])
        self.assertNotIn('CUSTOM_TRANSFER_MEDIA_BEGIN', other)
        self.assertLess(page.index('id="custom-transfer"'), page.index(block))
        self.assertLess(page.index(block), page.index('id="mcu-can-device"'))
        images = re.findall(r'!\[[^\]]*\]\((images/custom_transfer/[^)]+)\)', block)
        self.assertEqual(len(images), 4)
        self.assertEqual(len(set(images)), 4)
        for relative in images:
            local = ROOT / 'docs' / relative
            mirror = WEB / 'slide_isp_tool_for_RL78' / relative
            self.assertEqual(hashlib.sha256(local.read_bytes()).digest(),
                             hashlib.sha256(mirror.read_bytes()).digest())
            self.assertIn(']('+relative+')', block)
            with Image.open(local) as image:
                self.assertEqual(image.format, 'PNG')
                self.assertFalse(image.getexif())
                self.assertGreater(image.width, 1500)
                if '_mcu_log_' not in local.name:
                    self.assertEqual(image.getpixel((716, 460)), (227, 227, 227))
        for term in ('OFFLINE', 'DEVICE TARGET', '0xF24B', 'crc=OK',
                     'CAN RX=22, TX=22', '**MCU host**', '**MCU device**',
                     '不包含 CAN 波形量測', 'Boot／App 擦寫驗證'):
            self.assertIn(term, block)
        self.assertIn('/CAN_custom_transfer*.jpg', (ROOT / '.gitignore').read_text())

    def test_sync_is_read_only_on_failure_and_checks_asset_hashes(self):
        with tempfile.TemporaryDirectory(prefix='custom_transfer_media_') as folder:
            page = Path(folder) / 'slide_isp_tool_for_RL78/index.md'
            page.parent.mkdir()
            stale = '<!-- CUSTOM_TRANSFER_MEDIA_BEGIN -->stale<!-- CUSTOM_TRANSFER_MEDIA_END -->'
            page.write_text(stale, encoding='utf-8')
            args = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    str(ROOT / 'scripts/sync_custom_transfer_media.ps1'), '-WebRoot', folder]
            self.assertNotEqual(subprocess.run(args + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertEqual(page.read_text(encoding='utf-8'), stale)
            self.assertFalse((page.parent / 'images').exists())
            page.write_text('missing marker', encoding='utf-8')
            self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
            self.assertEqual(page.read_text(encoding='utf-8'), 'missing marker')
            self.assertFalse((page.parent / 'images').exists())
            page.write_text(stale, encoding='utf-8')
            self.assertEqual(subprocess.run(args, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(args + ['-CheckOnly'], capture_output=True).returncode, 0)
            asset = next((page.parent / 'images/custom_transfer').glob('*.png'))
            asset.write_bytes(asset.read_bytes() + b'deliberate mismatch')
            before = {p: p.read_bytes() for p in page.parent.rglob('*') if p.is_file()}
            self.assertNotEqual(subprocess.run(args + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertEqual({p: p.read_bytes() for p in page.parent.rglob('*') if p.is_file()}, before)

    def test_pdf_embeds_all_four_captures_and_keeps_scope_text(self):
        reader = PdfReader(ROOT / 'docs/FirmwareUpgradeTool_MultiBus_Guide_zh-TW.pdf')
        sizes = set()
        for page in reader.pages:
            resources = page.get('/Resources', {})
            for reference in resources.get('/XObject', {}).get_object().values() if resources.get('/XObject') else []:
                obj = reference.get_object()
                if obj.get('/Subtype') == '/Image':
                    sizes.add((int(obj['/Width']), int(obj['/Height'])))
        self.assertTrue({(1916, 1018), (1917, 1020), (1561, 628), (1560, 626)} <= sizes)
        text = '\n'.join(page.extract_text() or '' for page in reader.pages)
        for term in ('0xF24B', 'OFFLINE', 'DEVICE TARGET', 'CAN RX=22, TX=22'):
            self.assertIn(term, text)


if __name__ == '__main__':
    unittest.main()
