"""Two owned CS+ products; shared board sources never form a third product."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / 'firmware'
BOARD = FIRMWARE / 'f24_board'
NAMES = ('RL78_F24_Bridge_boot', 'RL78_F24_Bridge_app')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree_hashes(folder):
    return {str(p.relative_to(folder)): digest(p)
            for p in folder.rglob('*') if p.is_file()}


class F24Layout(unittest.TestCase):
    def test_only_two_products_and_shared_sources(self):
        self.assertEqual(sorted(p.stem for p in FIRMWARE.rglob('*.mtpj')), sorted(NAMES))
        self.assertFalse((FIRMWARE/'RL78_F24_CAN_host_upgrade').exists())
        self.assertFalse(list(BOARD.rglob('*.mtpj')))
        for name in ('DefaultBuild', 'main.c', 'build_firmware.ps1',
                     'f24_firmware_version.generated.h'):
            self.assertFalse((BOARD/name).exists(), name)
        self.assertTrue((BOARD/'f24_board.scfg').is_file())
        self.assertTrue((FIRMWARE/'common/bridge_i2c.c').is_file())
        self.assertTrue((FIRMWARE/NAMES[1]/'host_link_protocol.c').is_file())

    def test_all_registered_paths_resolve_without_cross_product_dependency(self):
        for name in NAMES:
            folder = FIRMWARE/name
            project = ET.parse(folder/(name+'.mtpj'))
            for node in project.findall('.//Instance[Type="File"]/RelativePath'):
                path = Path(node.text)
                self.assertTrue((folder/path).is_file(), (name, node.text))
                self.assertNotIn('RL78_F24_CAN_host_upgrade', node.text)
                self.assertNotIn('tmp', path.parts)
                if name.endswith('boot'):
                    self.assertNotIn('RL78_F24_Bridge_app', node.text)
            for tag in ('COptionI-0', 'AsmOptionI-0'):
                for path in project.find('.//'+tag).text.splitlines():
                    self.assertTrue((folder/path).is_dir(), (name, path))
            self.assertEqual(project.find('.//SCFGFile').text, 'f24_board.scfg')
            self.assertEqual(Path(project.find('.//GenerateFileFolder').text).resolve(), BOARD)
            self.assertNotIn('RL78_F24_CAN_host_upgrade', (folder/(name+'.mtpj')).read_text())
            for node in project.iter():
                if node.tag.startswith('ProjectGeneratr-GeneratedFile'):
                    self.assertTrue(Path(node.text).is_file(), node.text)
        source = (BOARD/'host_link_uart.c').read_text()
        self.assertNotIn('inc_main.h', source)
        self.assertIn('f24_runtime_hooks.h', source)

    def test_scfg_device_and_generated_hardware_match(self):
        config = ET.parse(BOARD/'f24_board.scfg')
        self.assertEqual(config.find('.//platform').get('id'), 'R7F124FLJ3xFB')
        for name in NAMES:
            project = ET.parse(FIRMWARE/name/(name+'.mtpj'))
            self.assertEqual({n.text for n in project.findall('.//DeviceName')}, {'R7F124FLJ3xFB'})
        header = (BOARD/'src/smc_gen/r_bsp/mcu/rl78_f24/register_access/ccrl/iodefine.h').read_text()
        self.assertTrue('__R7F124FIODEFINE_HEADER__' in header)

    def test_preparation_is_idempotent_app_only_and_tracks_new_sc_sources(self):
        # Exercise reconciliation on an isolated source copy, never canonical SC.
        temporary = tempfile.TemporaryDirectory(prefix='f24_layout_', dir=ROOT/'tmp')
        folder = Path(temporary.name).resolve()
        self.assertEqual(folder.parent, (ROOT/'tmp').resolve())
        try:
            for name in (*NAMES, 'f24_board', 'common', 'f24_bridge_common'):
                shutil.copytree(FIRMWARE/name, folder/'firmware'/name,
                                ignore=shutil.ignore_patterns('DefaultBuild', '*.mtud', '*.rcpe'))
            scripts = folder/'scripts'
            scripts.mkdir()
            for name in ('prepare_f24_bridge_projects.ps1', 'prepare_f24_app_irq.ps1', 'f24_app_contract.ps1'):
                shutil.copy2(ROOT/'scripts'/name, scripts/name)

            def prepare(kind):
                result = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                    '-File', str(scripts/'prepare_f24_bridge_projects.ps1'), '-Kind', kind],
                    capture_output=True, timeout=90)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            prepare('all')
            before = tree_hashes(folder/'firmware')
            prepare('all')
            self.assertEqual(before, tree_hashes(folder/'firmware'), 'Preparation is not byte-idempotent')
            boot = tree_hashes(folder/'firmware'/NAMES[0])
            app = folder/'firmware'/NAMES[1]
            identity = (app/'app_image.json').read_bytes()
            mapping = (app/'app_irq_handlers.json').read_bytes()
            project = app/(NAMES[1]+'.mtpj')
            tree = ET.parse(project)
            tree.find('.//COptionD-0').text += '\nAPP_LOCAL_SENTINEL=1\n'
            tree.write(project, encoding='utf-8')
            extra = folder/'firmware/f24_board/src/smc_gen/fixture_new_driver'
            extra.mkdir()
            (extra/'new_driver.c').write_text('void new_driver(void) {}\n', encoding='ascii')
            (extra/'new_driver.h').write_text('void new_driver(void);\n', encoding='ascii')
            prepare('app')
            self.assertEqual(boot, tree_hashes(folder/'firmware'/NAMES[0]))
            self.assertEqual(identity, (app/'app_image.json').read_bytes())
            self.assertEqual(mapping, (app/'app_irq_handlers.json').read_bytes())
            text = project.read_text()
            self.assertIn('APP_LOCAL_SENTINEL=1', text)
            self.assertIn('fixture_new_driver\\new_driver.c', text)
            self.assertIn('fixture_new_driver', ET.parse(project).find('.//COptionI-0').text)
            (extra/'new_driver.c').unlink()
            prepare('app')
            self.assertNotIn('fixture_new_driver\\new_driver.c', project.read_text())
            self.assertEqual(boot, tree_hashes(folder/'firmware'/NAMES[0]))
        finally:
            self.assertEqual(folder.parent, (ROOT/'tmp').resolve())
            temporary.cleanup()


if __name__ == '__main__':
    unittest.main(verbosity=2)
