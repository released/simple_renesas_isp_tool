"""Canonical operation figures, publication copies and inactivity guidance."""
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from PIL import Image
from pypdf import PdfReader
from workspace_paths import companion_path

ROOT=Path(__file__).resolve().parents[1]
WEB=companion_path('Web')
SOURCE=ROOT/'docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md'
SECTIONS=('TOOL_CONNECT_MEDIA','TOOL_DEVICE_MEDIA','TOOL_BRIDGE_MEDIA')

class OperationMedia(unittest.TestCase):
    def test_sixteen_public_copies_and_single_owner(self):
        source=SOURCE.read_text(encoding='utf-8-sig')
        page=(WEB/'slide_isp_tool_for_RL78/index.md').read_text(encoding='utf-8-sig')
        other=(WEB/'slide_boot_loader_for_RL78/index.md').read_text(encoding='utf-8-sig')
        images=[]
        for section in SECTIONS:
            pattern=r'<!-- '+section+r'_BEGIN -->.*?<!-- '+section+r'_END -->'
            blocks=re.findall(pattern,source,re.S)
            self.assertEqual(len(blocks),1)
            self.assertEqual(re.findall(pattern,page,re.S),blocks)
            self.assertNotIn(section,other)
            images+=re.findall(r'!\[[^\]]*\]\((images/tool_operation/[^)]+)\)',blocks[0])
        self.assertEqual(len(images),16)
        self.assertEqual(len(set(images)),16)
        for relative in images:
            local=ROOT/'docs'/relative
            public=WEB/'slide_isp_tool_for_RL78'/relative
            self.assertEqual(local.read_bytes(),public.read_bytes())
            original=ROOT/local.name
            if original.is_file():
                self.assertNotEqual(hashlib.sha256(local.read_bytes()).digest(),hashlib.sha256(original.read_bytes()).digest())
            with Image.open(local) as im:
                self.assertGreater(im.width,1500 if local.name=='MCU_F24_Device_app_log.png' else 1900)
                self.assertFalse(im.getexif())

    def test_timeout_contract_and_fresh_live_gate(self):
        text=SOURCE.read_text(encoding='utf-8-sig')
        for term in ('原 App 有效','尚未接受 START','30 秒','inactivity','刷新計時',
                     'START 已接受','重新 `Discover Target`','Device is no longer in Boot; update not started.'):
            self.assertIn(term,text)
        firmware=companion_path('F13Boot')
        self.assertRegex((firmware/'can_boot_config.h').read_text(),r'CAN_BOOT_INACTIVITY_TIMEOUT_MS\s+\(30000U\)')
        self.assertIn('if (started != 0U)',(firmware/'can_boot_transport.c').read_text())
        self.assertIn('Device is no longer in Boot; update not started.',(ROOT/'src/worker_can_workflows.cpp').read_text())
        self.assertIn('INVALID - HOST APP required',text)

    def test_success_report_operation_and_scope(self):
        text=SOURCE.read_text(encoding='utf-8-sig')
        device=text.split('<!-- TOOL_DEVICE_MEDIA_BEGIN -->',1)[1].split('<!-- TOOL_DEVICE_MEDIA_END -->',1)[0]
        bridge=text.split('<!-- TOOL_BRIDGE_MEDIA_BEGIN -->',1)[1].split('<!-- TOOL_BRIDGE_MEDIA_END -->',1)[0]
        for section in (device,bridge):
            self.assertIn('Open Last Report',section)
            self.assertIn('#update-html-report',section)
        report=(ROOT/'docs/UPDATE_VALIDATION_zh-TW.md').read_text(encoding='utf-8-sig')
        report=report.split('<a id="update-html-report"></a>',1)[1]
        for phrase in ('更新完成後開啟 Firmware update report','MESSAGE LOG','不需先按 Export Summary',
                       'Tool 不會自行開啟瀏覽器','Misc 寫入完成仍需明確 RUN',
                       'RUN 的確認另存一份報告','update_*.html','update_*.json',
                       '本次 Tool 執行期間最後成功儲存','重新開啟 Tool 後'):
            self.assertIn(phrase,report)
        self.assertIn('OnOpenLatestReport', (ROOT/'src/main_frame_evidence.cpp').read_text())

    def test_sync_validation_and_read_only_check(self):
        with tempfile.TemporaryDirectory(prefix='operation_media_') as folder:
            root=Path(folder)
            page=root/'slide_isp_tool_for_RL78/index.md'
            page.parent.mkdir()
            stub='prefix\n'+'\n'.join('<!-- '+s+'_BEGIN -->stale<!-- '+s+'_END -->' for s in SECTIONS)+'\nsuffix'
            page.write_text(stub,encoding='utf-8')
            cmd=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'scripts/sync_tool_operation_media.ps1'),'-WebRoot',str(root)]
            before=page.read_bytes()
            self.assertNotEqual(subprocess.run(cmd+['-CheckOnly'],capture_output=True).returncode,0)
            self.assertEqual(page.read_bytes(),before)
            self.assertFalse((page.parent/'images').exists())
            broken=stub.replace('<!-- TOOL_BRIDGE_MEDIA_END -->','')
            page.write_text(broken,encoding='utf-8')
            self.assertNotEqual(subprocess.run(cmd,capture_output=True).returncode,0)
            self.assertEqual(page.read_text(encoding='utf-8'),broken)
            self.assertFalse((page.parent/'images').exists())
            page.write_text(stub,encoding='utf-8')
            self.assertEqual(subprocess.run(cmd,capture_output=True).returncode,0)
            self.assertEqual(subprocess.run(cmd+['-CheckOnly'],capture_output=True).returncode,0)
            data=page.read_bytes()
            image=next((page.parent/'images/tool_operation').glob('*.jpg'))
            image.write_bytes(b'tampered')
            self.assertNotEqual(subprocess.run(cmd+['-CheckOnly'],capture_output=True).returncode,0)
            self.assertEqual(page.read_bytes(),data)

    def test_pdf_original_asset_root(self):
        spec=importlib.util.spec_from_file_location('operation_pdf',ROOT/'scripts/generate_f13_guide_pdf.py')
        module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.register_fonts()
        with tempfile.TemporaryDirectory(prefix='operation_pdf_') as folder:
            source=Path(folder)/'expanded.md'
            source.write_text('# Guide\n\n[![COM](images/tool_operation/tool_connect_COM.jpg)](images/tool_operation/tool_connect_COM.jpg)\n',encoding='utf-8')
            story=module.build_story(source,module.make_styles(),ROOT/'docs')
            self.assertTrue(any(hasattr(item,'_content') for item in story))
            output=Path(folder)/'guide.pdf'
            module.create_pdf(source,output,'Guide','Guide',ROOT/'docs')
            self.assertEqual(len(PdfReader(output).pages),1)
            with self.assertRaises(FileNotFoundError):
                module.build_story(source,module.make_styles())

if __name__=='__main__': unittest.main()
