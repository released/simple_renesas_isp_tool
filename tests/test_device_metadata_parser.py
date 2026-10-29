"""Common C90 module portability and project boundaries; no hardware writes."""
from pathlib import Path
import shutil
import os
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
F13 = companion_path('F13Boot')
F24 = F13.parent / 'RL78_F24_Boot_loader_CAN'
COMMON = ROOT / 'firmware/common/device_metadata'


class DeviceMetadataParser(unittest.TestCase):
    def test_common_c90_module_with_three_profiles_and_two_diagnostic_modes(self):
        compilers = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers, 'MSVC C90 compiler is required for this development test')
        bootstrap = compilers[-1].parents[7] / 'Common7/Tools/VsDevCmd.bat'
        setup = subprocess.run(f'cmd.exe /d /c call "{bootstrap}" -no_logo -arch=amd64 >nul && set', capture_output=True, text=True)
        self.assertEqual(setup.returncode, 0, setup.stdout + setup.stderr)
        environment = os.environ.copy()
        for line in setup.stdout.splitlines():
            if '=' in line and not line.startswith('='):
                key, value = line.split('=', 1)
                environment[key] = value
        with tempfile.TemporaryDirectory(prefix='metadata_c90_') as temporary:
            folder = Path(temporary)
            for name in ('image_metadata.c', 'image_metadata.h'):
                shutil.copyfile(COMMON / name, folder / name)
            shutil.copyfile(ROOT / 'tests/device_metadata_parser_test.c', folder / 'test.c')
            for detail in (0, 1):
                (folder / 'image_metadata_config.h').write_text(
                    '#define IMAGE_FORMAT_VERSION (6U)\n'
                    '#define IMAGE_MANIFEST_SIZE (128U)\n'
                    '#define IMAGE_METADATA_REGION_SIZE (256U)\n'
                    '#define IMAGE_MINIMUM_IMAGE_SIZE (516UL)\n'
                    '#define IMAGE_APPLICATION_CAPACITY (122880UL)\n'
                    '#define IMAGE_BOOT_VERSION (0x00010700UL)\n'
                    f'#define IMAGE_METADATA_DETAILED_STATUS ({detail}U)\n', encoding='utf-8')
                for transport in (1, 2, 4):
                    args = [str(compilers[-1]), '/nologo', '/TC', '/Za', '/W4', '/WX',
                            f'/DTEST_TRANSPORT={transport}UL', 'test.c', 'image_metadata.c', '/Fe:test.exe']
                    result = subprocess.run(args, cwd=folder, capture_output=True, env=environment)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    result = subprocess.run([str(folder / 'test.exe')], cwd=folder, capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn(b'PASS:', result.stdout)
                    # Exercise the CC-RL little-endian/ROM-qualified source path
                    # on a host; this is not MCU instruction/hardware testing.
                    modeled = args[:1] + ['/D__CCRL__', '/D__far='] + args[1:]
                    result = subprocess.run(modeled, cwd=folder, capture_output=True, env=environment)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    result = subprocess.run([str(folder / 'test.exe')], cwd=folder, capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            config = folder / 'image_metadata_config.h'
            valid = config.read_text()
            for old, new in (('FORMAT_VERSION (6U)', 'FORMAT_VERSION (5U)'),
                             ('MANIFEST_SIZE (128U)', 'MANIFEST_SIZE (96U)'),
                             ('REGION_SIZE (256U)', 'REGION_SIZE (128U)'),
                             ('DETAILED_STATUS (1U)', 'DETAILED_STATUS (2U)')):
                self.assertIn(old, valid)
                config.write_text(valid.replace(old, new), encoding='utf-8')
                result = subprocess.run([str(compilers[-1]), '/nologo', '/TC', '/Za', '/c', 'image_metadata.c'],
                                        cwd=folder, capture_output=True, env=environment)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_identical_local_modules_and_explicit_project_registration(self):
        for boot in (F13, F24):
            for name in ('image_metadata.c', 'image_metadata.h'):
                self.assertEqual((boot / name).read_text(), (COMMON / name).read_text())
            core = (boot / 'image_metadata.c').read_text()
            for forbidden in ('can_boot_config.h', 'CAN_BOOT_', 'Device_', 'R_RFD_', 'FSL_', 'g_metadata_words'):
                self.assertNotIn(forbidden, core)
            self.assertNotIn('#include "image_metadata.c"', (boot / 'can_boot_transport.c').read_text())
            for project in (boot, boot.parent / (boot.name + '_app')):
                tree = ET.parse(next(project.glob('*.mtpj')))
                names = [Path(node.text.replace('\\', '/')).name for node in tree.findall('.//Instance/RelativePath') if node.text]
                for source in ('image_metadata.c', 'image_metadata.h', 'image_metadata_profile.c'):
                    self.assertEqual(names.count(source), 1, (project.name, source))
            self.assertIn('ImageMetadataValidate(', (boot / 'custom_func.c').read_text())
            app = boot.parent / (boot.name + '_app')
            self.assertIn('ImageMetadataRead32(', (app / 'custom_func.c').read_text())

    def test_sync_check_only_refuses_drift_without_mutation(self):
        with tempfile.TemporaryDirectory(prefix='metadata_sync_') as temporary:
            projects = [Path(temporary) / name for name in ('f13', 'f24')]
            for project in projects:
                project.mkdir()
                for name in ('image_metadata.c', 'image_metadata.h'):
                    shutil.copyfile(COMMON / name, project / name)
            args = ['powershell.exe', '-NoProfile', '-File', str(ROOT / 'scripts/sync_device_metadata.ps1'),
                    '-F13Boot', str(projects[0]), '-F24Boot', str(projects[1]), '-CheckOnly']
            self.assertEqual(subprocess.run(args, capture_output=True).returncode, 0)
            path = projects[1] / 'image_metadata.h'
            path.write_text('deliberate drift', encoding='utf-8')
            before = {p: p.read_bytes() for p in Path(temporary).rglob('*') if p.is_file()}
            self.assertNotEqual(subprocess.run(args, capture_output=True).returncode, 0)
            self.assertEqual({p: p.read_bytes() for p in Path(temporary).rglob('*') if p.is_file()}, before)


if __name__ == '__main__':
    unittest.main()
