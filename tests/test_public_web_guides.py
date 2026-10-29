"""Keep public navigation linked and independent of delivery milestones."""
from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
WEB = companion_path('Web')
ISP = WEB / 'slide_simple_renesas_isp_tool'
BOOT = WEB / 'slide_boot_loader_for_RL78'
NAV = WEB / 'released.github.io/README.md'


class PublicWebGuides(unittest.TestCase):
    def test_update_timing_is_rough_report_based_and_single_owner(self):
        source = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        anchor = '<a id="device-update-timing"></a>'
        block = source.split(anchor, 1)[1].split('<a id="device-app-runtime-log">', 1)[0]
        layout = json.loads((ROOT/'scripts/web_guide_layout.json').read_text(encoding='utf-8-sig'))
        boot_page = layout[BOOT.name]['sections']['DEVICE_WORKSHOP']
        isp_page = layout[ISP.name]['sections']['DEVICE_WORKSHOP']
        boot = (BOOT / (boot_page+'.md')).read_text(encoding='utf-8-sig')
        isp = (ISP / (isp_page+'.md')).read_text(encoding='utf-8-sig')
        self.assertEqual(boot.count(anchor), 1)
        self.assertIn(block, boot)
        self.assertNotIn(anchor, isp)
        self.assertIn('#device-update-timing', isp)
        self.assertNotIn('約 65.8 秒', isp)
        for size, f13, f24, fd in ((24, '約 15.8 秒', '約 15.6 秒', '約 15.6 秒'),
                               (56, '約 32.3 秒', '約 32.2 秒', '約 32.2 秒'),
                               (119, '約 65.8 秒', '—', '—'),
                               (120, '不適用', '約 65.5 秒', '約 65.5 秒'),
                               (247, '不適用', '約 136.2 秒', '約 134.2 秒')):
            self.assertRegex(block, rf'(?m)^\| {size} KiB[^\n]+\| {f13} \| {f24} \| {fd} \|$')
        for token in ('115200', 'Detailed trace | 填充 BIN：關閉',
                      'nominal 500 kbit/s／data 2 Mbit/s／BRS', '0xCA0DE29D'):
            self.assertIn(token, block)
        for target in ('flash-metadata-can-reference', 'device-irq-ownership',
                       'device-irq-dispatch-cost', 'device-debug-monitor'):
            self.assertIn('/#' + target, block)
        for phrase in ('119／247 KiB', '120／248 KiB', '1 KiB', 'Boot 只能由 RFP 安裝'):
            self.assertIn(phrase, block)
        self.assertNotRegex(block, r'中位數|平均值|(?<![A-Za-z])[A-Za-z]:[\\/]|SourceCode')
        self.assertIn('| 原始 App（未填充） | 20,308 bytes／約 14.0 秒 | 4,224 bytes／約 5.2 秒 | 4,756 bytes／約 5.5 秒 |', block)
        self.assertIn('| 8 KiB（8,192 bytes） | 不適用 | 約 7.3 秒 | 約 7.2 秒 |', block)
        self.assertIn('六次均完成 END CRC、APP_READY／live image 比對及 CAN_STOP 確認', block)

    def test_f24_fd_timing_reports_match_published_values(self):
        from html import unescape
        records = (
            ('102840_499_88416_699087546_1', 4756, 5516, '2244D36A', '原始 App，4,756 bytes'),
            ('102911_331_88416_699118375_2', 8192, 7240, '3FCC99E2', '8 KiB'),
            ('102956_261_88416_699163312_3', 24576, 15559, '688C8587', '24 KiB'),
            ('103045_317_88416_699212359_4', 57344, 32188, '5ED8A374', '56 KiB'),
            ('103238_863_88416_699325906_5', 122880, 65515, '2B5ADB06', '120 KiB'),
            ('103540_647_88416_699507687_6', 252928, 134185, '325DBF76', '247 KiB'),
        )
        paths = [ROOT/'build/reports'/('update_20261022_' + r[0] + '.json') for r in records]
        if not all(path.is_file() for path in paths):
            self.skipTest('Private source reports are absent; no synthetic hardware evidence.')
        source = (ROOT/'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        for index, (path, (identity, size, elapsed, crc, label)) in enumerate(zip(paths, records)):
            report = json.loads(path.read_text(encoding='utf-8-sig'))
            u = report['update']
            self.assertEqual((u['input_bytes'], u['elapsed_ms']), (size, elapsed))
            self.assertEqual(u['result'], 'PASS_WITH_WARNINGS' if index == 5 else 'PASS')
            self.assertEqual(report['tool'], '0.1.0.0099')
            self.assertEqual(u['host_baud'], 115200)
            self.assertIn('CAN FD 500K/2M', u['can'])
            for token in ('app_confirmation=PASS', 'host_can_stop=PASS', '0x'+crc):
                self.assertIn(token, u['details'])
            self.assertIn(f'| `{identity}` | {label} | {elapsed} | `0x{crc}` |', source)
            plain = re.sub(r'\s+', ' ', unescape(re.sub('<[^>]+>', ' ', path.with_suffix('.html').read_text(encoding='utf-8'))))
            self.assertIn(f'Input bytes {size} Elapsed ms {elapsed}', plain)
        log = (ROOT/'build/firmware_upgrade_tool_20261022_182748.log').read_text(encoding='utf-8-sig')
        self.assertEqual(log.count('App confirmation PASS:'), 6)
        self.assertNotRegex(log, 'TRACE UART|TRACE CAN|UI event queue overflow')

    def test_f24_classical_timing_reports_match_published_values(self):
        from html import unescape
        records = (
            ('100929_089_55528_697936171_1', 4224, 5222, '4837111E', '原始 App，4,224 bytes'),
            ('101010_155_55528_697977234_2', 8192, 7272, '04D5ED74', '8 KiB'),
            ('101047_098_55528_698014187_3', 24576, 15556, 'BF72D477', '24 KiB'),
            ('101148_050_55528_698075140_4', 57344, 32198, '9A0D4CAA', '56 KiB'),
            ('101320_919_55528_698168000_5', 122880, 65453, '1F248CFA', '120 KiB'),
            ('101601_871_55528_698328953_6', 252928, 136242, '3274CC2A', '247 KiB'),
        )
        paths = [ROOT / 'build/reports' / ('update_20261022_' + r[0] + '.json') for r in records]
        if not all(path.is_file() for path in paths):
            self.skipTest('Private source reports are absent; no synthetic hardware evidence.')
        source = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        for path, (identity, size, elapsed, crc, label) in zip(paths, records):
            u = json.loads(path.read_text(encoding='utf-8-sig'))['update']
            self.assertEqual((u['input_bytes'], u['elapsed_ms']), (size, elapsed))
            self.assertIn(u['result'], ('PASS', 'PASS_WITH_WARNINGS'))
            self.assertEqual(u['host_baud'], 115200)
            self.assertIn('Classical CAN 500K', u['can'])
            self.assertIn('app_confirmation=PASS', u['details'])
            self.assertIn('host_can_stop=PASS', u['details'])
            self.assertIn('0x' + crc, u['details'])
            self.assertIn(f'| `{identity}` | {label} | {elapsed} | `0x{crc}` |', source)
            plain = re.sub(r'\s+', ' ', unescape(re.sub('<[^>]+>', ' ',
                path.with_suffix('.html').read_text(encoding='utf-8'))))
            self.assertIn(f'Input bytes {size} Elapsed ms {elapsed}', plain)
        log = (ROOT / 'build/firmware_upgrade_tool_20261022_180801.log').read_text(encoding='utf-8-sig')
        self.assertEqual(log.count('App confirmation PASS:'), 6)
        self.assertNotRegex(log, 'TRACE UART|TRACE CAN|UI event queue overflow')

    def test_f13_timing_source_reports_match_published_values(self):
        folder = ROOT / 'build/reports'
        records = (
            ('092335_621_54044_695182765_1', 24576, 15845, 'CA0DE29D'),
            ('092427_590_54044_695234734_2', 57344, 32281, '8BF223B3'),
            ('092553_622_54044_695320765_3', 121856, 65793, '7A2A9624'),
        )
        paths = [folder / ('update_20261022_' + record[0] + '.json') for record in records]
        if not all(path.is_file() for path in paths):
            self.skipTest('Private source reports are not in this checkout; no synthetic hardware evidence.')
        from html import unescape
        source = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        for path, (identity, size, elapsed, crc) in zip(paths, records):
            report = json.loads(path.read_text(encoding='utf-8-sig'))
            update = report['update']
            self.assertEqual(update['input_bytes'], size)
            self.assertEqual(update['elapsed_ms'], elapsed)
            self.assertEqual(update['result'], 'PASS')
            self.assertEqual(update['link_retries'], 0)
            self.assertTrue(all(item.strip().endswith('=0') for item in update['host_deltas'].split(';')))
            self.assertEqual(update['host_baud'], 115200)
            self.assertIn('Classical CAN 500K', update['can'])
            self.assertIn('app_confirmation=PASS', update['details'])
            self.assertIn('host_can_stop=PASS', update['details'])
            self.assertIn('0x' + crc, update['details'])
            self.assertIn(f'| `{identity}` | {size // 1024} KiB | {elapsed} | `0x{crc}` |', source)
            html = unescape(re.sub('<[^>]+>', ' ', path.with_suffix('.html').read_text(encoding='utf-8')))
            plain = re.sub(r'\s+', ' ', html)
            self.assertIn(f'Input bytes {size} Elapsed ms {elapsed}', plain)
    def test_f13_filled_bin_timing_log(self):
        path = ROOT/'build/firmware_upgrade_tool_20261022_172301.log'
        if not path.is_file():
            self.skipTest('Historical private Log absent; report JSON/HTML checks remain independent.')
        log = path.read_text(encoding='utf-8-sig')
        self.assertEqual(log.count('App confirmation PASS:'), 3)
        self.assertNotRegex(log, 'TRACE UART|TRACE CAN|UI event queue overflow')

    def test_f13_original_app_timing_matches_user_report(self):
        from html import unescape
        path = ROOT/'build/reports/update_20261027_013422_855_47096_164948546_1.json'
        if not path.is_file():
            self.skipTest('Private user report absent; no synthetic hardware evidence.')
        report = json.loads(path.read_text(encoding='utf-8-sig'))
        update = report['update']
        self.assertEqual((report['tool'],update['result'],update['input_bytes'],update['elapsed_ms']),
                         ('0.1.0.0109','PASS',20308,13960))
        self.assertEqual((update['host_baud'],update['link_retries']),(115200,0))
        self.assertIn('Classical CAN 500K',update['can'])
        self.assertIn('target 0xF13B',update['target_before'])
        for token in ('image_crc32=0x0C297B5E','blocks_acked=318',
                      'app_confirmation=PASS','host_can_stop=PASS'):
            self.assertIn(token,update['details'])
        self.assertEqual(update['host_counter_assessment'],'NO_RECORDED_ERRORS')
        self.assertTrue(all(item.strip().endswith('=0') for item in update['host_deltas'].split(';')))
        plain = re.sub(r'\s+',' ',unescape(re.sub('<[^>]+>',' ',path.with_suffix('.html').read_text(encoding='utf-8'))))
        self.assertIn('Input bytes 20308 Elapsed ms 13960',plain)
        log = (ROOT/'build/firmware_upgrade_tool_20261027_093357.log').read_text(encoding='utf-8-sig')
        for token in ('size 20308 bytes, CRC32 0x0C297B5E','318 blocks ACKed',
                      'App confirmation PASS:','App confirmed; Host CAN stopped.'):
            self.assertIn(token,log)
        self.assertNotRegex(log,'TRACE UART|TRACE CAN|Operation failed|UI event queue overflow')
        source = (ROOT/'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        self.assertIn('| `'+path.stem.removeprefix('update_20261027_')+'` | 原始 App，20,308 bytes | 13960 | `0x0C297B5E` |',source)
        self.assertIn('原始 App 識別碼前加 `update_20261027_`',source)
        self.assertIn('原始 App：0.1.0.0109；填充 BIN：0.1.0.0097',source)
        self.assertIn('firmware_upgrade_tool_20261027_093357.log',source)

    def test_debug_monitor_warning_is_single_owner_and_explicit(self):
        source = (ROOT / 'docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        anchor = '<a id="device-debug-monitor"></a>'
        block = source.split(anchor, 1)[1].split('### 3.', 1)[0]
        boot = (BOOT / 'index.md').read_text(encoding='utf-8-sig')
        isp = (ISP / 'index.md').read_text(encoding='utf-8-sig')
        self.assertEqual(boot.count(anchor), 1)
        self.assertIn(block, boot)
        self.assertNotIn(anchor, isp)
        for phrase in ('只能使用 Renesas Flash Programmer', '不可使用 CS+',
                       '120 KiB', '248 KiB', '119 KiB', '247 KiB',
                       '0x1FE00..0x1FFFF', '0x3FF00..0x3FFFF',
                       '512 bytes', '256 bytes', '重新建置配對的 Boot 與 App'):
            self.assertIn(phrase, block)
        for phrase in ('#device-debug-monitor', 'Boot 只能以 RFP 安裝', '可能導致 App 異常'):
            self.assertIn(phrase, isp)

    def test_crc_address_reference_is_single_owner(self):
        source = (ROOT / 'docs/FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        anchor = '<a id="image-crc-linear-address"></a>'
        block = source.split(anchor, 1)[1].split('## 2.', 1)[0]
        page = (BOOT / 'index.md').read_text(encoding='utf-8-sig')
        self.assertEqual(page.count(anchor), 1)
        self.assertIn(block, page)
        for phrase in ('32-bit', '0x0FFFF', '0x10000', '0x1FBFC', '0x3FBFC',
                       'F13_Crc32Bytes()', 'caculate_crc32_checksum()', 'APP_READY'):
            self.assertIn(phrase, block)

    def test_dispatch_cost_is_scoped_and_single_owner(self):
        source = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        page = (BOOT / 'index.md').read_text(encoding='utf-8-sig')
        start = '<a id="device-irq-dispatch-cost"></a>'
        block = source.split(start, 1)[1].split('##### 移植範圍', 1)[0]
        self.assertEqual(page.count(start), 1)
        self.assertIn(block, page)
        for phrase in ('22條指令', '3 + 4 + 12 + 5 + 11 + 3', '1.1875 µs',
                       '0.950 µs', '1.34375 µs', '1.125 µs', '0.095%', '9.5%',
                       '10 bytes', '尚非實板量測', 'GPIO pulse'):
            self.assertIn(phrase, block)
        self.assertEqual(sum((3, 4, 12, 5, 11, 3)), 38)
        self.assertEqual(38 / 32, 1.1875)
        self.assertEqual(38 / 40, 0.95)

    def test_rh850_has_an_independent_guide_and_short_peer_links(self):
        catalog = json.loads((ROOT/'scripts/web_guide_sections.json').read_text(encoding='utf-8-sig'))
        self.assertEqual(catalog['RH850_DEVICE']['owner'],'slide_boot_loader_for_RH850')
        rh850=WEB/'slide_boot_loader_for_RH850'
        pattern = re.compile(r'<!-- RH850_DEVICE_BEGIN -->.*?<!-- RH850_DEVICE_END -->', re.S)
        for page in (BOOT / 'index.md', ISP / 'index.md'):
            actual, = pattern.findall(page.read_text(encoding='utf-8-sig'))
            self.assertIn('slide_boot_loader_for_RH850/#rh850-can-device',actual)
            self.assertLess(len(actual),800)
            self.assertNotIn('```',actual)
        for name in ('can-device','app-development','serial-device','porting','reference','tool-guides'):
            self.assertTrue((rh850/(name+'.md')).is_file())
            self.assertTrue((rh850/(name+'.html')).is_file())
        self.assertIn('rh850-update-timing',(rh850/'app-development.md').read_text(encoding='utf-8-sig'))
        transfer = (ROOT / 'docs/MULTIBUS_TOOL_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        for name in ('Sample_Project_RH850_S1_CAN_Transfer', 'Sample_Project_RH850_S1_CAN_FD_Transfer'):
            self.assertNotIn(name, transfer)

    def test_can_roles_are_explicit_without_changing_project_identifiers(self):
        for page in [ISP / 'index.md', BOOT / 'index.md']:
            text = page.read_text(encoding='utf-8-sig')
            for phrase in ['F24 (MCU CAN host)', 'F13 (MCU CAN device)',
                           'F24 (MCU CAN device)', 'Misc']:
                self.assertIn(phrase, text)
            self.assertNotRegex(text, r'https?://[^\s\n]*\(MCU')
        isp = (ISP / 'index.md').read_text(encoding='utf-8-sig')
        for label in ['| F24 (MCU CAN host) Boot |', '| F24 (MCU CAN host) App |',
                      '| F13 (MCU CAN device) Boot |', '| F13 (MCU CAN device) App |']:
            self.assertIn(label, isp)

    def test_public_resources_downloads_anchors_and_sync(self):
        pattern = re.compile(r'<!-- PUBLIC_RESOURCES_BEGIN -->.*?<!-- PUBLIC_RESOURCES_END -->', re.S)
        source = (ROOT / 'docs/MULTIBUS_TOOL_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        block, = pattern.findall(source)
        layout = json.loads((ROOT / 'scripts/web_guide_layout.json').read_text(encoding='utf-8'))
        resource_pages = [site / (layout[site.name]['sections']['PUBLIC_RESOURCES'] + '.md')
                          for site in (ISP, BOOT)]
        for path in resource_pages:
            text = path.read_text(encoding='utf-8-sig')
            actual, = pattern.findall(text)
            if path.parent == ISP:
                self.assertEqual(actual, block)
            else:
                self.assertNotEqual(actual, block)
                self.assertIn('https://released.github.io/slide_simple_renesas_isp_tool/#downloads', actual)
                self.assertNotIn('raw.githubusercontent', actual)
            self.assertNotRegex(text, r'_Avery|[A-Z]:[\\/]|\.\./\.\./python|SourceCode')
            self.assertNotIn('有效core長度仍為96', text)
        for filename in ['FirmwareUpgradeTool.exe', 'RL78_F24_Bridge_boot.mot',
                         'RL78_F24_Bridge_app_bridge_update.bin', 'RL78_F24_Bridge_app_flash.mot', 'boot0000_1FFF.hex',
                         'boot_app.hex', 'RL78_F13_Boot_loader_CAN_app_update.bin']:
            self.assertIn('https://raw.githubusercontent.com/released/', block)
            self.assertIn('/' + filename + ')', block)
        anchors = [('slide_simple_renesas_isp_tool', name) for name in
                   ['tool-bridge-flow', 'tool-operation', 'custom-transfer',
                    'host-bridge-firmware', 'f13-device-update']]
        anchors.append(('slide_boot_loader_for_RL78', 'boot-app-porting'))
        anchors.append(('slide_boot_loader_for_RL78', 'f13-boot-decision'))
        for site, anchor in anchors:
            mapping = json.loads((WEB / site / 'guide-map.json').read_text(encoding='utf-8'))
            target = (WEB / site / mapping['aliases'][anchor]['page']).with_suffix('.md').read_text(encoding='utf-8-sig')
            self.assertEqual(target.count('<a id="' + anchor + '"></a>'), 1)
            self.assertIn('https://released.github.io/' + site + '/#' + anchor, block)

    def test_resource_sync_is_atomic_and_check_only_is_read_only(self):
        script = ROOT / 'scripts/sync_public_resources.ps1'
        with tempfile.TemporaryDirectory(prefix='public_sync_') as folder:
            web = Path(folder)
            pages = [web / site / 'index.md' for site in
                     ['slide_simple_renesas_isp_tool', 'slide_boot_loader_for_RL78']]
            for page in pages:
                page.parent.mkdir()
                page.write_text('prefix\n<!-- PUBLIC_RESOURCES_BEGIN -->stale<!-- PUBLIC_RESOURCES_END -->\nsuffix', encoding='utf-8')
            command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                       '-File', str(script), '-WebRoot', folder]
            before = [page.read_bytes() for page in pages]
            self.assertNotEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertEqual([page.read_bytes() for page in pages], before)
            pages[1].write_text('missing marker', encoding='utf-8')
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(pages[0].read_bytes(), before[0])
            pages[1].write_bytes(before[1])
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            for page in pages:
                self.assertTrue(page.read_text(encoding='utf-8').startswith('prefix\n'))
                self.assertTrue(page.read_text(encoding='utf-8').endswith('\nsuffix'))

    def test_navigation_has_canonical_links_and_function_descriptions(self):
        text = NAV.read_text(encoding='utf-8-sig')
        entries = re.findall(r'(?m)^\| \[(?:Simple Renesas ISP Tool|RL78 Bootloader)\].*$', text)
        self.assertEqual(len(entries), 2)
        self.assertIn('[Simple Renesas ISP Tool](https://released.github.io/slide_simple_renesas_isp_tool/', text)
        self.assertIn('[RL78 Bootloader](https://released.github.io/slide_boot_loader_for_RL78/', text)
        for entry in entries:
            self.assertNotRegex(entry, r'\d+\.\d+\.\d+|Tool\d{4}|工程第一版|19244|10963|301區塊')
        self.assertIn('Custom Transfer', entries[-1])
        self.assertTrue((ISP / 'index.html').is_file())

    def test_tool_branding_is_platform_neutral_and_personal(self):
        self.assertEqual(ISP.name, 'slide_simple_renesas_isp_tool')
        overview = (ISP / 'index.md').read_text(encoding='utf-8-sig')
        self.assertIn('Simple Renesas ISP Tool', overview)
        self.assertIn('個人開發與分享', overview)
        for page in ISP.glob('*.html'):
            html = page.read_text(encoding='utf-8-sig')
            self.assertIn('<strong>Simple Renesas ISP Tool</strong>', html)
            self.assertNotIn('RL78 ISP Tool', html)
        for path in [NAV, NAV.with_suffix('.html')]:
            text = path.read_text(encoding='utf-8-sig')
            self.assertIn('slide_simple_renesas_isp_tool/', text)
            self.assertNotIn('slide_isp_tool_for_' + 'RL78', text)

    def test_public_pages_have_no_draft_or_delivery_pins(self):
        for path in [NAV, NAV.with_suffix('.html'), ISP / 'index.md', BOOT / 'index.md',
                     BOOT / 'basic_uart_i2c/index.md',
                     ISP / 'README.md', BOOT / 'README.md']:
            text = path.read_text(encoding='utf-8-sig')
            self.assertNotRegex(text, r'尚未發布|未發布|尚未公開發布|本機稿|本機編輯稿|本機製作稿|工程第一版')
            # Measured results retain exact environment versions; navigation
            # and ordinary guides remain independent of delivery snapshots.
            evergreen = re.sub(r'(?s)<a id="device-update-timing"></a>.*?(?=<a id="device-app-runtime-log">)', '', text)
            self.assertNotRegex(evergreen, r'0\.1\.0\.\d+|Tool00\d\d|1\.5\.0|19244|10963')
            self.assertEqual(len(re.findall(r'(?m)^```', text)) % 2, 0)

    def test_navigation_exports_share_current_rl78_entries(self):
        from html import unescape
        markdown = NAV.read_text(encoding='utf-8-sig')
        exported = NAV.with_suffix('.html').read_text(encoding='utf-8-sig')
        for label in ('RL78 Bootloader', 'Simple Renesas ISP Tool'):
            entry, = re.findall(rf'(?m)^\| \[{re.escape(label)}\].*$', markdown)
            row, = re.findall(rf'<tr>\s*<td><a [^>]*>{re.escape(label)}</a></td>.*?</tr>', exported, re.S)
            plain_md = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', entry).strip('| ').split(' | ', 1)[1]
            plain_html = unescape(re.sub(r'<[^>]+>', '', row)).strip()
            self.assertEqual(plain_html, label + '\n' + plain_md)
            self.assertIn('https://released.github.io/slide_', row)
        entry, = re.findall(r'(?m)^\| \[RL78 Bootloader\].*$', markdown)
        self.assertNotIn('MCU CAN host', entry)

    def test_uart_i2c_scope_and_preserved_anchor_links(self):
        main = (BOOT / 'index.md').read_text(encoding='utf-8-sig')
        examples = (BOOT / 'basic_uart_i2c/index.md').read_text(encoding='utf-8-sig')
        self.assertTrue((BOOT / 'basic_uart_i2c/index.html').is_file())
        self.assertIn('參考工程', examples)
        self.assertIn('不是依 MCU 型號固定的最小 Boot 容量', examples)
        self.assertNotIn('MUST set bigger or equal', examples)
        self.assertNotIn('FLASH_TABLE       .EQU  0x5000', main)
        for text in (main, examples, (ISP / 'index.md').read_text(encoding='utf-8-sig')):
            prose = re.sub(r'(?ms)^```[^\n]*\n.*?^```\s*$', '', text)
            self.assertEqual(len(re.findall(r'(?m)^# ', prose)), 1)
            ids = re.findall(r'<a id="([^"]+)"></a>', prose)
            self.assertEqual(len(ids), len(set(ids)))
            self.assertNotRegex(text, r'[\uE000-\uF8FF\uFFFD]')
        targets = re.findall(r'\]\(basic_uart_i2c/#([^)]+)\)', main)
        self.assertGreater(len(targets), 10)
        for anchor in ('target_MCU_flash_type', 'ram_flag', 'FALSH_TABLE', 'ftable_inc',
                       'map_const', 'boot_main', 'boot_hdwinit', 'fsy_file',
                       'vect_address', 'vector_table'):
            self.assertIn(anchor, targets)
        folded = '\n'.join(re.findall(r'<details>.*?</details>', main, re.S))
        for anchor in targets:
            self.assertEqual(main.count(f'<a id="{anchor}"></a>'), 1)
            self.assertNotIn(f'<a id="{anchor}"></a>', folded)
            self.assertEqual(examples.count(f'<a id="{anchor}"></a>'), 1)

    def test_basic_names_redirects_and_engineering_notes(self):
        main = (BOOT / 'index.md').read_text(encoding='utf-8-sig')
        examples = (BOOT / 'basic_uart_i2c/index.md').read_text(encoding='utf-8-sig')
        redirect = (BOOT / 'uart_i2c/index.html').read_text(encoding='utf-8-sig')
        self.assertIn('location.search + location.hash', redirect)
        self.assertIn('../basic_uart_i2c/', redirect)
        for role in ('UART', 'app'):
            for suffix in ('', '_CRC', '_e2', '_HW_CRC'):
                self.assertIn(f'https://github.com/released/RL78_F24_Basic_Boot_loader_{role}{suffix}', examples)
        self.assertNotIn('RL78_F24_Boot_loader_', examples)
        for anchor in ('basic-vs-golden', 'flash-ram-execution', 'crc-backends'):
            self.assertEqual(main.count(f'<a id="{anchor}"></a>'), 1)
            self.assertIn(f'../#{anchor}', examples)
        for term in ('RFD_CMN', 'SMP_CF', 'DRAM', 'CRC32/ISO-HDLC', '0xCBF43926', '0x1021'):
            self.assertIn(term, main)

    def test_device_update_operation_includes_automatic_confirmation(self):
        text = (ISP / 'index.md').read_text(encoding='utf-8-sig')
        operation = text.split('## 3. MCU CAN device：App 更新', 1)[1].split('```mermaid', 1)[0]
        for phrase in ('APP_READY', 'live INFO', '[COMPLETE]', '不自動重寫',
                       '停止 Host CAN', '保留 COM', '#update-completion)'):
            self.assertIn(phrase, operation)

    def test_technical_contracts_and_unverified_boundaries_remain(self):
        text = (ISP / 'index.md').read_text(encoding='utf-8-sig')
        for phrase in ['最低 Boot 版本', 'IRQ ABI',
                       'G16', 'CRC提供完整性', 'BLOCKED']:
            self.assertIn(phrase, text)

    def test_single_owner_sections_and_generic_diagram_roles(self):
        catalog = json.loads((ROOT/'scripts/web_guide_sections.json').read_text(encoding='utf-8-sig'))
        sources = {'RH850_DEVICE':'RH850_CAN_DEVICE_GUIDE_zh-TW.md',
                   'UART_TOOL_GUIDE':'ISP_UART_TOOL_GUIDE_zh-TW.md',
                   'SERIAL_DEVICE_GUIDE':'ISP_UART_DEVICE_DESIGN_zh-TW.md',
                   'FD_TRANSFER':'CAN_FD_UPDATE_zh-TW.md',
                   'FD_UPDATE':'CAN_FD_UPDATE_zh-TW.md',
                   'FD_DEVICE':'CAN_FD_UPDATE_zh-TW.md',
                   'UPDATE_EVIDENCE':'UPDATE_VALIDATION_zh-TW.md',
                   'DEVICE_WORKSHOP':'DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md',
                   'F24_DEVICE':'F24_CAN_DEVICE_DESIGN_zh-TW.md',
                   'F13_BOOT_DECISION':'F13_BOOT_UPDATE_FLOW_zh-TW.md',
                   'F24_APP_IRQ':'SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md',
                   'COMMON_IMAGE_POLICY':'COMMON_IMAGE_METADATA.md',
                   'PUBLIC_RESOURCES':'MULTIBUS_TOOL_GUIDE_zh-TW.md'}
        layout = json.loads((ROOT/'scripts/web_guide_layout.json').read_text(encoding='utf-8-sig'))
        pages = {folder.name:'\n'.join((folder/(page+'.md')).read_text(encoding='utf-8-sig')
                 for page in layout[folder.name]['pages']) for folder in (ISP,BOOT)}
        for section, config in catalog.items():
            pattern = rf'<!-- {section}_BEGIN -->.*?<!-- {section}_END -->'
            if config.get('deferred'):
                for text in pages.values():
                    self.assertIn(f'<!-- {section}_BEGIN -->\n<!-- {section}_END -->', text)
                continue
            canonical, = re.findall(pattern,(ROOT/'docs'/sources[section]).read_text(encoding='utf-8-sig'),re.S)
            for folder, text in pages.items():
                block, = re.findall(pattern,text,re.S)
                if folder == config['owner']:
                    self.assertEqual(block,canonical)
                else:
                    self.assertLess(len(block),800)
                    self.assertNotIn('```',block)
                    self.assertIn(f"https://released.github.io/{config['owner']}/#{config['anchor']}",block)
        for text in pages.values():
            for diagram in re.findall(r'```mermaid\n(.*?)\n```',text,re.S):
                self.assertNotRegex(diagram,r'\bF(?:13|24)\b')
                self.assertNotRegex(diagram,r'MCU CAN (?:host|device) MCU CAN')
        boot = pages[BOOT.name]
        for section in ('DEVICE_WORKSHOP','RL78_PORTING','F24_DEVICE','FLASH_REFERENCE'):
            block, = re.findall(rf'<!-- {section}_BEGIN -->.*?<!-- {section}_END -->',boot,re.S)
            self.assertNotRegex(block,r'F24.{0,15}(?:host|HOST)|BUR1|Bridge_boot')
        self.assertIn('<a id="custom-transfer"></a>',pages[ISP.name])

    def test_device_irq_explanation_has_one_owner_and_porting_checks(self):
        workshop = (ROOT / 'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8-sig')
        ownership = workshop.split('<a id="device-irq-ownership"></a>', 1)[1].split('#### C1.', 1)[0]
        for token in ('16-bit', '低 64 KiB', '4-byte `BR`', '0x002E', '0x205C',
                      '0x5C', 'Device_TimerIsr', 'RETI', 'PUSH／POP', 'stack', 'boot swap'):
            self.assertIn(token, ownership)
        self.assertEqual(ownership.count('```mermaid'), 1)
        self.assertIn('0x2000 + (0x002E / 2) * 4 = 0x205C', ownership)
        self.assertEqual(0x2000 + (0x2E // 2) * 4, 0x205C)
        self.assertIn('<a id="device-app-irq-source"></a>', workshop)
        porting = (ROOT / 'docs/RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        section = porting.split('### 4. 固定向量介面', 1)[1].split('### 5.', 1)[0]
        self.assertNotIn('```mermaid', section)
        self.assertIn('#device-irq-ownership', section)
        self.assertIn('<a id="device-irq-porting"></a>', section)
        for label in ('IRQ 配置', 'Flash 分區', 'startup／linker', 'RAM／交接', 'Flash 更新'):
            self.assertIn('| ' + label + ' |', section)
        self.assertIn('擦寫期間的中斷條件', section)
        fd = (ROOT / 'docs/CAN_FD_UPDATE_zh-TW.md').read_text(encoding='utf-8-sig')
        fd_rx = fd.split('<a id="can-fd-device-rx"></a>', 1)[1].split('```mermaid', 1)[0]
        self.assertIn('0x2098', fd_rx)
        self.assertIn('#device-irq-ownership', fd_rx)
        self.assertNotIn('IMAGE_APPLICATION_START + physical_vector', fd_rx)

    def test_reader_pages_exclude_development_conversation(self):
        paths = [ISP/'index.md',BOOT/'index.md'] + [ROOT/'docs'/name for name in
            ('DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md','F24_CAN_DEVICE_DESIGN_zh-TW.md',
             'RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md','FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md',
             'MULTIBUS_TOOL_GUIDE_zh-TW.md','SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md')]
        for path in paths:
            text = path.read_text(encoding='utf-8-sig')
            self.assertNotRegex(text,r'G16\s*新版|新版\s*I2C|依使用者要求延後|本輪|本次未執行|native pipe|舊\s*format4|舊版 UART|較容易修改|為什麼 F24 device|不能把 F13')

    def test_verifier_accepts_future_product_version_without_page_update(self):
        with tempfile.TemporaryDirectory(prefix='isp_public_guide_') as folder:
            source = Path(folder) / 'src'
            source.mkdir()
            (source / 'build_info.generated.h').write_text(
                '#define FIRMWARE_UPGRADE_TOOL_VERSION L"9.9.9.999"\n', encoding='utf-8')
            # The verifier now also checks the current reader-copy contracts.
            for name in ('README.md', 'scripts/workspace_paths.ps1',
                         'scripts/sync_public_content_guidelines.ps1',
                         'scripts/verify_public_content.ps1'):
                target = Path(folder) / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / name, target)
            (Path(folder) / 'docs').mkdir()
            for guide in (ROOT / 'docs').glob('*.md'):
                shutil.copy2(guide, Path(folder) / 'docs' / guide.name)
            result = subprocess.run([
                'powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', str(ISP / 'scripts/verify_local.ps1'), '-ToolRoot', folder],
                capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(b'no build-version pin', result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
