"""Source ownership checks; native tests execute RichEdit and geometry."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
def read(path): return (ROOT/path).read_text(encoding='utf-8-sig')

class ClassicConsole(unittest.TestCase):
    def test_follow_default_and_ini_roundtrip(self):
        self.assertIn('bool log_auto_scroll_ = true;',read('src/main_frame.h'))
        settings=read('src/main_frame_settings.cpp')
        self.assertIn('ReadIniValue(L"log_auto_scroll",L"1")!=L"0"',settings)
        self.assertIn('WriteIniValue(L"log_auto_scroll",log_auto_scroll_ ? L"1" : L"0")',settings)
        handler=settings.split('void CMainFrame::OnLogAutoScroll()')[1]
        self.assertIn('ScrollToEnd(',handler)
        self.assertIn('SaveUiSettings()',handler)

    def test_append_does_not_jump_when_follow_disabled(self):
        helper=read('src/ui/log_view.cpp')
        for message in ('EM_EXGETSEL','EM_GETSCROLLPOS','EM_EXSETSEL','EM_SETSCROLLPOS','EM_SCROLLCARET','SB_BOTTOM'):
            self.assertIn(message,helper)
        self.assertLess(helper.index('EM_GETSCROLLPOS'),helper.index('EM_REPLACESEL'))
        restore=helper.split('if (follow) ScrollToEnd(window);')[1]
        self.assertIn('EM_EXSETSEL',restore)
        self.assertIn('EM_SETSCROLLPOS',restore)
        frame=read('src/main_frame.cpp').split('void CMainFrame::AppendLog(')[1].split('std::wstring CMainFrame::BuildSessionSummary')[0]
        self.assertIn('log_view::Append(',frame)
        self.assertNotIn('SetSel(',frame)
        self.assertIn('fwrite(',frame)
        self.assertIn('fflush(',frame)

    def test_all_fields_in_four_semantic_groups(self):
        layout=read('src/main_frame_layout.cpp')
        groups=layout.split('const FirmwareFieldIndex groups[4][5]=')[1].split(';',1)[0]
        fields=re.findall(r'kFirmwareField\w+',groups)
        self.assertEqual(len(fields),20)
        self.assertEqual(len(set(fields)),20)
        self.assertIn('layout.metadata_panes[group]',layout)
        self.assertIn('MinimumClientHeight(metric.tmHeight)',layout)
        self.assertIn('Move(log_auto_scroll_check_,log_tools.auto_scroll)',layout)

    def test_classic_system_colors_and_five_groups(self):
        theme=read('src/main_frame_status.cpp')
        for title in ('1. HOST CONNECTION','2. BIN / DEVICE INFORMATION','3. PERIPHERAL / TARGET',
                      '4. LOAD / UPDATE IMAGE','5. MESSAGE LOG'):
            self.assertIn(title,theme)
        self.assertIn('CPen pen(PS_SOLID,2,GetSysColor(COLOR_3DSHADOW))',theme)
        self.assertIn('OnSysColorChange()',theme)
        self.assertNotIn('RGB(245,247,251)',theme)
        self.assertIn('GetSysColor(COLOR_WINDOW)',read('src/ui/status_card.cpp'))
        self.assertIn('SetInfoStyle()',read('src/main_frame_overview.cpp'))

if __name__=='__main__': unittest.main()
