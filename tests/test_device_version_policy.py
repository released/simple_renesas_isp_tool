"""Authoring/transaction guards; hardware behavior belongs to board acceptance."""
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest
from app_config_fixtures import image_header
from workspace_paths import companion_path

ROOT=Path(__file__).resolve().parents[1]
F13=companion_path('F13Boot')
F24=F13.parent/'RL78_F24_Boot_loader_CAN'


class DeviceVersionPolicy(unittest.TestCase):
    def test_both_configs_explain_values_and_generate_exact_policy(self):
        for boot in (F13,F24):
            app=boot.parent/(boot.name+'_app')
            text=(app/'app_image_config.h').read_text(encoding='utf-8-sig')
            for term in ('0x02U=I2C','0x04U=UART','5U','0xF33BU',
                         'IMAGE_BOOT_VERSION','APP_IMAGE_VERSION_CONTROL',
                         'force upgrade old version firmware','0U=disabled'):
                self.assertIn(term,text)
            data=(app/'DefaultBuild'/(app.name+'_update.bin')).read_bytes()
            self.assertEqual(data[0x100:0x104],b'CAN\x00')
            self.assertEqual(data[0x164],3)
            self.assertEqual(struct.unpack_from('<I',data,0x174)[0],0)
            self.assertEqual(data[0x178:0x180],b'\x00'*8)

    def test_header_reader_rejects_undefined_values_without_mutation(self):
        valid=image_header(dict(version='0x00010600',build=20261017,target='0xF13B',role='DEVICE_APP',transport='CAN'))
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'app_image_config.h'
            args=['powershell.exe','-NoProfile','-Command',
                  f"$ErrorActionPreference='Stop'; . '{F13/'app_config.ps1'}'; Read-AppImageConfig '{temporary}' 0xF13B | Out-Null"]
            for text in (valid,valid.replace('VERSION_CONTROL (0)','VERSION_CONTROL (1)')):
                path.write_text(text,encoding='utf-8')
                result=subprocess.run(args,capture_output=True)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for original,replacement in (
                ('ROLE (1U)','ROLE (5U)'),('TARGET_ID (0xF13BUL)','TARGET_ID (0xF33BUL)'),
                ('UPDATE_TRANSPORTS (1U)','UPDATE_TRANSPORTS (2U)'),
                ('UPDATE_TRANSPORTS (1U)','UPDATE_TRANSPORTS (4U)'),
                ('UPDATE_TRANSPORTS (1U)','UPDATE_TRANSPORTS (3U)'),
                ('VERSION_CONTROL (0)','VERSION_CONTROL (2)'),
                ('VERSION (0x00010600UL)','VERSION (0x01000000UL)')):
                self.assertIn(original,valid)
                text=valid.replace(original,replacement)
                path.write_text(text,encoding='utf-8')
                before=path.read_bytes()
                self.assertNotEqual(subprocess.run(args,capture_output=True).returncode,0)
                self.assertEqual(path.read_bytes(),before)

    def test_boot_has_request_local_force_before_flash_and_real_validity(self):
        for boot in (F13,F24):
            text=(boot/'can_boot_transport.c').read_text(encoding='utf-8-sig')
            body=text.split('signed long CAN_BootReceive')[1]
            self.assertLess(body.index('CAN_BootInstalledValid()'),body.index('for (;;)'))
            start=body.split('case CAN_BOOT_CMD_START:')[1].split('case CAN_BOOT_CMD_BLOCK_BEGIN:')[0]
            self.assertIn('case CAN_BOOT_CMD_FORCE_START:',start)
            self.assertIn('VersionAllowed(g_version_policy, reply, frame.DB[0])',start)
            self.assertIn('CAN_BOOT_STATUS_VERSION',start)
            self.assertNotIn('Erase(',start)
            self.assertNotIn('Write(',start)
            self.assertNotRegex(text,r'\bg_force\w*\b')
            config=(boot/'image_metadata_config.h').read_text(encoding='utf-8-sig')
            self.assertIn('(0x004E4143UL)',config)
            self.assertNotIn('CAN3',config)

    def test_ui_password_and_worker_do_not_persist_authorization(self):
        resource=(ROOT/'FirmwareUpgradeTool.rc').read_text(encoding='utf-8-sig')
        self.assertIn('ES_PASSWORD',resource)
        self.assertIn('force upgrade old version firmware',resource)
        ui=(ROOT/'src/main_frame_workflow.cpp').read_text(encoding='utf-8-sig')
        body=ui.split('void CMainFrame::StartDeviceUpdate')[1].split('void CMainFrame::OnBnClickedDisconnect')[0]
        self.assertLess(body.index('ConfirmForceOldFirmware()'),body.index('ProgramF13Boot('))
        self.assertIn('force_old_version,approved_version',body)
        worker=(ROOT/'src/worker_can_workflows.cpp').read_text(encoding='utf-8-sig')
        body=worker.split('bool WorkerSession::ProcessProgramF13Boot')[1].split('bool WorkerSession::ConfirmDeviceApp')[0]
        self.assertLess(body.index('GetTargetInfo('),body.index('device_version::Check('))
        self.assertLess(body.index('approved_installed_version'),body.index('RunUpdate('))
        self.assertIn('callbacks.force_old_version=command.force_old_version',body)
        settings=(ROOT/'src/main_frame_settings.cpp').read_text(encoding='utf-8-sig')
        self.assertNotIn('force_old_version',settings)
        self.assertNotIn('3036',settings)

    def test_operation_docs_have_public_confirmation_and_single_owner(self):
        text=(ROOT/'docs/UPDATE_VALIDATION_zh-TW.md').read_text(encoding='utf-8-sig')
        self.assertIn('<a id="force-old-version"></a>',text)
        self.assertIn('3036',text)
        self.assertIn('force upgrade old version firmware',text)
        web=companion_path('Web')
        owner=(web/'slide_isp_tool_for_RL78/index.md').read_text(encoding='utf-8-sig')
        other=(web/'slide_boot_loader_for_RL78/index.md').read_text(encoding='utf-8-sig')
        self.assertEqual(owner.count('<a id="force-old-version"></a>'),1)
        self.assertNotIn('<a id="force-old-version"></a>',other)
        self.assertIn('3036',owner)


if __name__=='__main__':
    unittest.main()
