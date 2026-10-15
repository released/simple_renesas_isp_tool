"""App-local generation negatives and optional real CC-RL A/B integration builds.

Temporary projects are never flashed; their new IRQs are never enabled.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / 'firmware/RL78_F24_Bridge_app'
BOOT = ROOT / 'firmware/RL78_F24_Bridge_boot'
OPTIONS = argparse.ArgumentParser(add_help=False)
OPTIONS.add_argument('--build-fixtures', action='store_true')
ARGS, REST = OPTIONS.parse_known_args()


def ps(script, *args):
    return subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass',
        '-File',str(ROOT/'scripts'/script),*map(str,args)],cwd=ROOT,
        stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)


def boot_hashes():
    return {str(p.relative_to(BOOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (
        BOOT/'bridge_vectors.asm', BOOT/'RL78_F24_Bridge_boot.mtpj',
        BOOT/'DefaultBuild/RL78_F24_Bridge_boot.mot')}


class LocalApp(unittest.TestCase):
    def setUp(self):
        self.before = boot_hashes()
        self.temp = tempfile.TemporaryDirectory(prefix='F24_IRQ_fixture_',dir=ROOT/'firmware')
        self.folder = Path(self.temp.name).resolve()
        self.assertEqual(self.folder.parent,(ROOT/'firmware').resolve())
        shutil.copytree(APP/'generated_projection',self.folder/'generated_projection')
        for source in (*APP.glob('*.c'), *APP.glob('*.h'),
                       APP/'app_irq_handlers.json', APP/'app_image.json'):
            shutil.copy2(source,self.folder/source.name)
        tree = ET.parse(APP/'RL78_F24_Bridge_app.mtpj')
        original = tree.getroot().find('.//ProjectGuid').text
        guid = str(uuid.uuid4())
        for node in tree.iter():
            if node.text == original: node.text = guid
        item = ET.SubElement(tree.getroot().find('Class'),'Instance',{'Guid':str(uuid.uuid4())})
        for key,value in {'Name':'local_irq.c','Type':'File','RelativePath':'local_irq.c',
            'TreeImageGuid':'941832c1-fc3b-4e1b-94e8-01ea17128b42',
            'ParentItem':'76f247c0-0a47-461a-b088-233f5a2b5061'}.items():
            ET.SubElement(item,key).text=value
        self.project = self.folder/'F24_IRQ_Project_A.mtpj'
        tree.write(self.project,encoding='utf-8',xml_declaration=False)
        self.write_source('aaa_irq','bbb_irq')

    def tearDown(self):
        self.assertEqual(boot_hashes(),self.before,'App operation changed boot source/project/MOT')
        # Resolve the exact generated fixture before recursive temp cleanup.
        self.assertEqual(Path(self.temp.name).resolve().parent,(ROOT/'firmware').resolve())
        self.temp.cleanup()

    def write_source(self,first,second):
        code = 'volatile unsigned int local_irq_count;\n'
        for symbol in (first,second):
            code += f'#pragma interrupt {symbol}\nvoid __near {symbol}(void)\n{{\n    local_irq_count++;\n}}\n'
        (self.folder/'local_irq.c').write_text(code,encoding='ascii')

    def generate(self,ok=True,*extra):
        result=ps('prepare_f24_app_irq.ps1','-AppProject',self.folder,*extra)
        if ok: self.assertEqual(result.returncode,0,result.stdout.decode(errors='replace'))
        else: self.assertNotEqual(result.returncode,0,result.stdout.decode(errors='replace'))
        return result

    def test_valid_app_local_mapping_identity_and_checkonly(self):
        mapping=json.loads((self.folder/'app_irq_handlers.json').read_text())
        mapping.update({'0x08':'aaa_irq','0x0A':'bbb_irq'})
        (self.folder/'app_irq_handlers.json').write_text(json.dumps(mapping))
        (self.folder/'app_image.json').write_text(json.dumps({'version':'0x00020001','build':20261016}))
        self.generate()
        table=(self.folder/'bridge_vectors.asm').read_text()
        self.assertEqual(table.count('    BR !!'),64)
        self.assertIn('.EXTERN _aaa_irq',table)
        self.assertIn('F24_APP_IMAGE_VERSION (0x00020001UL)',(self.folder/'f24_app_identity.generated.h').read_text())
        self.generate(True,'-CheckOnly')
        data=(self.folder/'bridge_vectors.asm').read_bytes()+b'; stale\r\n'
        (self.folder/'bridge_vectors.asm').write_bytes(data)
        self.generate(False,'-CheckOnly')
        self.assertEqual((self.folder/'bridge_vectors.asm').read_bytes(),data)

    def test_invalid_inputs_do_not_replace_previous_output(self):
        self.generate()
        previous=(self.folder/'bridge_vectors.asm').read_bytes()
        cases=[{'0x7E':'aaa_irq'},{'0x00':'aaa_irq'},{'0x02':'aaa_irq'},
            {'0x35':'aaa_irq'},{'0x74':'aaa_irq'},{'0x76':'aaa_irq'},
            {'0x34':'missing_irq'},{'0x34':'bad;symbol'},{'0x34':42},
            {'IRQ_ADC':'aaa_irq'}]
        for mapping in cases:
            with self.subTest(mapping=mapping):
                (self.folder/'app_irq_handlers.json').write_text(json.dumps(mapping))
                self.generate(False)
                self.assertEqual((self.folder/'bridge_vectors.asm').read_bytes(),previous)
        (self.folder/'app_irq_handlers.json').write_text('{"0x08":"aaa_irq","0x08":"bbb_irq"}')
        self.generate(False)
        (self.folder/'app_irq_handlers.json').write_text('null')
        self.generate(False)

    def test_rejects_ordinary_static_physical_and_unregistered_isrs(self):
        (self.folder/'app_irq_handlers.json').write_text('{"0x08":"aaa_irq"}')
        for code in ('void __near aaa_irq(void) {}',
            '#pragma interrupt aaa_irq\nstatic void __near aaa_irq(void) {}',
            '#pragma interrupt aaa_irq(vect=INTP0)\nvoid __near aaa_irq(void) {}',
            '/*\n#pragma interrupt aaa_irq\n*/\nvoid __near aaa_irq(void) {}'):
            with self.subTest(code=code):
                (self.folder/'local_irq.c').write_text(code,encoding='ascii')
                self.generate(False)
        self.write_source('aaa_irq','bbb_irq')
        text=self.project.read_text().replace('<RelativePath>local_irq.c</RelativePath>','<RelativePath>bridge_app.c</RelativePath>')
        self.project.write_text(text)
        self.generate(False)

    def test_identity_and_build_command_guards(self):
        for identity in ({'version':'1.2.0','build':1},{'version':'0xFFFFFFFF','build':1},
            {'version':'0x00010200','build':-1},{'version':'0x00010200','build':4294967295}):
            (self.folder/'app_image.json').write_text(json.dumps(identity))
            self.generate(False)
        result=ps('build_f24_bridge.ps1','-Kind','app','-Prepare','-AppProject',self.folder)
        self.assertNotEqual(result.returncode,0)
        result=ps('build_f24_bridge.ps1','-Kind','boot','-AppProject',self.folder)
        self.assertNotEqual(result.returncode,0)
        self.project.write_text(self.project.read_text().replace(
            '<DeviceName>R7F124FLJ3xFB</DeviceName>', '<DeviceName>R5F10BMG</DeviceName>'))
        result=self.generate(False)
        self.assertIn(b'supports only',result.stdout)
        self.project.write_text(self.project.read_text().replace(
            '<DeviceName>R5F10BMG</DeviceName>', '<DeviceName>R7F124FLJ3xFB</DeviceName>').replace(
            '<COptionD-0>BRIDGE_SPLIT_APP', '<COptionD-0>NOT_A_SPLIT_APP'))
        result=self.generate(False)
        self.assertIn(b'Enable BRIDGE_SPLIT_APP',result.stdout)

    @unittest.skipUnless(ARGS.build_fixtures,'Real CC-RL fixture builds not requested')
    def test_real_project_a_b_build_and_package_with_same_boot(self):
        evidence=ROOT/'tmp/internal/build/verify/f24_app_irq'
        evidence.mkdir(parents=True,exist_ok=True)
        original=json.loads((self.folder/'app_irq_handlers.json').read_text())
        for label,first,second,vectors,version,build in (
            ('A','aaa_irq','bbb_irq',('0x08','0x0A'),'0x00020001',20261016),
            ('B','ccc_irq','ddd_irq',('0x0C','0x0E'),'0x00030002',20261017)):
            name='F24_IRQ_Project_'+label
            next_project=self.folder/(name+'.mtpj')
            if next_project!=self.project:
                self.project.rename(next_project); self.project=next_project
            self.write_source(first,second)
            mapping=dict(original); mapping.update({vectors[0]:first,vectors[1]:second})
            (self.folder/'app_irq_handlers.json').write_text(json.dumps(mapping))
            (self.folder/'app_image.json').write_text(json.dumps({'version':version,'build':build}))
            result=ps('build_f24_bridge.ps1','-Kind','app','-AppProject',self.folder)
            (evidence/(label+'_build.log')).write_bytes(result.stdout)
            self.assertEqual(result.returncode,0,result.stdout.decode(errors='replace'))
            self.assertEqual(boot_hashes(),self.before)
            folder=self.folder/'DefaultBuild'
            image=(folder/(name+'_bridge_update.bin')).read_bytes()
            self.assertEqual(int.from_bytes(image[0x110:0x114],'little'),int(version,16))
            self.assertEqual(int.from_bytes(image[0x114:0x118],'little'),build)
            for extension in ('_bridge_update.bin','_flash.mot','.map'):
                shutil.copy2(folder/(name+extension),evidence/(name+extension))
            for local in ('app_irq_handlers.json','app_image.json','bridge_vectors.asm'):
                shutil.copy2(self.folder/local,evidence/(label+'_'+local))
        (evidence/'boot_hashes.json').write_text(json.dumps(self.before,indent=2),encoding='ascii')


if __name__=='__main__':
    unittest.main(argv=[sys.argv[0]]+REST,testRunner=unittest.TextTestRunner(stream=sys.stdout,verbosity=2))
