"""Keep the public field reference aligned with actual display adapters."""
from pathlib import Path
import ast
import re
import unittest

from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8-sig")


class ToolUiFieldGuide(unittest.TestCase):
    def setUp(self):
        self.document = read("docs/UPDATE_VALIDATION_zh-TW.md")
        self.section = self.document.split('<a id="tool-ui-fields"></a>', 1)[1]
        self.section = self.section.split("### 差異表與分階段進度", 1)[0]

    def test_all_twenty_display_captions_have_one_reference_row(self):
        source = read("src/main_frame.cpp")
        captions = source.split("firmware_field_captions[kFirmwareFieldCount]", 1)[1]
        captions = captions.split("};", 1)[0]
        captions = re.findall(r'L"([^"]+)"', captions)
        self.assertEqual(len(captions), 20)
        for caption in captions:
            self.assertEqual(self.section.count(f"| {caption} |"), 1, caption)

    def test_status_labels_are_real_and_scoped(self):
        image = read("src/main_frame_image.cpp")
        misc = read("src/main_frame_misc.cpp")
        i2c = read("src/main_frame_i2c_image.cpp")
        for label in ("UNAVAILABLE", "INVALID (see log)", "ENABLED | same / newer",
                      "DISABLED | any version", "RECOVERY | no valid App",
                      "Discover Target required"):
            self.assertIn(label, image)
            self.assertIn(label.replace("|", r"\|"), self.section)
        for label in ("INVALID - HOST APP required", "Bridge UART contract3 / BUR1",
                      "PC -> HOST BRIDGE"):
            self.assertIn(label, misc)
            self.assertIn(label, self.section)
        for label in ("RAW BIN - no manifest", "MATCH (G16 reference)",
                      "Calculated CRC16", "Stored CRC16"):
            self.assertIn(label, i2c)
            self.assertIn(label, self.section)
        for boundary in ("不是來源認證", "不包含 CRC 相符", "不是目前板上版本",
                         "數值欄位不會逐一", "不代表已與 live Boot",
                         "不是零值", "不代表 disabled", "目前為唯讀參考操作"):
            self.assertIn(boundary, self.section)

    def test_error_examples_exist_in_production_parsers(self):
        messages = re.findall(r"\| `([^`]+\.)` \|", self.section)
        self.assertGreaterEqual(len(messages), 5)
        source = re.sub(r'"\s*L"', "", read("src/core/firmware_update_profile.cpp") +
                        read("src/ui/update_feedback.h"))
        for message in messages:
            self.assertIn(message, source)
        self.assertIn("App Start Override does not match image metadata (",
                      read("src/main_frame_image.cpp"))
        self.assertEqual(0x2000 + 8192 - 1, 0x3FFF)

    def test_can_mode_rate_blocker_has_action_and_emphasis_reference(self):
        for kind in ("mode", "bitrate"):
            message = f"Selected CAN {kind} is not supported by the image."
            self.assertIn(message, read("src/ui/update_feedback.h"))
            self.assertIn(message, self.section)
        for text in ("粗體黑字", "Check BIN supported CAN mode", "此時不接續顯示 Discover 指引"):
            self.assertIn(text, self.section)

    def test_stable_links_and_single_web_owner(self):
        web = companion_path("Web")
        owner = (web / "slide_simple_renesas_isp_tool/index.md").read_text(encoding="utf-8-sig")
        other = (web / "slide_boot_loader_for_RL78/index.md").read_text(encoding="utf-8-sig")
        pattern = r"<!-- UPDATE_EVIDENCE_BEGIN -->.*?<!-- UPDATE_EVIDENCE_END -->"
        self.assertEqual(re.findall(pattern, self.document, re.S),
                         re.findall(pattern, owner, re.S))
        for anchor in ("tool-ui-fields", "tool-ui-status", "tool-ui-metadata",
                       "tool-ui-adapters", "tool-metadata-comparison", "tool-console-layout", "tool-log-auto-scroll"):
            self.assertEqual(owner.count(f'<a id="{anchor}"></a>'), 1)
            self.assertNotIn(f'<a id="{anchor}"></a>', other)
        self.assertIn("[欄位、Metadata 與狀態訊息](#tool-ui-fields)", owner)
        self.assertNotRegex(self.section, r"[A-Z]:[\\/]|_Avery|SourceCode")
        for target in re.findall(r"blob/main/(src/[^)]+)", self.section):
            self.assertTrue((ROOT / target).is_file(), target)

    def test_pdf_table_reader_keeps_escaped_ui_separators_in_one_cell(self):
        tree = ast.parse(read("scripts/generate_f13_guide_pdf.py"))
        function = next(node for node in tree.body
                        if isinstance(node, ast.FunctionDef) and node.name == "split_table_row")
        namespace = {"re": re}
        exec(compile(ast.Module(body=[function], type_ignores=[]), "table_reader", "exec"), namespace)
        split_row = namespace["split_table_row"]
        self.assertEqual(split_row(r"| Policy | `DEVICE APP \| CAN` | OK |"),
                         ["Policy", "`DEVICE APP | CAN`", "OK"])
        self.assertEqual(split_row("| Field | | N/A |"), ["Field", "", "N/A"])
        for row in self.section.splitlines():
            if row.startswith("| "):
                self.assertIn(len(split_row(row)), (3, 4), row)


if __name__ == "__main__":
    unittest.main()
