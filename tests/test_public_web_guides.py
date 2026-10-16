"""Keep public navigation linked and independent of delivery milestones."""
from pathlib import Path
import json
import re
import subprocess
import tempfile
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
WEB = companion_path('Web')
ISP = WEB / 'slide_isp_tool_for_RL78'
BOOT = WEB / 'slide_boot_loader_for_RL78'
NAV = WEB / 'released.github.io/README.md'


class PublicWebGuides(unittest.TestCase):
    def test_can_roles_are_explicit_without_changing_project_identifiers(self):
        for page in [ISP / 'index.md', BOOT / 'index.md']:
            text = page.read_text(encoding='utf-8-sig')
            for phrase in ['F24 (MCU CAN host)', 'F13 (MCU CAN device)',
                           'F24 (MCU CAN device)', 'Misc']:
                self.assertIn(phrase, text)
            self.assertNotRegex(text, r'https?://[^\s\n]*\(MCU')
        isp = (ISP / 'index.md').read_text(encoding='utf-8-sig')
        for label in ['| F24 (MCU CAN host) Boot |', '| F24 (MCU CAN host) App |',
                      '| F13 (MCU CAN device) Boot |', '| F13 (MCU CAN device) App |']:
            self.assertIn(label, isp)

    def test_public_resources_downloads_anchors_and_sync(self):
        pattern = re.compile(r'<!-- PUBLIC_RESOURCES_BEGIN -->.*?<!-- PUBLIC_RESOURCES_END -->', re.S)
        source = (ROOT / 'docs/MULTIBUS_TOOL_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        block, = pattern.findall(source)
        for path in [ISP / 'index.md', BOOT / 'index.md']:
            text = path.read_text(encoding='utf-8-sig')
            actual, = pattern.findall(text)
            if path.parent == ISP:
                self.assertEqual(actual, block)
            else:
                self.assertNotEqual(actual, block)
                self.assertIn('https://released.github.io/slide_isp_tool_for_RL78/#downloads', actual)
                self.assertNotIn('raw.githubusercontent', actual)
            self.assertNotRegex(text, r'_Avery|[A-Z]:[\\/]|\.\./\.\./python|SourceCode')
            self.assertNotIn('有效core長度仍為96', text)
        for filename in ['FirmwareUpgradeTool.exe', 'RL78_F24_Bridge_boot.mot',
                         'RL78_F24_Bridge_app_bridge_update.bin', 'boot0000_1FFF.hex',
                         'boot_app.hex', 'RL78_F13_Boot_loader_CAN_app_update.bin']:
            self.assertIn('https://raw.githubusercontent.com/released/', block)
            self.assertIn('/' + filename + ')', block)
        anchors = [('slide_isp_tool_for_RL78', name) for name in
                   ['tool-bridge-flow', 'tool-operation', 'custom-transfer',
                    'host-bridge-firmware', 'f13-device-update']]
        anchors.append(('slide_boot_loader_for_RL78', 'boot-app-porting'))
        anchors.append(('slide_boot_loader_for_RL78', 'f13-boot-decision'))
        for site, anchor in anchors:
            target = (WEB / site / 'index.md').read_text(encoding='utf-8-sig')
            self.assertEqual(target.count('<a id="' + anchor + '"></a>'), 1)
            self.assertIn('https://released.github.io/' + site + '/#' + anchor, block)

    def test_resource_sync_is_atomic_and_check_only_is_read_only(self):
        script = ROOT / 'scripts/sync_public_resources.ps1'
        with tempfile.TemporaryDirectory(prefix='public_sync_') as folder:
            web = Path(folder)
            pages = [web / site / 'index.md' for site in
                     ['slide_isp_tool_for_RL78', 'slide_boot_loader_for_RL78']]
            for page in pages:
                page.parent.mkdir()
                page.write_text('prefix\n<!-- PUBLIC_RESOURCES_BEGIN -->stale<!-- PUBLIC_RESOURCES_END -->\nsuffix', encoding='utf-8')
            command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                       '-File', str(script), '-WebRoot', folder]
            before = [page.read_bytes() for page in pages]
            self.assertNotEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertEqual([page.read_bytes() for page in pages], before)
            pages[1].write_text('missing marker', encoding='utf-8')
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(pages[0].read_bytes(), before[0])
            pages[1].write_bytes(before[1])
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            for page in pages:
                self.assertTrue(page.read_text(encoding='utf-8').startswith('prefix\n'))
                self.assertTrue(page.read_text(encoding='utf-8').endswith('\nsuffix'))

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
        for phrase in ['最低 Boot 版本', 'IRQ ABI',
                       'G16', 'CRC提供完整性', 'BLOCKED']:
            self.assertIn(phrase, text)

    def test_single_owner_sections_and_generic_diagram_roles(self):
        catalog = json.loads((ROOT/'scripts/web_guide_sections.json').read_text(encoding='utf-8-sig'))
        sources = {'DEVICE_WORKSHOP':'DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md',
                   'F24_DEVICE':'F24_CAN_DEVICE_DESIGN_zh-TW.md',
                   'F13_BOOT_DECISION':'F13_BOOT_UPDATE_FLOW_zh-TW.md',
                   'F24_APP_IRQ':'SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md',
                   'COMMON_IMAGE_POLICY':'COMMON_IMAGE_METADATA.md',
                   'PUBLIC_RESOURCES':'MULTIBUS_TOOL_GUIDE_zh-TW.md'}
        pages = {p.parent.name:p.read_text(encoding='utf-8-sig')
                 for p in (ISP/'index.md',BOOT/'index.md')}
        for section, config in catalog.items():
            pattern = rf'<!-- {section}_BEGIN -->.*?<!-- {section}_END -->'
            canonical, = re.findall(pattern,(ROOT/'docs'/sources[section]).read_text(encoding='utf-8-sig'),re.S)
            for folder, text in pages.items():
                block, = re.findall(pattern,text,re.S)
                if folder == config['owner']:
                    self.assertEqual(block,canonical)
                else:
                    self.assertLess(len(block),800)
                    self.assertNotIn('```',block)
                    self.assertIn(f"https://released.github.io/{config['owner']}/#{config['anchor']}",block)
        for text in pages.values():
            for diagram in re.findall(r'```mermaid\n(.*?)\n```',text,re.S):
                self.assertNotRegex(diagram,r'\bF(?:13|24)\b')
                self.assertNotRegex(diagram,r'MCU CAN (?:host|device) MCU CAN')
        boot = pages[BOOT.name]
        for section in ('DEVICE_WORKSHOP','RL78_PORTING','F24_DEVICE','FLASH_REFERENCE'):
            block, = re.findall(rf'<!-- {section}_BEGIN -->.*?<!-- {section}_END -->',boot,re.S)
            self.assertNotRegex(block,r'F24.{0,15}(?:host|HOST)|BUR1|Bridge_boot')
        self.assertIn('<a id="custom-transfer"></a>',pages[ISP.name])

    def test_reader_pages_exclude_development_conversation(self):
        paths = [ISP/'index.md',BOOT/'index.md'] + [ROOT/'docs'/name for name in
            ('DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md','F24_CAN_DEVICE_DESIGN_zh-TW.md',
             'RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md','FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md',
             'MULTIBUS_TOOL_GUIDE_zh-TW.md','SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md')]
        for path in paths:
            text = path.read_text(encoding='utf-8-sig')
            self.assertNotRegex(text,r'G16\s*新版|新版\s*I2C|依使用者要求延後|本輪|本次未執行|native pipe|舊\s*format4|舊版 UART|較容易修改|為什麼 F24 device|不能把 F13')

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
