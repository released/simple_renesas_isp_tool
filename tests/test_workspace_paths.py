"""Public checkout layout and explicit integration-test dependencies."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
VARIABLES = ('FIRMWARE_F13_BOOT_PROJECT', 'FIRMWARE_F13_APP_PROJECT', 'FIRMWARE_WEB_ROOT')


class WorkspacePathsTests(unittest.TestCase):
    def test_default_siblings_and_environment_overrides(self):
        with tempfile.TemporaryDirectory(prefix='public_layout_') as folder:
            parent = Path(folder)
            tool = parent / 'simple_renesas_isp_tool'
            with patch.dict(os.environ, {name: '' for name in VARIABLES}):
                self.assertEqual(companion_path('F13Boot', tool), parent / 'RL78_F13_Boot_loader_CAN')
                self.assertEqual(companion_path('F13App', tool), parent / 'RL78_F13_Boot_loader_CAN_app')
                self.assertEqual(companion_path('Web', tool), parent)
                override = parent / 'other boot'
                os.environ['FIRMWARE_F13_BOOT_PROJECT'] = str(override)
                self.assertEqual(companion_path('F13Boot', tool), override)

    def test_powershell_default_override_and_explicit_precedence(self):
        with tempfile.TemporaryDirectory(prefix='public_ps_layout_') as folder:
            parent = Path(folder)
            env = dict(os.environ, **{name: '' for name in VARIABLES})
            helper = str(ROOT / 'scripts/workspace_paths.ps1').replace("'", "''")
            tool = str(parent / 'tool').replace("'", "''")
            code = ". '{}' ; Get-CompanionPath -Kind F13Boot -ToolRoot '{}'".format(helper, tool)
            def run(command, environment):
                result = subprocess.run(['powershell.exe', '-NoProfile', '-Command', command],
                                        env=environment, capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                return Path(result.stdout.decode().strip())
            self.assertEqual(run(code, env), parent / 'RL78_F13_Boot_loader_CAN')
            env['FIRMWARE_F13_BOOT_PROJECT'] = str(parent / 'override')
            self.assertEqual(run(code, env), parent / 'override')
            explicit = str(parent / 'explicit').replace("'", "''")
            self.assertEqual(run(code + " -ExplicitPath '{}'".format(explicit), env), parent / 'explicit')

    def test_no_private_paths_and_test_project_is_explicit(self):
        self.assertIn('*.pdf binary', (ROOT / '.gitattributes').read_text(encoding='utf-8-sig'))
        solution = (ROOT / 'FirmwareUpgradeTool.sln').read_text(encoding='utf-8-sig')
        test_guid = '{0BDEBA9F-7941-4B8C-B90F-C894159E2401}'
        self.assertIn(test_guid, solution)
        self.assertNotRegex(solution, test_guid.replace('{', r'\{').replace('}', r'\}') + r'.*Build\.0')
        project = (ROOT / 'tests/HostLinkProtocolTests.vcxproj').read_text(encoding='utf-8-sig')
        self.assertIn('ValidateF13TestSources', project)
        self.assertIn('FIRMWARE_F13_BOOT_PROJECT', project)
        for path in [ROOT / 'tests/can_block_tests.cpp', ROOT / 'tests/image_policy_tests.cpp',
                     ROOT / 'tests/f13_board_support_tests.c', ROOT / 'tests/HostLinkProtocolTests.vcxproj',
                     ROOT / 'scripts/workspace_paths.ps1', ROOT / 'scripts/verify_f13_artifacts.ps1']:
            self.assertNotRegex(path.read_text(encoding='utf-8-sig'), r'_Avery|[A-Z]:\\SourceCode|\.\./\.\./\.\./_')


if __name__ == '__main__':
    unittest.main(verbosity=2)
