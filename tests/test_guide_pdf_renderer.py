"""Regression checks for guide-only Markdown parsing; no PDF files authored."""
import importlib.util
from pathlib import Path
import re
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock

from pypdf import PdfReader
from reportlab.platypus import Paragraph, Spacer, NextPageTemplate

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("guide_pdf", ROOT / "scripts/generate_f13_guide_pdf.py")
GUIDE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GUIDE)


class GuideMarkdownTests(unittest.TestCase):
    def test_portrait_break_does_not_leave_table_spacer_on_blank_page(self):
        GUIDE.register_fonts()
        source = '# Test\n\n| A | B |\n| --- | --- |\n| x | y |\n\n#### Diagram\n\n![diagram](fake.png)\n'
        with tempfile.TemporaryDirectory(prefix='guide_page_break_') as folder:
            path = Path(folder)/'source.md'
            path.write_text(source,encoding='utf-8')
            picture = MagicMock()
            picture.__enter__.return_value.width = 100
            picture.__enter__.return_value.height = 200
            with patch.object(GUIDE.PillowImage,'open',return_value=picture), \
                    patch.object(GUIDE,'diagram_image',return_value=(Spacer(1,10),0.5)):
                story = GUIDE.build_story(path,GUIDE.make_styles())
        portrait = next(i for i,value in enumerate(story) if isinstance(value,NextPageTemplate))
        self.assertNotIsInstance(story[portrait-1],Spacer)

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
            "FirmwareUpgradeTool_MultiBus_Guide_zh-TW.pdf": 14,
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
                self.assertNotIn("PUBLIC_RESOURCES_", text)
                self.assertNotIn('<a id=', text)
                self.assertIn("UPD1",text)
                self.assertIn("921600",text)
                self.assertIn("device_uart_baud",text)
                for role in ['F24 (MCU CAN host)', 'F13 (MCU CAN device)', 'F24 (MCU CAN device)']:
                    self.assertIn(role, re.sub(r'\s+', ' ', text))
                self.assertNotIn("####", text)
                self.assertIn(version, re.sub(r"\s+", "", text))
                if name.startswith('F13_'):
                    self.assertIn('舊format4', re.sub(r'\s+', '', text))
                if name.startswith('FirmwareUpgradeTool_Shared'):
                    self.assertIn('f24_board', text)
                    self.assertNotIn('RL78_F24_CAN_host_upgrade', text)
                if name.startswith('FirmwareUpgradeTool_MultiBus'):
                    self.assertIn('下載與操作總覽', text)
                    self.assertIn('六個操作與流程圖入口', text)
                    self.assertEqual(sum(len(page.images) for page in reader.pages), 7)


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
