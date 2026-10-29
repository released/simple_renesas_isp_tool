"""English F24 README parity and source/link checks; no builds or Git writes."""
from pathlib import Path
import json
import os
import re
import unittest
from urllib.parse import urlparse
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
WEB = companion_path('Web')
BOOT = Path(os.environ.get('FIRMWARE_F24_DEVICE_BOOT_PROJECT') or
            companion_path('F13Boot').parent / 'RL78_F24_Boot_loader_CAN')
TRANSFER = Path(os.environ.get('FIRMWARE_F24_DEVICE_TRANSFER_PROJECT') or BOOT.parent / 'RL78_F24_CAN_Transfer')
APP = Path(os.environ.get('FIRMWARE_F24_DEVICE_APP_PROJECT') or BOOT.parent / 'RL78_F24_Boot_loader_CAN_app')


class F24DeviceReadmes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = {folder.name: (folder / 'README.md').read_text(encoding='utf-8-sig')
                    for folder in (TRANSFER, BOOT, APP)}

    def test_english_role_checkout_and_public_links(self):
        for name, text in self.text.items():
            self.assertTrue(text.isascii(), name)
            self.assertIn('R7F124FPJ5xFB', text)
            self.assertIn('same parent', text)
            self.assertIn('-main', text)
            self.assertNotIn('RL78_F13', text)
            for project in (TRANSFER.name, BOOT.name, APP.name):
                self.assertIn('https://github.com/released/' + project, text)
            self.assertNotRegex(text, r'[A-Z]:[\\/]|_Avery|SourceCode')
            self.assertEqual(text.count('```') % 2, 0)

    def test_corresponding_f13_reader_structure(self):
        shared = {
            BOOT: ('Fixed reusable IRQ contract', 'Status', 'Public examples and checkout layout',
                   'Hardware and CAN settings', 'Boot decision', 'Recovery and limitations', 'Main files'),
            APP: ('Public examples and checkout layout', 'App-only IRQ extension', 'Memory layout',
                  'Runtime identity and boot provenance', 'CAN boot entry', 'Build and artifacts',
                  'Debugging with E2 Lite', 'Verification requirements'),
        }
        for folder, headings in shared.items():
            reference = folder.parent / folder.name.replace('F24', 'F13', 1) / 'README.md'
            source = reference.read_text(encoding='utf-8-sig')
            for heading in headings:
                self.assertIn('## ' + heading, source)
                self.assertIn('## ' + heading, self.text[folder.name])
        self.assertIn('## UART comparison trace', self.text[TRANSFER.name])
        self.assertIn('## Build and artifacts', self.text[TRANSFER.name])

    def test_local_web_anchors_exist(self):
        for name, text in self.text.items():
            for link in re.findall(r'\]\((https?://[^)]+)\)', text):
                parsed = urlparse(link)
                if parsed.netloc != 'released.github.io':
                    continue
                path = WEB / parsed.path.strip('/') / 'index.md'
                self.assertTrue(path.is_file(), (name, link))
                if parsed.fragment:
                    anchor = 'id="' + parsed.fragment + '"'
                    if anchor not in path.read_text(encoding='utf-8-sig'):
                        routes = json.loads((path.parent / 'guide-map.json').read_text(encoding='utf-8-sig'))
                        route = routes['aliases'][parsed.fragment]
                        chapter = (path.parent / route['page']).with_suffix('.md').resolve()
                        self.assertEqual(chapter.parent, path.parent.resolve())
                        self.assertIn('id="' + route['anchor'] + '"', chapter.read_text(encoding='utf-8-sig'), (name, link))

    def test_device_geometry_timer_and_current_capabilities(self):
        contract = (BOOT / 'image_metadata_config.h').read_text()
        values = dict(re.findall(r'#define\s+(IMAGE_\w+)\s+\((0x[0-9A-Fa-f]+)(?:U?L?)\)', contract))
        for folder in (BOOT, APP):
            tokens = {int(value, 16) for value in re.findall(r'0x[0-9A-Fa-f]+', self.text[folder.name])}
            self.assertIn(int(values['IMAGE_APPLICATION_START'], 16), tokens)
            self.assertIn(int(values['IMAGE_APPLICATION_CAPACITY'], 16), tokens)
            self.assertIn(0x2100, tokens)
            self.assertIn(0x2200, tokens)
            self.assertIn('252928', self.text[folder.name])
            self.assertIn('RFD', self.text[folder.name])
        irq = (APP / 'app_irq_handlers.h').read_text()
        self.assertRegex(irq, r'X\(0x2E,\s*Device_TimerIsr\)')
        self.assertIn('TAU0 channel 1', self.text[APP.name])
        self.assertIn('does not advertise read-only VERIFY', self.text[BOOT.name])
        self.assertNotIn('CAN_BOOT_CMD_VERIFY', (BOOT / 'can_boot_transport.h').read_text())
        self.assertIn('CAN_BOOT_DEBUG_UART=0U', self.text[BOOT.name])
        self.assertIn('F24_Boot_loader_CAN\\build.ps1 -Kind app -AppProject .', self.text[APP.name])

    def test_artifacts_and_runtime_match_sources(self):
        app = self.text[APP.name]
        package = (BOOT / 'tools/package_images.ps1').read_text(encoding='utf-8-sig')
        self.assertIn('flash_app_variable_CRC.hex', package)
        self.assertIn('flash_app_variable_CRC.hex', app)
        self.assertIn('app_build_report.html', app)
        for folder, keeper in ((TRANSFER, TRANSFER.name + '.hex'), (BOOT, 'boot0000_1FFF.hex'),
                               (APP, APP.name + '_update.bin')):
            self.assertIn('DefaultBuild/' + keeper, self.text[folder.name])
            self.assertIn('!/DefaultBuild/' + keeper, (folder / '.gitignore').read_text())
        runtime = (APP / 'custom_func.c').read_text()
        for term in ('boot-validated', 'CAN update: completed', 'log(timer)-'):
            self.assertIn(term, runtime)
            self.assertIn(term, app)
        self.assertIn('APP_RUNTIME_LOG_TAG', (APP / 'app_runtime_log.h').read_text())
        self.assertIn('Boot MOT/map are needed', self.text[BOOT.name])
        self.assertIn('all three projects', self.text[TRANSFER.name])
        protocol = (TRANSFER / 'can_transfer_protocol.c').read_text()
        self.assertIn('static uint8_t crc8_atm', protocol)
        self.assertIn('(crc << 1) ^ 0x07U', protocol)
        frames = re.findall(r'data=\[([0-9A-F ]+)\]', self.text[TRANSFER.name])
        self.assertEqual(len(frames), 2)
        for frame in frames:
            data = bytes.fromhex(frame)
            self.assertEqual(len(data), 8)
            crc = 0
            for value in data[:7]:
                crc ^= value
                for _ in range(8):
                    crc = ((crc << 1) ^ (0x07 if crc & 0x80 else 0)) & 0xFF
            self.assertEqual(crc, data[7])


if __name__ == '__main__':
    unittest.main(verbosity=2)
