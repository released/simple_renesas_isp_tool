"""Reader entry, wire examples, source definitions and stable transport bookmarks."""
from pathlib import Path
import binascii
import json
import os
import re
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
WEB = Path(os.environ['FIRMWARE_WEB_ROOT']) if os.environ.get('FIRMWARE_WEB_ROOT') else None


@unittest.skipUnless(WEB, 'Set FIRMWARE_WEB_ROOT to the local Web workspace')
class SerialTransportGuideTests(unittest.TestCase):
    def source(self, name):
        return (ROOT/'docs'/name).read_text(encoding='utf-8-sig')

    def test_both_platforms_have_direct_boot_and_protocol_entries(self):
        for platform in ('RL78', 'RH850'):
            site = WEB/('slide_boot_loader_for_'+platform)
            home = (site/'index.md').read_text(encoding='utf-8')
            for bus in ('uart', 'i2c', 'xmodem'):
                self.assertIn(f'{bus}-device.html#{bus}-boot-flow', home)
                self.assertIn(f'{bus}-device.html#{bus}-protocol', home)
                page = (site/(bus+'-device.md')).read_text(encoding='utf-8')
                for number in range(1, 7):
                    self.assertIn(f'id="guide-{bus}-device-{number}"', page)
                self.assertIn('serial-device.html#guide-serial-device-1', page)
                self.assertIn('### Protocol Format', page)

    def test_info_request_example_matches_production_header_and_crc(self):
        source = self.source('ISP_SERIAL_TRANSPORT_GUIDE_zh-TW.md')
        matches = re.findall(r'^49 53 (?:[0-9A-F]{2} ){11}[0-9A-F]{2}$', source, re.M)
        self.assertEqual(len(matches), 1)
        frame = bytes.fromhex(matches[0])
        self.assertEqual(frame[:10], bytes.fromhex('49 53 01 00 01 00 01 00 00 00'))
        self.assertEqual(zlib.crc32(frame[:-4]), int.from_bytes(frame[-4:], 'little'))
        code = (ROOT/'firmware/common/isp_device/isp_protocol.c').read_text(encoding='utf-8')
        self.assertIn('output[0] = 0x49U;', code)
        self.assertIn('output[1] = 0x53U;', code)
        self.assertIn('IspWrite32(output + length, IspCrc32(output, length));', code)

    def test_xmodem_payload_crc_is_independently_reproducible(self):
        source = self.source('ISP_SERIAL_TRANSPORT_GUIDE_zh-TW.md')
        self.assertEqual(binascii.crc_hqx(bytes(range(128)), 0), 0xE80A)
        self.assertEqual(binascii.crc_hqx(b'123456789', 0), 0x31C3)
        self.assertIn('01 01 FE [payload: 00 01 02 ... 7F，共 128 bytes] E8 0A', source)
        self.assertIn('EOT 只送一次', source)

    def test_command_and_status_values_are_tied_to_c_header(self):
        header = (ROOT/'firmware/common/isp_device/isp_protocol.h').read_text(encoding='utf-8')
        common = self.source('ISP_UART_DEVICE_DESIGN_zh-TW.md')
        for status, name in enumerate(('OK', 'BAD_COMMAND', 'BAD_SIZE', 'BAD_STATE', 'BAD_SEQUENCE',
                                       'BAD_METADATA', 'BAD_VERSION', 'FLASH_ERROR', 'CRC_ERROR')):
            self.assertRegex(header, rf'#define ISP_{name}\s+{status}U')
            self.assertIn(f'| {status:02X} | {name} |', common)
        for command, value in (('METADATA', 3), ('START', 4), ('WRITE', 5), ('END', 6),
                               ('ABORT', 7), ('VERIFY', 9), ('FORCE_START', 14)):
            self.assertIn(f'#define ISP_CMD_{command} 0x{value:02X}U', header)
        self.assertIn('#### INFO payload：88 bytes', common)
        self.assertIn('#### 更新命令 payload', common)

    def test_legacy_transport_anchors_move_to_their_real_body(self):
        for platform in ('RL78', 'RH850'):
            site = WEB/('slide_boot_loader_for_'+platform)
            mapping = json.loads((site/'guide-map.json').read_text(encoding='utf-8'))['aliases']
            for anchor, bus in [('guide-serial-device-6', 'uart'), ('serial-uart-diagnostics', 'uart'),
                                ('guide-serial-device-7', 'i2c'), ('guide-serial-device-8', 'xmodem')]:
                self.assertEqual(mapping[anchor]['page'], bus+'-device.html')
                page = (site/(bus+'-device.md')).read_text(encoding='utf-8')
                self.assertIn('id="'+mapping[anchor]['anchor']+'"', page)

    def test_authoritative_tables_have_one_owner_per_platform(self):
        for platform in ('RL78', 'RH850'):
            site = WEB/('slide_boot_loader_for_'+platform)
            common = (site/'serial-device.md').read_text(encoding='utf-8')
            self.assertIn('| Frame byte |', common)
            for bus in ('uart', 'i2c', 'xmodem'):
                page = (site/(bus+'-device.md')).read_text(encoding='utf-8')
                self.assertNotIn('| Frame byte |', page)
                self.assertIn('serial-device.html#guide-serial-device-5', page)
                self.assertNotRegex(page, r'(?<![A-Za-z])[A-Za-z]:[\\/]|_Avery|SourceCode')
            self.assertNotIn('#### UART診斷格式', common)
            self.assertNotIn('### XMODEM-CRC128資料與診斷', common)

    def test_independent_build_and_platform_diagnostic_scope(self):
        operation = self.source('ISP_UART_TOOL_GUIDE_zh-TW.md')
        self.assertNotIn('共用來源與建置 helper 由該 Boot 提供', operation)
        self.assertIn('各自包含來源碼、設定與建置工具', operation)
        for platform, limit, excluded in [('RL78', '65535', '0xFFFFFFFF'), ('RH850', '0xFFFFFFFF', '65535')]:
            page = (WEB/('slide_boot_loader_for_'+platform)/'uart-device.md').read_text(encoding='utf-8')
            diagnostics = page.split('#### UART診斷格式')[1]
            self.assertIn(limit, diagnostics)
            self.assertNotIn(excluded, diagnostics)


if __name__ == '__main__':
    unittest.main(verbosity=2)
