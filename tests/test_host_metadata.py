"""Portable Host metadata module and separate engineering contracts."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
COMMON = ROOT / 'firmware/common'
PROFILE = ROOT / 'firmware/f24_bridge_common'


class HostMetadata(unittest.TestCase):
    def test_strict_c90_metadata_and_profile_without_hardware(self):
        compilers = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers)
        bootstrap = compilers[-1].parents[7] / 'Common7/Tools/VsDevCmd.bat'
        setup = subprocess.run(f'cmd.exe /d /c call "{bootstrap}" -no_logo -arch=amd64 >nul && set',capture_output=True,text=True)
        self.assertEqual(setup.returncode,0,setup.stdout+setup.stderr)
        environment=os.environ.copy()
        for line in setup.stdout.splitlines():
            if '=' in line and not line.startswith('='):
                key,value=line.split('=',1); environment[key]=value
        with tempfile.TemporaryDirectory(prefix='host_metadata_') as temporary:
            args=[str(compilers[-1]),'/nologo','/TC','/Za','/W4','/WX',
                '/I'+str(COMMON),'/I'+str(PROFILE),str(COMMON/'bridge_image_metadata.c'),
                str(PROFILE/'image_metadata_profile.c'),str(ROOT/'tests/bridge_metadata_test.c'),'/Fetest.exe']
            result=subprocess.run(args,cwd=temporary,capture_output=True,env=environment)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(Path(temporary)/'test.exe')],capture_output=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_separate_metadata_transport_handoff_and_project_registration(self):
        core=(COMMON/'bridge_image_metadata.c').read_text()
        header=(COMMON/'bridge_image_metadata.h').read_text()
        for forbidden in ('bridge_update_contract.h','platform.h','HostLink','R_RFD_','BU_BEGIN_COMMAND'):
            self.assertNotIn(forbidden,core+header)
        self.assertNotIn('F24_BRIDGE_COOKIE_ADDRESS',(PROFILE/'image_metadata_config.h').read_text())
        self.assertIn('F24_BRIDGE_COOKIE_ADDRESS',(PROFILE/'boot_app_handoff.h').read_text())
        self.assertFalse((PROFILE/'f24_bridge_layout.h').exists())
        for kind in ('boot','app'):
            folder=ROOT/('firmware/RL78_F24_Bridge_'+kind)
            names=[Path(n.text.replace('\\','/')).name for n in ET.parse(next(folder.glob('*.mtpj'))).findall('.//Instance/RelativePath') if n.text]
            for name in ('bridge_image_metadata.c','bridge_image_metadata.h','image_metadata_profile.c','image_metadata_config.h','boot_app_handoff.h'):
                self.assertEqual(names.count(name),1,(kind,name))
            if kind=='app':
                for name in ('app_image_config.h','app_irq_handlers.h'):
                    self.assertTrue((folder/name).is_file()); self.assertEqual(names.count(name),1)
                for name in ('app_image.json','app_irq_handlers.json'):
                    self.assertFalse((folder/name).exists())
        self.assertNotIn('#include "bridge_image_metadata.c"',core)
        protocol=(ROOT/'firmware/RL78_F24_Bridge_app/host_link_protocol.c').read_text()
        self.assertRegex(protocol,r'#define HOST_LINK_BOARD_ID\s+F24_BRIDGE_BOARD_ID')

    def test_metadata_configuration_rejects_duplicate_computed_or_bad_geometry(self):
        import shutil
        with tempfile.TemporaryDirectory(prefix='host_config_') as temporary:
            root=Path(temporary); (root/'scripts').mkdir(); target=root/'firmware/f24_bridge_common'
            target.mkdir(parents=True)
            for name in ('f24_app_contract.ps1','device_app_config.ps1'):
                shutil.copy2(ROOT/'scripts'/name,root/'scripts'/name)
            original=(PROFILE/'image_metadata_config.h').read_text()
            for old,new in (('F24_BRIDGE_UART_BAUD (115200UL)','F24_BRIDGE_UART_BAUD (921600UL)'),
                    ('F24_BRIDGE_ERASE_SIZE (1024UL)','F24_BRIDGE_ERASE_SIZE (0UL)'),
                    ('F24_BRIDGE_APP_START (0x00005000UL)','F24_BRIDGE_APP_START (0x00005000UL + 4UL)'),
                    ('#endif','#define F24_BRIDGE_BOARD_ID (0x0101U)\n#endif')):
                with self.subTest(old=old):
                    self.assertIn(old,original); (target/'image_metadata_config.h').write_text(original.replace(old,new))
                    result=subprocess.run(['powershell','-NoProfile','-Command',f". '{root/'scripts/f24_app_contract.ps1'}'; Read-F24MetadataConfig"],capture_output=True)
                    self.assertNotEqual(result.returncode,0,result.stdout+result.stderr)

    def test_host_engineering_guide_and_pdf_match_c_header_workflow(self):
        from pypdf import PdfReader
        text=(ROOT/'docs/SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md').read_text(encoding='utf-8')
        for term in ('app_image_config.h','app_irq_handlers.h','bridge_image_metadata.c/.h',
                'image_metadata_profile.c/.h','boot_app_handoff.h','Bu_ParseMetadataRegion',
                'linked App','0xF33BU','115200','length'):
            self.assertIn(term,text)
        for term in ('app_image.json','app_irq_handlers.json','f24_bridge_layout.h'):
            self.assertNotIn(term,text)
        pdf=PdfReader(ROOT/'docs/FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf')
        self.assertEqual(sum(len(page.images) for page in pdf.pages),3)
        self.assertIn('Bu_ParseMetadataRegion','\n'.join(page.extract_text() for page in pdf.pages))


if __name__=='__main__': unittest.main(verbosity=2)
