"""Measured-report provenance and RH850 image/address contract; read-only checks."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
WEB = companion_path('Web')
SITE = WEB / 'slide_boot_loader_for_RH850'


class Rh850GuideTests(unittest.TestCase):
    def test_can_nominal_mask_uses_the_actual_host_preset(self):
        host = (ROOT / 'src/core/can_host_client.cpp').read_text(encoding='utf-8-sig')
        preset = int(re.search(r'case (\d+)U: return L"1M"', host).group(1))
        guide = (ROOT / 'docs/RH850_CAN_DEVICE_GUIDE_zh-TW.md').read_text(encoding='utf-8-sig')
        declared = int(re.search(r'1M 為 `0x([0-9A-F]+)`', guide).group(1), 16)
        self.assertEqual(declared, 1 << preset)

    def test_timing_matches_actual_operation_inputs_and_original_reports(self):
        page = (SITE / 'app-development.md').read_text(encoding='utf-8-sig')
        summaries = [json.loads(p.read_text(encoding='utf-8-sig')) for p in sorted((ROOT / 'docs/data/rh850_update_timing').glob('*.json'))]
        self.assertTrue(summaries)
        chosen = {}
        for report in summaries:
            actual = ROOT / 'build/reports' / report['source_report']
            if actual.exists():
                raw = actual.read_bytes()
                self.assertEqual(hashlib.sha256(raw).hexdigest().upper(), report['source_sha256'])
                operation = json.loads(raw.decode('utf-8-sig'))['update']
                for field in ('input_bytes', 'elapsed_ms', 'phases_ms', 'result', 'host_baud', 'can', 'host_deltas'):
                    self.assertEqual(report[field], operation[field], field)
                self.assertIn('app_confirmation=PASS', operation['details'])
            key = (report['input_bytes'], report['image_crc32'], report['host_baud'], report['can'])
            chosen.setdefault(key, report)
        body = re.search(r'<!-- RH850_TIMING_DATA_BEGIN -->(.*?)<!-- RH850_TIMING_DATA_END -->', page, re.S).group(1)
        rows = [line for line in body.splitlines() if re.match(r'^\| [0-9.]+ KiB \|', line)]
        self.assertEqual(len(rows), len(chosen))
        for report in chosen.values():
            size = report['input_bytes']
            matching = next(row for row in rows if f'| {size:,} |' in row)
            self.assertIn(f'| {report["elapsed_ms"] / 1000:.1f} s |', matching)
            self.assertIn(report['source_report'].replace('.json', '.html'), matching)
        self.assertIn('nominal 500', page)
        self.assertIn('data 2', page)

    def test_all_metadata_bytes_have_rh850_absolute_addresses(self):
        example = json.loads((ROOT / 'docs/data/rh850_image_example.json').read_text())
        metadata = bytes.fromhex(example['metadata_hex'])
        self.assertEqual(len(metadata), 256)
        self.assertEqual(metadata[:4], b'CAN\0')
        self.assertEqual(int.from_bytes(metadata[0x18:0x1c], 'little'), example['input_bytes'])
        self.assertEqual(int.from_bytes(metadata[0x1c:0x20], 'little'), 0x10000)
        self.assertEqual(int.from_bytes(metadata[0x68:0x6c], 'little'), 0x31563852)
        page = (SITE / 'reference.md').read_text(encoding='utf-8-sig')
        rows = re.findall(r'^\| 0x([0-9A-F]{2}) \| 0x([0-9A-F]{8}) \| ([0-9A-F]{2}) \|', page, re.M)
        self.assertEqual(len(rows), 256)
        for expected, (offset, address, value) in enumerate(rows):
            self.assertEqual(int(offset, 16), expected)
            self.assertEqual(int(address, 16), 0x10100 + expected)
            self.assertEqual(int(value, 16), metadata[expected])

    def test_check_only_preserves_canonical_and_projected_files(self):
        paths = list(SITE.rglob('*')) + list((ROOT / 'docs').glob('RH850_*.md'))
        paths = [p for p in paths if p.is_file()]
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        result = subprocess.run(['node', str(ROOT / 'scripts/sync_rh850_guides.mjs'), str(WEB), '--check'], capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(before, {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})


if __name__ == '__main__':
    unittest.main(verbosity=2)
