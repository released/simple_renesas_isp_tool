"""Guide renderer checks; authored fixtures stay in temporary directories."""
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
    def test_markdown_navigation_remains_clickable_in_pdf(self):
        value = GUIDE.inline_markup('[Guide](https://released.github.io/slide_boot_loader_for_RL78/#device-app-irq)')
        self.assertIn('<link href="https://released.github.io/slide_boot_loader_for_RL78/#device-app-irq"',value)
        self.assertIn('<u>Guide</u>',value)

    def test_local_anchor_has_a_pdf_destination(self):
        GUIDE.register_fonts()
        with tempfile.TemporaryDirectory(prefix='guide_links_') as folder:
            source=Path(folder)/'guide.md'
            source.write_text('# Guide\n\n[Jump](#details)\n\n<a id="details"></a>\n\n## Details\n\nBody.\n',encoding='utf-8')
            output=Path(folder)/'guide.pdf'
            GUIDE.create_pdf(source,output,'Guide','Guide')
            reader=PdfReader(output)
            annotations=[a.get_object() for p in reader.pages for a in p.get('/Annots',[])]
            self.assertTrue(any(a.get('/Subtype')=='/Link' and '/Dest' in a for a in annotations))

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
        expected = {
            "FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf": (18,['BUR1','f24_board','115200','App 工程','Device App 更新操作圖','Host App 更新操作圖']),
            "FirmwareUpgradeTool_MultiBus_Guide_zh-TW.pdf": (4,['下載與操作總覽','CRC16','100 kHz','128']),
            "F13_CAN_Bootloader_Update_Guide_zh-TW.pdf": (10,['UPD1','0x2100','30','SEAL']),
        }
        for name, (diagrams, phrases) in expected.items():
            with self.subTest(guide=name):
                reader = PdfReader(ROOT / "docs" / name)
                self.assertGreater(len(reader.pages), 2)
                text = "\n".join(page.extract_text() for page in reader.pages)
                self.assertNotIn("F13_BOOT_DECISION_", text)
                self.assertNotIn("F24_APP_IRQ_", text)
                self.assertNotIn("COMMON_IMAGE_POLICY_", text)
                self.assertNotIn("PUBLIC_RESOURCES_", text)
                self.assertNotIn('<a id=', text)
                for phrase in phrases:
                    self.assertIn(phrase,text)
                self.assertNotIn("####", text)
                self.assertNotRegex(text,r'本輪|native pipe|舊\s*format4|G16\s*新版')
                self.assertEqual(sum(len(page.images) for page in reader.pages),diagrams)
                if name.startswith('FirmwareUpgradeTool_Shared'):
                    self.assertIn('f24_board', text)
                    self.assertNotIn('RL78_F24_CAN_host_upgrade', text)


if __name__ == "__main__":
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
