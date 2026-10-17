"""Golden header syntax and standalone reader checks; no build/device writes."""
from pathlib import Path
import hashlib
import subprocess
import tempfile
import unittest
from app_config_fixtures import image_header, irq_header
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
BOOT = companion_path('F13Boot')
F24 = BOOT.parent / 'RL78_F24_Boot_loader_CAN'


class DeviceCHeaders(unittest.TestCase):
    def test_golden_readers_headers_and_no_json_inputs(self):
        content = (ROOT / 'scripts/device_app_config.ps1').read_bytes()
        for reader in (BOOT / 'app_config.ps1', F24 / 'tools/app_config.ps1'):
            self.assertEqual(reader.read_bytes(), content)
        for app in (BOOT.parent / 'RL78_F13_Boot_loader_CAN_app', F24.parent / 'RL78_F24_Boot_loader_CAN_app'):
            self.assertFalse((app / 'app_image.json').exists())
            self.assertFalse((app / 'app_irq_handlers.json').exists())
            self.assertTrue((app / 'app_image_config.h').exists())
            self.assertTrue((app / 'app_irq_handlers.h').exists())
        policy = (ROOT / 'docs/DEVICE_GOLDEN_SAMPLE_zh-TW.md').read_text(encoding='utf-8-sig')
        for text in ('F13', 'F24', 'golden', 'A／B', 'app_image_config.h', 'app_irq_handlers.h', '不依賴', 'Python'):
            self.assertIn(text.lower(), policy.lower())

    def test_strict_parser_preprocessing_duplicate_and_empty_list_cases(self):
        with tempfile.TemporaryDirectory(prefix='device_c_config_') as temporary:
            folder = Path(temporary)
            config = folder / 'app_image_config.h'
            irq = folder / 'app_irq_handlers.h'
            valid = image_header(dict(version='0x00010600', build=20261017, target='0xF13B', role='DEVICE_APP', transport='CAN'))
            config.write_text(valid, encoding='utf-8')
            irq.write_text(irq_header({}), encoding='utf-8')
            command = f". '{BOOT / 'app_config.ps1'}'; $i=Read-AppImageConfig '{folder}' 0xF13B; $m=Read-AppIrqConfig '{folder}'; Write-Output ($i.Version.ToString()+','+$m.Count)"
            args = ['powershell.exe', '-NoProfile', '-Command', command]
            before = hashlib.sha256(config.read_bytes()).digest()
            self.assertEqual(subprocess.run(args, capture_output=True).returncode, 0)
            self.assertEqual(hashlib.sha256(config.read_bytes()).digest(), before)
            cases = [valid.replace('#endif', '#define APP_IMAGE_VERSION 1U\n#endif'),
                     valid.replace('#endif', '#if 0\n#endif\n#endif'),
                     valid.replace('0x00010600UL', '1U + 2U'),
                     valid.replace('20261017', '020261017'),
                     valid.replace('20261017', '4294967296'),
                     valid.replace('#endif', '#include "other.h"\n#endif'),
                     valid.replace('#endif', '/* unclosed\n#endif')]
            for text in cases:
                with self.subTest(text=text):
                    config.write_text(text, encoding='utf-8')
                    snapshot = config.read_bytes()
                    self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
                    self.assertEqual(config.read_bytes(), snapshot)
            config.write_text(valid, encoding='utf-8')
            for entries in ('X(0x08, handler) X(8U, other)', 'X(0x7E, handler)', 'X(0x08, bad-name)', 'X(0x08, handler) + 1'):
                irq.write_text('#ifndef APP_IRQ_HANDLERS_H\n#define APP_IRQ_HANDLERS_H\n#define APP_IRQ_HANDLERS(X) '+entries+'\n#endif\n', encoding='utf-8')
                self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)


if __name__ == '__main__':
    unittest.main()
