"""Guard the synchronous EN_CHANGE path before the INI path is initialized."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SettingsStartup(unittest.TestCase):
    def test_loading_guard_is_active_from_construction(self):
        header = (ROOT/'src/main_frame.h').read_text()
        self.assertIn('bool loading_ui_settings_ = true;', header)
        bus = (ROOT/'src/main_frame_bus.cpp').read_text()
        self.assertIn('i2c_address_edit_.SetWindowTextW(L"0x09")', bus)
        self.assertRegex(bus, r'void CMainFrame::OnI2cSettingsChanged\(\) \{\s*if \(loading_ui_settings_')
        frame = (ROOT/'src/main_frame.cpp').read_text()
        self.assertLess(frame.index('InitializeBusUi()'), frame.index('InitializeSettingsFile();'))
        self.assertRegex(frame, r'void CMainFrame::OnEnChangeApplicationStart\(\) \{\s*if \(loading_ui_settings_\) return;')

    def test_profile_writes_never_use_empty_filename(self):
        source = (ROOT/'src/main_frame_settings.cpp').read_text()
        writer = source.split('void CMainFrame::WriteIniValue(')[1].split('void CMainFrame::ApplyComboSelection')[0]
        self.assertLess(writer.index('if (ini_path_.empty()) return;'), writer.index('WritePrivateProfileStringW('))
        self.assertIn('const DWORD error = GetLastError();', writer)
        self.assertIn('ini_path_ +', writer)
        save = source.split('void CMainFrame::SaveUiSettings()')[1]
        self.assertRegex(save, r'^\s*\{\s*if \(loading_ui_settings_ \|\| ini_path_.empty\(\)\) return;')

    def test_loading_is_released_only_after_restore(self):
        source = (ROOT/'src/main_frame_settings.cpp').read_text()
        load = source.split('void CMainFrame::LoadUiSettings()')[1].split('void CMainFrame::SaveUiSettings()')[0]
        self.assertLess(load.index('loading_ui_settings_ = true;'), load.index('ReadIniValue('))
        self.assertLess(load.index('LoadBinaryFromPath('), load.index('loading_ui_settings_ = false;'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
