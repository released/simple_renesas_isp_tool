"""Mechanical public-copy gates; contextual wording still receives human review."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
NAME = 'PUBLIC_CONTENT_GUIDELINES_zh-TW.md'
WEB = companion_path('Web')


def run_script(name, *arguments):
    return subprocess.run(
        ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
         str(ROOT / 'scripts' / name), *map(str, arguments)],
        capture_output=True)


class PublicContentRules(unittest.TestCase):
    def test_guideline_contract_and_local_entry_points(self):
        source = (WEB / NAME).read_text(encoding='utf-8-sig')
        for text in ('語氣與標題', '主題歸屬', '共通概念', '目前及未來',
                     '來源', '相對路徑', 'CheckOnly', '不自動改寫', '驗證', '發布前核對'):
            self.assertIn(text, source)
        self.assertNotRegex(source, r'F13|F24|MCU CAN|FirmwareUpgradeTool|兩份 JSON|ISP Tool')
        self.assertIn(NAME, (WEB / 'AGENTS.md').read_text(encoding='utf-8-sig'))
        self.assertEqual((ROOT / 'docs' / NAME).read_text(encoding='utf-8-sig').split('\n', 1)[1], source)
        self.assertIn(NAME, (ROOT / 'AGENTS.md').read_text(encoding='utf-8-sig'))
        for folder in ('slide_boot_loader_for_RL78', 'slide_simple_renesas_isp_tool'):
            page = WEB / folder
            self.assertIn(NAME, (page / 'AGENTS.md').read_text(encoding='utf-8-sig'))
            self.assertIn(NAME, (page / 'README.md').read_text(encoding='utf-8-sig'))
            actual = (page / NAME).read_text(encoding='utf-8-sig')
            self.assertEqual(actual.split('\n', 1)[1], source)
        result = run_script('sync_public_content_guidelines.ps1', '-WebRoot', WEB, '-CheckOnly')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_sync_check_only_and_invalid_target_do_not_write(self):
        with tempfile.TemporaryDirectory(prefix='copy_rules_') as temporary:
            root = Path(temporary)
            (root / NAME).write_bytes((WEB / NAME).read_bytes())
            (root / 'scripts').mkdir()
            script = root / 'scripts/sync_public_content_guidelines.ps1'
            shutil.copyfile(WEB / 'scripts/sync_public_content_guidelines.ps1', script)
            def run(*arguments):
                return subprocess.run(['powershell.exe', '-NoProfile', '-File', str(script), *arguments], capture_output=True)
            copies = []
            for folder in ('slide_boot_loader_for_RL78', 'slide_simple_renesas_isp_tool'):
                page = root / folder
                page.mkdir()
                for name in ('index.md', 'README.md', 'AGENTS.md'):
                    (page / name).write_text(NAME, encoding='utf-8')
                copies.append(page / NAME)
            result = run()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            before = [path.read_bytes() for path in copies]
            result = run('-CheckOnly')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(before, [path.read_bytes() for path in copies])
            copies[0].write_text('stale', encoding='utf-8')
            stale = copies[0].read_bytes()
            self.assertNotEqual(run('-CheckOnly').returncode, 0)
            self.assertEqual(copies[0].read_bytes(), stale)
            self.assertNotEqual(run('-Project', 'missing_site').returncode, 0)
            self.assertEqual(copies[0].read_bytes(), stale)
            self.assertEqual(copies[1].read_bytes(), before[1])
            future = root / 'slide_future_topic'
            future.mkdir()
            self.assertEqual(run('-Project', future.name).returncode, 0)
            self.assertEqual((future / NAME).read_bytes(), before[1])

    def test_checker_accepts_technical_limits_and_ignores_code_as_prose(self):
        with tempfile.TemporaryDirectory(prefix='copy_valid_') as temporary:
            path = Path(temporary) / 'valid.md'
            path.write_text(
                '# CRC 範圍\nCRC 必須符合；不符時禁止擦寫。\n'
                '本次更新的計數差值不代表開機以來峰值。\n'
                'metadata format6、minimum Boot 版本與 IRQ ABI 須符合。\n'
                '````text\n### 不同 CRC 不要混淆\n```\n本輪 fixture\n````\n'
                '~~~text\n你必須知道\n~~~\n', encoding='utf-8')
            before = path.read_bytes()
            result = run_script('verify_public_content.ps1', '-TextPath', path)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(path.read_bytes(), before)

    def test_checker_rejects_narrow_regressions_without_rewriting(self):
        fixtures = {
            'private-path': '```powershell\nD:\\SourceCode\\private\\app.bin\n```\n',
            'invalid-glyph': 'vector \ue000\n',
            'publication-label': '尚未發布\n',
            'publication-plan': 'GitHub 來源將於實板驗證後提供。\n',
            'development-dialogue': '依使用者要求，本輪修改\n',
            'reader-judgment': '你必須知道這件事情\n',
            'corrective-heading': '### 先分清三層責任\n',
            'unclosed-fence': '```text\nunclosed\n',
        }
        with tempfile.TemporaryDirectory(prefix='copy_invalid_') as temporary:
            path = Path(temporary) / 'invalid.md'
            for label, text in fixtures.items():
                with self.subTest(label=label):
                    path.write_text(text, encoding='utf-8')
                    before = path.read_bytes()
                    result = run_script('verify_public_content.ps1', '-TextPath', path)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(label.encode(), result.stdout + result.stderr)
                    self.assertEqual(path.read_bytes(), before)

    def test_current_reader_copy_and_technical_boundaries(self):
        result = run_script('verify_public_content.ps1', '-WebRoot', WEB)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for name in ('CAN_UPDATE_WALKTHROUGH_zh-TW.md', 'FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md',
                     'DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md'):
            text = (ROOT / 'docs' / name).read_text(encoding='utf-8-sig')
            self.assertNotRegex(text, r'先分清|不要混淆|應看到什麼|替你')
        flash = (ROOT / 'docs/FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        self.assertEqual(len(re.findall(r'(?m)^\| 0x[0-9A-F]{2} \| 0x[0-9A-F]{5} \| [0-9A-F]{2} \|', flash)), 256)
        self.assertIn('0x0003', flash)
        self.assertIn('0x80..0xFF', flash)
        workshop = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        for phrase in ('RETI', '`vect=`', 'stack', 'pending flag', 'CheckOnly 僅檢查'):
            self.assertIn(phrase, workshop)


if __name__ == '__main__':
    unittest.main()
