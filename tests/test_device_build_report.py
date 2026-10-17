"""Real-BIN offline HTML summaries; no compiler or MCU writes."""
from pathlib import Path
from html.parser import HTMLParser
import hashlib
import shutil
import struct
import subprocess
import tempfile
import unittest
import zlib
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
BOOT = companion_path('F13Boot')
F24 = BOOT.parent / 'RL78_F24_Boot_loader_CAN'


class Tables(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tables = []
        self.row = None
        self.cell = None

    def handle_starttag(self, tag, attrs):
        if tag == 'table':
            self.tables.append([])
        elif tag == 'tr':
            self.row = []
        elif tag == 'td':
            self.cell = ''

    def handle_data(self, data):
        if self.cell is not None:
            self.cell += data

    def handle_endtag(self, tag):
        if tag == 'td':
            self.row.append(self.cell)
            self.cell = None
        elif tag == 'tr' and self.row:
            self.tables[-1].append(self.row)
            self.row = None


class DeviceBuildReport(unittest.TestCase):
    def invoke(self, app, image):
        command = (f"$ErrorActionPreference='Stop'; . '{ROOT / 'scripts/device_app_config.ps1'}'; "
                   f". '{ROOT / 'scripts/device_app_report.ps1'}'; "
                   f"Write-AppBuildReport -Project '{app}' -ImagePath '{image}'")
        return subprocess.run(['powershell.exe', '-NoProfile', '-Command', command],
                              capture_output=True, text=True)

    def fixture(self, boot, base):
        original = boot.parent / (boot.name + '_app')
        app = base / 'App & build'
        app.mkdir()
        for name in ('app_image_config.h', 'app_irq_handlers.h', 'iodefine.h'):
            shutil.copyfile(original / name, app / name)
        image = app / 'sample_update.bin'
        image.write_bytes((original / 'DefaultBuild' / (original.name + '_update.bin')).read_bytes())
        return app, image

    def test_identical_standalone_helpers_and_build_hooks(self):
        expected = (ROOT / 'scripts/device_app_report.ps1').read_bytes()
        self.assertEqual((BOOT / 'app_build_report.ps1').read_bytes(), expected)
        self.assertEqual((F24 / 'tools/app_build_report.ps1').read_bytes(), expected)
        for path in (BOOT.parent / (BOOT.name + '_app') / 'generate_update_image.ps1',
                     F24 / 'tools/package_images.ps1'):
            self.assertIn('Write-AppBuildReport', path.read_text(encoding='utf-8-sig'))

    def test_both_real_bins_all_bytes_and_all_irq_slots(self):
        for boot in (BOOT, F24):
            with self.subTest(boot=boot), tempfile.TemporaryDirectory() as temporary:
                app, image = self.fixture(boot, Path(temporary))
                data = image.read_bytes()
                result = self.invoke(app, image)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                report = (app / 'app_build_report.html').read_text(encoding='utf-8')
                self.assertIn(hashlib.sha256(data).hexdigest().upper(), report)
                self.assertIn('App &amp; build', report)
                self.assertNotIn(str(app), report)
                self.assertNotIn('<script', report)
                self.assertNotIn('<link', report)
                tables = Tables()
                tables.feed(report)
                self.assertEqual(len(tables.tables), 3)
                fields, irqs, rows = tables.tables
                self.assertEqual(len(irqs), 64)
                self.assertEqual(len(rows), 256)
                start = struct.unpack_from('<I', data, 0x11c)[0]
                for offset, row in enumerate(rows):
                    self.assertEqual(row[:4], [f'0x{offset:02X}', f'0x{offset+256:03X}',
                                              f'0x{start+256+offset:05X}', f'{data[256+offset]:02X}'])
                for index, row in enumerate(irqs):
                    self.assertEqual(row[0], f'0x{index*2:02X}')
                    self.assertEqual(row[2], f'0x{start+index*4:05X}')
                    self.assertEqual(row[3], data[index*4:index*4+4].hex(' ').upper())
                self.assertEqual(image.read_bytes(), data)

    def test_invalid_bin_or_stale_header_cannot_produce_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            app, image = self.fixture(BOOT, Path(temporary))
            original = image.read_bytes()
            malformed = []
            malformed.append(original[:-1])
            data = bytearray(original)
            data[-1] ^= 1
            malformed.append(data)
            for offset in (0x100, 0x118, 0x170, 0x180):
                data = bytearray(original)
                data[offset] ^= 1
                struct.pack_into('<I', data, len(data)-4, zlib.crc32(data[:-4]))
                malformed.append(data)
            for data in malformed:
                image.write_bytes(data)
                result = self.invoke(app, image)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((app / 'app_build_report.html').exists())
            image.write_bytes(original)
            header = app / 'app_image_config.h'
            header.write_text(header.read_text().replace('20261017UL', '20261018UL'))
            self.assertNotEqual(self.invoke(app, image).returncode, 0)
            self.assertFalse((app / 'app_build_report.html').exists())

    def test_metadata_text_is_html_escaped(self):
        with tempfile.TemporaryDirectory() as temporary:
            app, image = self.fixture(BOOT, Path(temporary))
            data = bytearray(image.read_bytes())
            data[0x140:0x150] = b'<tag>&"\x00'.ljust(16, b'\x00')
            struct.pack_into('<I', data, len(data)-4, zlib.crc32(data[:-4]))
            image.write_bytes(data)
            self.assertEqual(self.invoke(app, image).returncode, 0)
            text = (app / 'app_build_report.html').read_text(encoding='utf-8')
            self.assertIn('&lt;tag&gt;&amp;&quot;', text)
            self.assertNotIn('<tag>', text)


if __name__ == '__main__':
    unittest.main()
