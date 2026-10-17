"""Documentation contract checks; no COM access, Flash writes or PDF authoring."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from pypdf import PdfReader
import pdfplumber
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT / 'docs/CAN_UPDATE_WALKTHROUGH_zh-TW.md'
PATTERN = r'<!-- CAN_WALKTHROUGH_BEGIN -->.*?<!-- CAN_WALKTHROUGH_END -->'


class CanUpdateWalkthrough(unittest.TestCase):
    def test_teaching_structure_and_safety_boundaries(self):
        text = DOC.read_text(encoding='utf-8-sig')
        self.assertEqual(text.count('```mermaid\n'), 9)
        self.assertEqual(len(re.findall(r'(?m)^\|(?: --- \|){3,5}$', text)), 15)
        self.assertEqual(len(re.findall(r'(?m)^### [A-M]\. ', text)), 13)
        for phrase in ['MCU CAN host', 'MCU CAN device',
                       'Discover 不會讀回完整 256-byte', '不是 read-back CRC',
                       'Metadata Status = VALID', 'CRC Result = MATCH',
                       'START ACK 不是「已擦除完成」', '轉送 CAN frames',
                       '外層 status OK', '128 core + 128 reserved',
                       '不是 CAN FD frame', '不是斷線續傳功能',
                       '不是 rollback', 'FLASH_ERROR', '另收 App log',
                       'CRC提供完整性，不是簽章', '30秒 inactivity', 'Misc／UART']:
            self.assertIn(phrase, text)
        self.assertNotRegex(text, r'_Avery|[A-Z]:[\\/]|SourceCode|尚未發布|Tool00\d\d')

    def test_wire_examples_and_address_units(self):
        text = DOC.read_text(encoding='utf-8-sig')
        meta = bytes.fromhex('08 00 00 00 43 41 4E 00')
        ack = bytes.fromhex('08 00 04 00 00 01 00 00')
        commit = bytes.fromhex('0C 00 00 00 40 20 00 00')
        for data in [meta, ack, commit]:
            self.assertIn(data.hex(' ').upper(), text)
        self.assertEqual(meta[4:], b'CAN\0')
        self.assertEqual((int.from_bytes(ack[2:4], 'little'), int.from_bytes(ack[4:], 'little')), (4, 256))
        self.assertEqual(int.from_bytes(commit[4:], 'little'), 0x2000 + 64)
        self.assertEqual(12 + 10 * (2 + 1 + 8), 122)
        self.assertIn('含 UART framing 共 136 bytes', text)
        self.assertIn('0x2000 + 0x100 = 0x2100', text)
        self.assertIn('block index4、image offset0x100', text)

    def test_source_links_resolve_to_real_files_and_functions(self):
        text = DOC.read_text(encoding='utf-8-sig')
        urls = set(re.findall(r'https://github\.com/released/[^\s)]+/blob/main/[^\s)]+\.(?:cpp|c|inc)', text))
        self.assertEqual(len(urls), 15)
        roots = {'simple_renesas_isp_tool': ROOT,
                 'RL78_F13_Boot_loader_CAN': companion_path('F13Boot')}
        for url in urls:
            repo, relative = url.split('/released/', 1)[1].split('/blob/main/', 1)
            self.assertTrue((roots[repo] / relative).is_file(), url)
        symbols = {
            ROOT / 'src/main_frame_image.cpp': ['LoadBinaryFromPath', 'ValidateLoadedFirmwareImage'],
            ROOT / 'src/core/firmware_update_profile.cpp': ['ParseMetadata', 'InspectImage', 'ValidateImage'],
            ROOT / 'src/worker_can_workflows.cpp': ['ProcessDiscoverF13Boot', 'ValidateF13BootTarget', 'ProcessProgramF13Boot'],
            ROOT / 'src/core/f13_boot_profile.cpp': ['Client::GetTargetInfo', 'Client::StageMetadata', 'Client::SendBlock', 'Client::End'],
            ROOT / 'firmware/RL78_F24_Bridge_app/host_link_protocol.c': ['HostLink_Process', 'evaluate_parser', 'dispatch_request'],
            ROOT / 'firmware/RL78_F24_Bridge_app/host_link_can_process.inc': ['process_can_transaction', 'next_burst_frame'],
            roots['RL78_F13_Boot_loader_CAN'] / 'can_boot_transport.c': ['CAN_BootRead', 'CAN_BootReceive', 'CAN_BootWrite'],
            roots['RL78_F13_Boot_loader_CAN'] / 'can_boot_metadata.inc': ['MetadataStage', 'MetadataSeal', 'MetadataValid'],
            roots['RL78_F13_Boot_loader_CAN'] / 'can_boot_block.inc': ['BlockBegin', 'BlockSegment', 'BlockCheck'],
        }
        for path, names in symbols.items():
            source = path.read_text(encoding='utf-8-sig')
            for name in names:
                self.assertIn(name.split('::')[-1], text)
                self.assertRegex(source, re.escape(name) + r'\s*\(')

    def test_sync_is_bounded_and_check_only_read_only(self):
        text = DOC.read_text(encoding='utf-8-sig')
        web = companion_path('Web')
        isp = (web / 'slide_isp_tool_for_RL78/index.md').read_text(encoding='utf-8-sig')
        boot = (web / 'slide_boot_loader_for_RL78/index.md').read_text(encoding='utf-8-sig')
        self.assertEqual(re.findall(PATTERN, text, re.S), re.findall(PATTERN, isp, re.S))
        self.assertEqual(isp.count('<a id="can-update-walkthrough"></a>'), 1)
        self.assertIn('https://released.github.io/slide_isp_tool_for_RL78/#can-update-walkthrough', boot)
        with tempfile.TemporaryDirectory(prefix='can_walkthrough_') as folder:
            page = Path(folder) / 'slide_isp_tool_for_RL78/index.md'
            page.parent.mkdir()
            page.write_text('prefix\n<!-- CAN_WALKTHROUGH_BEGIN -->stale<!-- CAN_WALKTHROUGH_END -->\nsuffix', encoding='utf-8')
            command = ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                       str(ROOT / 'scripts/sync_can_update_walkthrough.ps1'), '-WebRoot', folder]
            before = page.read_bytes()
            self.assertNotEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertEqual(page.read_bytes(), before)
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(command + ['-CheckOnly'], capture_output=True).returncode, 0)
            self.assertTrue(page.read_text(encoding='utf-8').startswith('prefix\n'))
            self.assertTrue(page.read_text(encoding='utf-8').endswith('\nsuffix'))
            page.write_text(page.read_text(encoding='utf-8') + '<!-- CAN_WALKTHROUGH_BEGIN --><!-- CAN_WALKTHROUGH_END -->', encoding='utf-8')
            before = page.read_bytes()
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(page.read_bytes(), before)

    def test_pdf_has_all_diagrams_and_no_sync_markup(self):
        reader = PdfReader(ROOT / 'docs/CAN_UPDATE_WALKTHROUGH_zh-TW.pdf')
        self.assertEqual(sum(len(page.images) for page in reader.pages), 9)
        text = '\n'.join(page.extract_text() for page in reader.pages)
        self.assertNotIn('CAN_WALKTHROUGH_', text)
        self.assertNotIn('<a id=', text)
        for phrase in ['CAN_BootReceive', 'MetadataStage', 'BlockCheck', 'CRC Result', '136 bytes']:
            self.assertIn(phrase, text)
        for page in reader.pages:
            self.assertTrue(page.extract_text().strip())
        # Detect a landscape-width table accidentally carried into a portrait page.
        with pdfplumber.open(ROOT / 'docs/CAN_UPDATE_WALKTHROUGH_zh-TW.pdf') as pdf:
            for index, page in enumerate(pdf.pages, 1):
                for char in page.chars:
                    if char['text'].strip():
                        self.assertGreaterEqual(char['x0'], -0.5, (index, char))
                        self.assertLessEqual(char['x1'], page.width + 0.5, (index, char))


if __name__ == '__main__':
    unittest.main(verbosity=2)
