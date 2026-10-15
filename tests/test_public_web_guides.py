"""Keep public navigation linked and independent of delivery milestones."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT.parents[1] / '_Avery_Markdown/web'
ISP = WEB / 'slide_isp_tool_for_RL78'
BOOT = WEB / 'slide_boot_loader_for_RL78'
NAV = WEB / 'released.github.io/README.md'


class PublicWebGuides(unittest.TestCase):
    def test_navigation_has_canonical_links_and_function_descriptions(self):
        text = NAV.read_text(encoding='utf-8-sig')
        entries = re.findall(r'(?m)^\| \[RL78 (?:ISP Tool|Bootloader)\].*$', text)
        self.assertEqual(len(entries), 2)
        self.assertIn('[RL78 ISP Tool](https://released.github.io/slide_isp_tool_for_RL78/', text)
        self.assertIn('[RL78 Bootloader](https://released.github.io/slide_boot_loader_for_RL78/', text)
        for entry in entries:
            self.assertNotRegex(entry, r'\d+\.\d+\.\d+|Tool\d{4}|工程第一版|19244|10963|301區塊')
        self.assertIn('I2C 探索', entries[-1])
        self.assertTrue((ISP / 'index.html').is_file())

    def test_public_pages_have_no_draft_or_delivery_pins(self):
        for path in [NAV, ISP / 'index.md', BOOT / 'index.md',
                     ISP / 'README.md', BOOT / 'README.md']:
            text = path.read_text(encoding='utf-8-sig')
            self.assertNotRegex(text, r'尚未發布|未發布|尚未公開發布|本機稿|本機編輯稿|本機製作稿|工程第一版')
            self.assertNotRegex(text, r'0\.1\.0\.\d+|Tool00\d\d|1\.5\.0|19244|10963')
            self.assertEqual(len(re.findall(r'(?m)^```', text)) % 2, 0)

    def test_technical_contracts_and_unverified_boundaries_remain(self):
        text = (ISP / 'index.md').read_text(encoding='utf-8-sig')
        for phrase in ['最低boot版本', 'image format6', 'contract3', 'F1V1', 'F2V1',
                       'G16', '完整故障矩陣', 'CRC提供完整性', '尚未開放']:
            self.assertIn(phrase, text)

    def test_verifier_accepts_future_product_version_without_page_update(self):
        with tempfile.TemporaryDirectory(prefix='isp_public_guide_') as folder:
            source = Path(folder) / 'src'
            source.mkdir()
            (source / 'build_info.generated.h').write_text(
                '#define FIRMWARE_UPGRADE_TOOL_VERSION L"9.9.9.999"\n', encoding='utf-8')
            result = subprocess.run([
                'powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', str(ISP / 'scripts/verify_local.ps1'), '-ToolRoot', folder],
                capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(b'no build-version pin', result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
