"""Device-only guide and App log checks. No COM, Flash or real project edits."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
F13 = companion_path('F13Boot')
F24 = Path(os.environ.get('FIRMWARE_F24_DEVICE_BOOT_PROJECT', str(F13.parent / 'RL78_F24_Boot_loader_CAN')))
APP = Path(os.environ.get('FIRMWARE_F24_DEVICE_APP_PROJECT', str(F24.parent / 'RL78_F24_Boot_loader_CAN_app')))


class DeviceCanBitrateGuide(unittest.TestCase):
    def test_presets_match_real_clocks_register_encoding_and_metadata_bits(self):
        guide = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        for rate, f13_brp, f24_brp, bit in ((250000, 3, 7, 1), (500000, 1, 3, 2), (1000000, 0, 1, 4)):
            self.assertEqual(16000000 // ((f13_brp + 1) * (1 + 11 + 4)), rate)
            self.assertEqual(40000000 // ((f24_brp + 1) * (1 + 15 + 4)), rate)
            self.assertIn(f'`0x{1 << bit:08X}UL`', guide)
        for path in (F13 / 'hal/can/RSCANlite.h', F13.parent / 'RL78_F13_Boot_loader_CAN_app/hal/can/RSCANlite.h'):
            text = path.read_text(encoding='utf-8-sig')
            self.assertIn('CAN_CFG_BAUDRATEL(1U)', text)
            self.assertIn('CAN_TSEG1_11TQ, CAN_TSEG2_4TQ', text)
        config = (F24 / 'hal/device/f24_device_config.h').read_text()
        for name, value in (('NBRP', 3), ('NMNL_TSEG1', 15), ('NMNL_TSEG2', 4)):
            self.assertRegex(config, rf'#define CAN_CFG_{name}\s+\({value}\)')
        self.assertIn('**Boot 與 App 各自**', guide)
        self.assertIn('0x130..0x133', guide)
        self.assertIn('CAN 更新只改 App', guide)
        self.assertIn('不宣告多個速率 bit', guide)
        self.assertIn('250K／1M 的連續更新', guide)

    def test_irq_same_name_rule_uses_actual_separate_images(self):
        text = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        for anchor in ('device-irq-ownership', 'device-can-bitrate', 'device-app-runtime-log'):
            self.assertEqual(text.count(f'<a id="{anchor}"></a>'), 1)
        self.assertIn('兩份映像可以使用相同 C ISR 名稱', text)
        self.assertIn('同一個映像內，每個函式只能有一份定義', text)
        self.assertIn('`0xAA`', text)
        self.assertIn('`0x55`', text)
        self.assertIn('無 `vect=`', text)
        self.assertIn('X(0x2E, Device_TimerIsr)', (APP / 'app_irq_handlers.h').read_text())
        vectors = (F24 / 'f24_boot_vectors.asm').read_text()
        self.assertIn('#LOWW(_Device_TimerIsr)', vectors)
        self.assertIn('CMP !0xF710,#0x55', vectors)
        self.assertIn('f13_boot_timer_isr', (F13 / 'f13_boot_vectors.asm').read_text())

    def test_log_is_main_loop_only_and_registered_in_app(self):
        main = (APP / 'custom_func.c').read_text()
        self.assertIn('log_counter = 0UL;', main)
        self.assertRegex(main, r'App_FormatRuntimeCounter\(\w+,\s*\w+\+\+\);')
        self.assertIn('>= 1000U', main)
        self.assertIn('log(timer)-', main)
        self.assertNotIn('APP alive version=', main)
        board = (F24 / 'hal/device/device_board.c').read_text()
        isr = board.split('void __near Device_TimerIsr(void)', 1)[1].split('unsigned int get_tick', 1)[0]
        self.assertNotIn('Device_Log', isr)
        self.assertNotIn('App_FormatRuntimeCounter', isr)
        mtpj = next(APP.glob('*.mtpj')).read_text()
        for name in ('app_runtime_log.c', 'app_runtime_log.h'):
            self.assertIn(f'<RelativePath>{name}</RelativePath>', mtpj)

    def test_production_c90_formatter_v1_v2_boundaries_and_wrap(self):
        compilers = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))
        self.assertTrue(compilers, 'MSVC is required for this developer-only native test')
        bootstrap = compilers[-1].parents[7] / 'Common7/Tools/VsDevCmd.bat'
        setup = subprocess.run(f'cmd.exe /d /c call "{bootstrap}" -no_logo -arch=amd64 >nul && set', capture_output=True, text=True)
        self.assertEqual(setup.returncode, 0, setup.stdout + setup.stderr)
        environment = os.environ.copy()
        for line in setup.stdout.splitlines():
            if '=' in line and not line.startswith('='):
                key, value = line.split('=', 1)
                environment[key] = value
        with tempfile.TemporaryDirectory(prefix='device_app_log_') as temporary:
            folder = Path(temporary)
            for name in ('app_runtime_log.c', 'app_runtime_log.h'):
                shutil.copyfile(APP / name, folder / name)
            shutil.copyfile(ROOT / 'tests/device_app_runtime_log_test.c', folder / 'test.c')
            original = (folder / 'app_runtime_log.h').read_text()
            for tag in ('V1', 'V2'):
                (folder / 'app_runtime_log.h').write_text(original.replace('#define APP_RUNTIME_LOG_TAG "V1"', f'#define APP_RUNTIME_LOG_TAG "{tag}"'), encoding='utf-8')
                result = subprocess.run([str(compilers[-1]), '/nologo', '/TC', '/Za', '/W4', '/WX', 'test.c', 'app_runtime_log.c', '/Fe:test.exe'], cwd=folder, env=environment, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run([str(folder / 'test.exe')], cwd=folder, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(f'PASS: {tag}'.encode(), result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
