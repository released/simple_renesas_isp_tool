"""App command ownership must reach the real helper through Host Link dispatch."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'firmware/RL78_F24_Bridge_app'


class AppCommands(unittest.TestCase):
    def test_only_app_owned_commands_are_routed(self):
        text = (APP/'host_link_protocol.c').read_text()
        group = re.search(r'((?:\s*case BU_\w+_COMMAND:)+)\s*F24Bridge_AppCommand\(', text)
        self.assertIsNotNone(group)
        self.assertEqual(set(re.findall(r'case (BU_\w+_COMMAND):', group[1])),
                         {'BU_INFO_COMMAND', 'BU_ENTER_COMMAND', 'BU_READ_METADATA_COMMAND', 'BU_DIAGNOSTICS_COMMAND'})

    def test_readback_is_required_for_valid_installed_app(self):
        text = (ROOT/'src/core/bridge_update_profile.cpp').read_text()
        self.assertRegex(text, r'if \(info->flags&BU_FLAG_VALID_APP\).*?\n'
                         r'\s*std::vector.*?\n\s*if \(!ReadMetadata\(link,&region,error\)\) return false;')
        boot = (ROOT/'firmware/RL78_F24_Bridge_boot/main.c').read_text()
        self.assertIn('(P3&0x04U)', boot)


if __name__ == '__main__':
    unittest.main(verbosity=2)
