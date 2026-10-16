"""Check canonical porting content, local entry links and bounded web sync."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from urllib.parse import urlsplit
from workspace_paths import companion_path

from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[1]
WEB = companion_path('Web')
DOC = ROOT / "docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md"
SCRIPT = ROOT / "scripts/sync_rl78_porting_guide.ps1"
PATTERN = re.compile(r"<!-- RL78_PORTING_BEGIN -->.*?<!-- RL78_PORTING_END -->", re.S)


class PortingGuideTests(unittest.TestCase):
    def run_sync(self, web, check=False):
        command = ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
                   "-File", str(SCRIPT), "-WebRoot", str(web)]
        if check:
            command.append("-CheckOnly")
        return subprocess.run(command, capture_output=True, timeout=30)

    def test_canonical_web_and_reader_entries(self):
        source = DOC.read_text(encoding="utf-8-sig")
        page = (WEB / "slide_boot_loader_for_RL78/index.md").read_text(encoding="utf-8-sig")
        self.assertEqual(len(PATTERN.findall(source)), 1)
        self.assertEqual(PATTERN.findall(source), PATTERN.findall(page))
        self.assertIn('<a id="boot-app-porting"></a>', page)
        for path in [ROOT / "README.md",
                     WEB / "slide_isp_tool_for_RL78/index.md",
                     WEB / "released.github.io/README.md"]:
            text = path.read_text(encoding="utf-8-sig")
            self.assertTrue("RL78_BOOT_APP_PORTING_GUIDE" in text or "#boot-app-porting" in text)

    def test_links_and_source_contract(self):
        text = DOC.read_text(encoding="utf-8-sig")
        self.assertEqual(len(re.findall(r"(?m)^```", text)) % 2, 0)
        self.assertEqual(text.count("```mermaid"), 2)
        for link in re.findall(r"\]\(([^)]+)\)", text):
            if link.startswith('https://'):
                self.assertIn(urlsplit(link).hostname, ['github.com', 'released.github.io'])
            else:
                self.assertTrue((DOC.parent / link.split("#")[0]).exists(), link)
        config = (companion_path('F13Boot') / 'can_boot_config.h').read_text()
        self.assertRegex(config, r"CAN_BOOT_IMAGE_FORMAT_VERSION\s+\(0x06U\)")
        self.assertRegex(config, r"CAN_BOOT_MANIFEST_SIZE\s+\(128U\)")
        f24 = companion_path('F13Boot').parent/'RL78_F24_Boot_loader_CAN'
        f24_vectors = (f24 / 'device_vectors.asm').read_text()
        self.assertIn('CMP !0xF710,#0x55', f24_vectors)
        f24_config = (f24 / 'can_boot_config.h').read_text()
        self.assertRegex(f24_config,r'CAN_BOOT_RAM_OWNER_BOOT\s+\(0xAAU\)')
        self.assertRegex(f24_config,r'CAN_BOOT_RAM_OWNER_APPLICATION\s+\(0x55U\)')
        for phrase in ["0x2100..0x217F", "0x2180..0x21FF", "app_image.json",
                       "Physical BRK", "RETI", "0xAA", "0x55", "F1V1", "F2V1",
                       "package_images.ps1", "Windows PowerShell"]:
            self.assertIn(phrase, text)
        self.assertIn('0xFF700',text)
        self.assertIn('0xFF71C',text)
        self.assertNotRegex(text,r'F24.{0,12}host|BUR1|Python|python')

    def test_check_only_is_read_only_and_sync_preserves_surroundings(self):
        with tempfile.TemporaryDirectory(prefix="rl78_porting_") as folder:
            web = Path(folder)
            target = web / "slide_boot_loader_for_RL78/index.md"
            target.parent.mkdir()
            original = "Existing user prefix\n<!-- RL78_PORTING_BEGIN -->\nstale\n<!-- RL78_PORTING_END -->\nExisting suffix\n"
            target.write_text(original, encoding="utf-8")
            self.assertNotEqual(self.run_sync(web, True).returncode, 0)
            self.assertEqual(target.read_text(encoding="utf-8"), original)
            self.assertEqual(self.run_sync(web).returncode, 0)
            self.assertEqual(self.run_sync(web, True).returncode, 0)
            result = target.read_text(encoding="utf-8")
            self.assertTrue(result.startswith("Existing user prefix\n"))
            self.assertTrue(result.endswith("\nExisting suffix\n"))
            target.write_text(result + PATTERN.findall(result)[0], encoding="utf-8")
            before = target.read_bytes()
            self.assertNotEqual(self.run_sync(web).returncode, 0)
            self.assertEqual(target.read_bytes(), before)
            target.write_text("User page without a managed block\n", encoding="utf-8")
            before = target.read_bytes()
            self.assertNotEqual(self.run_sync(web).returncode, 0)
            self.assertEqual(target.read_bytes(), before)

    def test_offline_pdf_has_content_and_both_diagrams(self):
        reader = PdfReader(DOC.with_suffix(".pdf"))
        self.assertGreater(len(reader.pages), 2)
        text = "\n".join(page.extract_text() for page in reader.pages)
        for phrase in ["移植指南", "F1V1", "F2V1", "0x2100..0x217F",
                       "Windows PowerShell", "0x55", "app_image.json"]:
            self.assertIn(phrase, text)
        self.assertNotIn("RL78_PORTING_BEGIN", text)
        self.assertNotIn("f24_board", text)
        self.assertNotIn("RL78_F24_CAN_host_upgrade", text)
        self.assertEqual(sum(len(page.images) for page in reader.pages), 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
