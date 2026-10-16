"""Real-BIN documentation, complete address coverage and read-only sync checks."""
import importlib.util
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from workspace_paths import companion_path
from pypdf import PdfReader

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('flash_reference', ROOT / 'scripts/generate_flash_reference.py')
REFERENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REFERENCE)
DOC = ROOT / 'docs/FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md'
F13_BIN = companion_path('F13App') / 'DefaultBuild/RL78_F13_Boot_loader_CAN_app_update.bin'
F24_BIN = companion_path('F13Boot').parent / 'RL78_F24_Boot_loader_CAN_app/DefaultBuild/RL78_F24_Boot_loader_CAN_app_update.bin'
PATTERN = r'<!-- FLASH_REFERENCE_BEGIN -->.*?<!-- FLASH_REFERENCE_END -->'


class FlashReferenceTests(unittest.TestCase):
    def test_offline_pdf_contains_full_address_and_frame_reference(self):
        reader = PdfReader(DOC.with_suffix('.pdf'))
        self.assertGreater(len(reader.pages), 15)
        text = '\n'.join(page.extract_text() for page in reader.pages)
        for phrase in ['0x02100', '0x021FF', 'BLOCK_BEGIN', 'BLOCK_COMMIT',
                       'ReadLe32', 'CRC32', 'Get-DeviceMetadata']:
            self.assertIn(phrase, text)
        self.assertNotIn('FLASH_REFERENCE_BEGIN', text)
        self.assertNotIn('METADATA_BYTES_BEGIN', text)
        self.assertEqual(sum(len(page.images) for page in reader.pages), 0)

    def test_all_256_addresses_and_real_example_bytes(self):
        a, b = REFERENCE.read_image(F13_BIN, 'F13'), REFERENCE.read_image(F24_BIN, 'F24')
        text = DOC.read_text(encoding='utf-8-sig')
        rows = re.findall(r'(?m)^\| 0x([0-9A-F]{2}) \| 0x([0-9A-F]{5}) \| ([0-9A-F]{2}) \| ([^|]+) \| 0x([0-9A-F]{5}) \| ([0-9A-F]{2}) \| ([^|]+) \|$', text)
        self.assertEqual(len(rows), 256)
        self.assertEqual([int(row[0], 16) for row in rows], list(range(256)))
        for offset, row in enumerate(rows):
            self.assertEqual(int(row[1], 16), a['start'] + 0x100 + offset)
            self.assertEqual(int(row[2], 16), a['meta'][offset])
            self.assertEqual(int(row[4], 16), b['start'] + 0x100 + offset)
            self.assertEqual(int(row[5], 16), b['meta'][offset])
        for schema in [REFERENCE.F13, REFERENCE.F24]:
            self.assertEqual(len(REFERENCE.byte_fields(schema)), 256)
        with self.assertRaises(ValueError):
            REFERENCE.byte_fields(REFERENCE.F13 + [(0, 1, 'overlap', 'invalid')])
        self.assertEqual(a['start'], 0x2000)
        self.assertEqual(b['start'], 0x2000)
        self.assertIn('0x0210C..0x0210D', text)
        self.assertEqual(a['start']+0x100+0x18,0x2118)
        self.assertEqual(b['start']+0x100+0x1B,0x211B)
        self.assertIn('0x02118',text)
        self.assertIn('0x0211B',text)

    def test_generator_check_is_read_only_and_rejects_stale_or_missing_marker(self):
        command = ['python', str(ROOT / 'scripts/generate_flash_reference.py'),
                   '--f13-image', str(F13_BIN), '--f24-image', str(F24_BIN)]
        self.assertEqual(subprocess.run(command + ['--check'], capture_output=True).returncode, 0)
        with tempfile.TemporaryDirectory(prefix='flash_reference_') as folder:
            fixture = Path(folder) / 'guide.md'
            fixture.write_text('prefix\n<!-- METADATA_BYTES_BEGIN -->stale<!-- METADATA_BYTES_END -->\nsuffix', encoding='utf-8')
            before = fixture.read_bytes()
            self.assertNotEqual(subprocess.run(command + ['--guide', str(fixture), '--check'], capture_output=True).returncode, 0)
            self.assertEqual(fixture.read_bytes(), before)
            self.assertEqual(subprocess.run(command + ['--guide', str(fixture)], capture_output=True).returncode, 0)
            self.assertTrue(fixture.read_text(encoding='utf-8').startswith('prefix\n'))
            self.assertTrue(fixture.read_text(encoding='utf-8').endswith('\nsuffix'))
            fixture.write_text('no markers', encoding='utf-8')
            self.assertNotEqual(subprocess.run(command + ['--guide', str(fixture)], capture_output=True).returncode, 0)
            self.assertEqual(fixture.read_text(encoding='utf-8'), 'no markers')

    def test_invalid_example_image_is_not_documented(self):
        with tempfile.TemporaryDirectory(prefix='flash_bad_bin_') as folder:
            fixture = Path(folder) / 'bad.bin'
            image = bytearray(F13_BIN.read_bytes())
            image[-1] ^= 1
            fixture.write_bytes(image)
            with self.assertRaisesRegex(ValueError, 'CRC mismatch'):
                REFERENCE.read_image(fixture, 'F13')
            image = bytearray(F13_BIN.read_bytes())
            image[0x180] = 0
            fixture.write_bytes(image)
            with self.assertRaisesRegex(ValueError, 'reserved region'):
                REFERENCE.read_image(fixture, 'F13')

    def test_web_sync_and_protocol_examples(self):
        text = DOC.read_text(encoding='utf-8-sig')
        page = companion_path('Web') / 'slide_boot_loader_for_RL78/index.md'
        self.assertEqual(re.findall(PATTERN, text, re.S), re.findall(PATTERN, page.read_text(encoding='utf-8-sig'), re.S))
        for phrase in ['Byte0..Byte7指CAN Data payload', 'App只處理INFO／ENTER',
                       'CAN控制器鏈路CRC', '0x720..0x727', '128個reserved FF',
                       'F24 (MCU CAN device)', 'Get-DeviceMetadata', 'Client::StageMetadata／SendBlock']:
            self.assertIn(phrase, text)
        for name in ['HELLO', 'START', 'END', 'ABORT', 'ENTER', 'INFO', 'METADATA', 'SEAL',
                     'METADATA_RESET', 'BLOCK_BEGIN', 'BLOCK_COMMIT']:
            self.assertRegex(text, r'(?m)^\| ' + name + r' \| [128] \|')
        ack = bytes.fromhex('0C 00 00 00 40 20 00 00')
        self.assertEqual(int.from_bytes(ack[4:], 'little'), 0x2040)
        info = bytes.fromhex('07 00 00 02 3B F1 05 06')
        self.assertEqual((int.from_bytes(info[4:6], 'little'), info[6], info[7]), (0xF13B, 5, 6))
        with tempfile.TemporaryDirectory(prefix='flash_sync_') as folder:
            target = Path(folder) / 'slide_boot_loader_for_RL78/index.md'
            target.parent.mkdir()
            target.write_text('prefix\n<!-- FLASH_REFERENCE_BEGIN -->stale<!-- FLASH_REFERENCE_END -->\nsuffix', encoding='utf-8')
            command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                       str(ROOT / 'scripts/sync_flash_reference.ps1'), '-WebRoot', folder]
            before = target.read_bytes()
            self.assertNotEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertEqual(target.read_bytes(), before)
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertTrue(target.read_text(encoding='utf-8').startswith('prefix\n'))
            self.assertTrue(target.read_text(encoding='utf-8').endswith('\nsuffix'))
            target.write_text(target.read_text(encoding='utf-8') + '<!-- FLASH_REFERENCE_BEGIN --><!-- FLASH_REFERENCE_END -->', encoding='utf-8')
            before = target.read_bytes()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(target.read_bytes(), before)

    def test_document_c_example_compiles_as_c90(self):
        candidates = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))
        if not candidates:
            self.skipTest('MSVC C compiler not installed')
        code, = re.findall(r'```c\n(.*?)\n```', DOC.read_text(encoding='utf-8-sig'), re.S)
        code += '\nint main(void) { unsigned char a[128] = {0}; a[0x1D] = 0x20; return ExampleDeviceAppStart(a) != 0x2000UL; }\n'
        with tempfile.TemporaryDirectory(prefix='flash_c90_') as folder:
            source = Path(folder) / 'reference.c'
            source.write_text(code, encoding='ascii')
            result = subprocess.run([str(candidates[-1]), '/nologo', '/TC', '/Za', '/W4', '/WX', '/c',
                                     '/Fo' + str(Path(folder) / 'reference.obj'), str(source)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main(verbosity=2)
