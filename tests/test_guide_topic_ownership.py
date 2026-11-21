"""First-visit navigation, single content owners and precise legacy destinations."""
import json
import os
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
WEB = Path(os.environ['FIRMWARE_WEB_ROOT']) if os.environ.get('FIRMWARE_WEB_ROOT') else None
BUSES = (('CAN', 'can-update', 'can-device'), ('CAN FD', 'can-fd', 'can-fd-device'),
         ('I2C', 'i2c', 'i2c-device'), ('UART', 'uart', 'uart-device'),
         ('Xmodem', 'xmodem', 'xmodem-device'))


@unittest.skipUnless(WEB, 'Set FIRMWARE_WEB_ROOT to the local Web workspace')
class GuideTopicOwnershipTests(unittest.TestCase):
    def read(self, site, page):
        return (WEB/site/(page+'.md')).read_text(encoding='utf-8-sig')

    def test_all_three_entries_offer_each_interface_without_model_benchmarks(self):
        for site in ('slide_simple_renesas_isp_tool', 'slide_boot_loader_for_RL78',
                     'slide_boot_loader_for_RH850'):
            home = self.read(site, 'index')
            for label, operation, engineering in BUSES:
                page = operation if site.endswith('isp_tool') else engineering
                self.assertRegex(home, rf'\| {label} \|')
                self.assertIn(']('+page+'.html)', home)
            self.assertNotIn('## CAN 範例分類', home)
            self.assertNotIn('_MEASURED_BEGIN', home)
            self.assertLess(len(home.splitlines()), 70)

    def test_tool_owns_operation_and_engineering_owns_measurements(self):
        for bus, title in (('i2c', 'I2C'), ('uart', 'UART'), ('xmodem', 'Xmodem')):
            text = self.read('slide_simple_renesas_isp_tool', bus)
            self.assertEqual(text.count('\n# '+title+' 操作\n'), 1)
            self.assertNotIn('\n## '+title+' 操作\n', text)
        for page in (WEB/'slide_simple_renesas_isp_tool').glob('*.md'):
            if page.name in ('AGENTS.md', 'PUBLIC_CONTENT_GUIDELINES_zh-TW.md'):
                continue
            self.assertNotIn('_MEASURED_BEGIN', page.read_text(encoding='utf-8-sig'), page.name)
        owner = self.read('slide_boot_loader_for_RL78', 'experiments')
        self.assertLess(owner.index('id="experiment-i2c"'), owner.index('id="experiment-uart"'))
        self.assertLess(owner.index('id="experiment-uart"'), owner.index('id="experiment-xmodem"'))
        for anchor in ('f13-i2c', 'f13-uart', 'f13-xmodem', 'f24-i2c',
                       'f24-uart', 'f24-xmodem', 'g16-i2c', 'g16-uart', 'g16-xmodem'):
            marker = f'id="{anchor}-update-measurements"'
            self.assertEqual(owner.count(marker), 1)
            for page in (WEB/'slide_boot_loader_for_RL78').glob('*.md'):
                if page.stem != 'experiments':
                    self.assertNotIn(marker, page.read_text(encoding='utf-8-sig'), page.name)

    def test_interface_entry_reaches_each_model_before_implementation_prose(self):
        profiles = json.loads((ROOT/'docs/data/device_upgrade_paths.json').read_text(encoding='utf-8-sig'))
        for profile in profiles:
            text = self.read('slide_boot_loader_for_'+profile['platform'], profile['page'])
            start = text.index(f'id="upgrade-{profile["key"]}"')
            self.assertLess(text.index(f'](#upgrade-{profile["key"]})'), start)
            self.assertIn(profile['part'], text[:start])
            self.assertIn('experiments.html#', text[:start])
        source = (ROOT/'docs/DEVICE_APP_UPGRADE_PATHS_zh-TW.md').read_text(encoding='utf-8-sig')
        self.assertNotIn('```mermaid', source)
        self.assertNotIn('#### Simple Renesas ISP Tool 操作步驟', source)
        self.assertIn('RAM 交接', source)

    def test_old_measurement_links_go_to_the_single_exact_owner(self):
        mapping = json.loads((WEB/'slide_simple_renesas_isp_tool/guide-map.json').read_text(encoding='utf-8'))['aliases']
        for model, bus in (('f13', 'i2c'), ('f24', 'uart'), ('g16', 'i2c')):
            anchor = model+'-'+bus+'-update-measurements'
            self.assertEqual(mapping[anchor], {'guide': 'slide_boot_loader_for_RL78',
                                               'page': 'experiments.html', 'anchor': anchor})
        for platform in ('RL78', 'RH850'):
            owner = self.read('slide_boot_loader_for_'+platform, 'experiments')
            app = self.read('slide_boot_loader_for_'+platform, 'app-development')
            self.assertIn('### App BIN 更新耗時參考', owner)
            self.assertNotIn('### App BIN 更新耗時參考', app)
        rh = self.read('slide_boot_loader_for_RH850', 'experiments')
        fd = self.read('slide_boot_loader_for_RH850', 'can-fd-device')
        self.assertIn('id="rh850-can-fd-update-measurements"', rh)
        self.assertIn('experiments.html#rh850-can-fd-update-measurements', fd)

    def test_shared_wire_has_one_owner_across_platforms(self):
        rl = self.read('slide_boot_loader_for_RL78', 'serial-device')
        rh = self.read('slide_boot_loader_for_RH850', 'serial-device')
        self.assertIn('| Frame byte |', rl)
        self.assertNotIn('| Frame byte |', rh)
        self.assertIn('../slide_boot_loader_for_RL78/serial-device.html#guide-serial-device-5', rh)


if __name__ == '__main__':
    unittest.main(verbosity=2)
