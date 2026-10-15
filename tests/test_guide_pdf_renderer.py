"""Regression checks for guide-only Markdown parsing; no PDF files authored."""
import importlib.util
from pathlib import Path
import re
import sys
import unittest

from pypdf import PdfReader
from reportlab.platypus import Paragraph

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("guide_pdf", ROOT / "scripts/generate_f13_guide_pdf.py")
GUIDE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GUIDE)


class GuideMarkdownTests(unittest.TestCase):
    def test_fourth_level_heading_and_hidden_sync_comments(self):
        GUIDE.register_fonts()
        styles = GUIDE.make_styles()
        story = GUIDE.build_story(ROOT / "tests/fixtures/guide_pdf_headings.md", styles)
        paragraphs = [item for item in story if isinstance(item, Paragraph)]
        self.assertEqual([item.getPlainText() for item in paragraphs], [
            "PDF renderer fixture", "Fourth-level heading", "Visible body text."])
        self.assertIs(paragraphs[1].style, styles["h3"])

    def test_final_guide_outputs(self):
        version = (ROOT / "src/build_version.txt").read_text(encoding="ascii").strip()
        expected = {
            "FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf": 10,
            "FirmwareUpgradeTool_MultiBus_Guide_zh-TW.pdf": 11,
            "F13_CAN_Bootloader_Update_Guide_zh-TW.pdf": 30,
        }
        for name, pages in expected.items():
            with self.subTest(guide=name):
                reader = PdfReader(ROOT / "docs" / name)
                self.assertEqual(len(reader.pages), pages)
                text = "\n".join(page.extract_text() for page in reader.pages)
                self.assertNotIn("F13_BOOT_DECISION_", text)
                self.assertNotIn("F24_APP_IRQ_", text)
                self.assertNotIn("COMMON_IMAGE_POLICY_", text)
                self.assertIn("UPD1",text)
                self.assertIn("921600",text)
                self.assertIn("device_uart_baud",text)
                self.assertNotIn("####", text)
                self.assertIn(version, re.sub(r"\s+", "", text))
                if name.startswith('F13_'):
                    self.assertIn('舊format4', re.sub(r'\s+', '', text))
                if name.startswith('FirmwareUpgradeTool_Shared'):
                    self.assertIn('f24_board', text)
                    self.assertNotIn('RL78_F24_CAN_host_upgrade', text)


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
