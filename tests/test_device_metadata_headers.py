"""Independent image/CAN/handoff contracts; no device or production writes."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from workspace_paths import companion_path

F13 = companion_path('F13Boot')
F24 = F13.parent / 'RL78_F24_Boot_loader_CAN'
HEADERS = ('image_metadata_config.h', 'can_boot_config.h', 'boot_app_handoff.h')


class DeviceMetadataHeaders(unittest.TestCase):
    def test_header_ownership_and_project_visibility(self):
        names = []
        for boot in (F13, F24):
            image, can, handoff = [(boot / name).read_text(encoding='utf-8-sig') for name in HEADERS]
            self.assertNotRegex(image, r'(?m)^\s*#include|\bCAN_BOOT_')
            self.assertNotRegex(can, r'(?m)^\s*#include|^#define\s+(IMAGE_|BOOT_APP_)')
            self.assertNotRegex(handoff, r'(?m)^\s*#include|\bCAN_BOOT_')
            names.append(set(re.findall(r'^#define (IMAGE_\w+)', image, re.M)))
            self.assertIn('IMAGE_BOOT_VERSION', image)
            self.assertIn('IMAGE_METADATA_REGION_SIZE', image)
            for project in (boot, boot.parent / (boot.name + '_app')):
                xml = ET.parse(next(project.glob('*.mtpj')))
                registered = [node.text.replace('\\', '/') for node in xml.findall('.//Instance/RelativePath') if node.text]
                self.assertTrue(any(name.endswith('/image_metadata_config.h') or name == 'image_metadata_config.h' for name in registered))
                self.assertTrue(any(name.endswith('/boot_app_handoff.h') or name == 'boot_app_handoff.h' for name in registered))
        self.assertEqual(names[0], names[1])

    def test_contract_readers_reject_missing_duplicate_or_computed_values(self):
        for boot in (F13, F24):
            with self.subTest(boot=boot.name), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                tools = root if boot == F13 else root / 'tools'
                tools.mkdir(exist_ok=True)
                for name in HEADERS:
                    shutil.copyfile(boot / name, root / name)
                shutil.copyfile(boot / ('app_config.ps1' if boot == F13 else 'tools/app_config.ps1'), tools / 'app_config.ps1')
                script = 'generate_irq_contract.ps1' if boot == F13 else 'device_common.ps1'
                shutil.copyfile(boot / (script if boot == F13 else 'tools/' + script), tools / script)
                args = ['powershell.exe', '-NoProfile', '-Command',
                        f"$ErrorActionPreference='Stop'; . '{tools / script}'" +
                        (' -Mode Boot -CheckOnly' if boot == F13 else '; Get-DeviceMacro IMAGE_TARGET_ID | Out-Null')]
                # F13 CheckOnly needs its expected vector file but must not write it.
                if boot == F13:
                    shutil.copyfile(boot / 'f13_boot_vectors.asm', root / 'f13_boot_vectors.asm')
                self.assertEqual(subprocess.run(args, capture_output=True).returncode, 0)
                header = root / 'image_metadata_config.h'
                original = header.read_text()
                macro = 'IMAGE_APPLICATION_START' if boot == F13 else 'IMAGE_TARGET_ID'
                line = re.search(r'^#define ' + macro + r'[^\n]*', original, re.M).group()
                for altered in (original.replace(line, ''), original + '\n' + line,
                                original.replace(line, '#define ' + macro + ' (1U + 2U)')):
                    header.write_text(altered, encoding='utf-8')
                    snapshot = {p: p.read_bytes() for p in root.rglob('*') if p.is_file()}
                    result = subprocess.run(args, capture_output=True)
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual({p: p.read_bytes() for p in root.rglob('*') if p.is_file()}, snapshot)


if __name__ == '__main__':
    unittest.main()
