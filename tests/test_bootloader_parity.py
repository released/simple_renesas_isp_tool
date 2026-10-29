from pathlib import Path
import hashlib
import json
import os
import shutil
import stat
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
WEB=Path(os.environ['FIRMWARE_WEB_ROOT']) if os.environ.get('FIRMWARE_WEB_ROOT') else None


@unittest.skipUnless(WEB,'Set FIRMWARE_WEB_ROOT to the local Web workspace')
class BootloaderParityTests(unittest.TestCase):
    def fixture_base(self):
        base=ROOT/'tmp/internal/tests'
        for path in (base,*base.parents):
            if not path.is_relative_to(ROOT):
                break
            if path.exists():
                self.assertFalse(path.stat(follow_symlinks=False).st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT)
        base.mkdir(parents=True,exist_ok=True)
        return base

    def command(self,web):
        return subprocess.run(['node',str(ROOT/'scripts/verify_bootloader_parity.mjs'),str(web)],
                              capture_output=True,text=True,encoding='utf-8')

    def test_real_topics_titles_specs_and_style(self):
        result=self.command(WEB)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        contract=json.loads((ROOT/'scripts/bootloader_guide_contract.json').read_text(encoding='utf-8'))
        count=sum(len(page['topics']) for page in contract.values())
        self.assertIn(str(count)+' paired topics',result.stdout)

    def fixture(self,folder):
        for name in ('slide_boot_loader_for_RL78','slide_boot_loader_for_RH850'):
            destination=folder/name
            destination.mkdir()
            for source in (WEB/name).glob('*.md'):
                shutil.copy2(source,destination/source.name)
            shutil.copy2(WEB/name/'index.html',destination/'index.html')

    def test_menu_label_cannot_hide_a_missing_counterpart_body(self):
        base=self.fixture_base()
        with tempfile.TemporaryDirectory(prefix='guide_parity_',dir=base) as temporary:
            folder=Path(temporary)
            self.fixture(folder)
            page=folder/'slide_boot_loader_for_RH850/app-development.md'
            page.write_text(page.read_text(encoding='utf-8').replace(
                '### B. Metadata 設定與產生來源','### Missing metadata body'),encoding='utf-8')
            result=self.command(folder)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('real topic body',result.stderr)
        self.assertFalse(folder.exists())

    def test_independent_theme_drift_is_rejected(self):
        base=self.fixture_base()
        with tempfile.TemporaryDirectory(prefix='guide_parity_',dir=base) as temporary:
            folder=Path(temporary)
            self.fixture(folder)
            page=folder/'slide_boot_loader_for_RH850/index.html'
            page.write_text(page.read_text(encoding='utf-8').replace('<style>','<style>/* changed theme */',1),encoding='utf-8')
            result=self.command(folder)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('same theme',result.stderr)
        self.assertFalse(folder.exists())

    def test_future_rl78_topic_also_requires_a_rh850_body(self):
        base=self.fixture_base()
        with tempfile.TemporaryDirectory(prefix='guide_parity_',dir=base) as temporary:
            folder=Path(temporary)
            self.fixture(folder)
            page=folder/'slide_boot_loader_for_RL78/porting.md'
            page.write_text(page.read_text(encoding='utf-8')+'\n### Additional peripheral checks\n\nExample.\n',encoding='utf-8')
            result=self.command(folder)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('requires RH850 counterpart',result.stderr)
        self.assertFalse(folder.exists())

    def test_existing_rl78_measurements_are_preserved(self):
        source=(ROOT/'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md').read_text(encoding='utf-8')
        for milliseconds in (13960,15845,32281,65793,5222,7272,15556,32198,65453,
                             136242,5516,7240,15559,32188,65515,134185):
            self.assertIn('| '+str(milliseconds)+' |',source)
        self.assertIn('### App BIN 更新耗時參考',source)
        self.assertEqual(source.count('<a id="device-update-timing"></a>'),1)

    def test_projection_check_preserves_both_guides_and_sources(self):
        paths=list((ROOT/'docs').glob('RH850_*.md'))
        for name in ('slide_boot_loader_for_RL78','slide_boot_loader_for_RH850'):
            paths+=list((WEB/name).glob('*.md'))+list((WEB/name).glob('*.html'))
        before={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
        result=subprocess.run(['node',str(ROOT/'scripts/sync_rh850_guides.mjs'),str(WEB),'--check'],
                              capture_output=True,text=True,encoding='utf-8')
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(before,{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
