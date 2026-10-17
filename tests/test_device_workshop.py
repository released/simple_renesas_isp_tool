"""Device documentation examples execute only in temporary projects / RAM.

Never rebuild firmware, open COM, overwrite actual images or publish a site.
"""
import binascii
import base64
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

from workspace_paths import companion_path
from app_config_fixtures import image_header, irq_header, read_irq_header, image_examples
from pypdf import PdfReader
import pdfplumber

ROOT = Path(__file__).resolve().parents[1]
DOC = ROOT/'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md'
WEB = companion_path('Web')
F13 = companion_path('F13Boot')
F13_APP = companion_path('F13App')
F24 = Path(os.environ.get('FIRMWARE_F24_DEVICE_BOOT_PROJECT', str(F13.parent/'RL78_F24_Boot_loader_CAN')))
F24_APP = Path(os.environ.get('FIRMWARE_F24_DEVICE_APP_PROJECT', str(F24.parent/'RL78_F24_Boot_loader_CAN_app')))
PATTERN = re.compile(r'<!-- DEVICE_WORKSHOP_BEGIN -->.*?<!-- DEVICE_WORKSHOP_END -->',re.S)


def command(args):
    result = subprocess.run(args,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60)
    if result.returncode:
        raise AssertionError(result.stdout+result.stderr)
    return result


def hashed(paths):
    return {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}


class DeviceWorkshop(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = DOC.read_text(encoding='utf-8-sig')
        blocks = re.findall(r'```c\n(.*?)\n```',cls.text,re.S)
        cls.examples = image_examples(cls.text) + [read_irq_header(value) for value in blocks if '#ifndef APP_IRQ_HANDLERS_H' in value]
        cls.irq_c = next(value for value in blocks if '#pragma interrupt app_aux_irq' in value)

    def test_scope_structure_source_links_and_examples(self):
        self.assertEqual(self.text.count('```mermaid'),3)
        self.assertEqual(len(self.examples),8)
        self.assertGreaterEqual(len(re.findall(r'(?m)^\| --- \|',self.text)),7)
        self.assertEqual(self.text.count('```')%2,0)
        self.assertNotRegex(self.text,r'[A-Z]:[\\/]|_Avery|尚未發布')
        for phrase in ('F13 (MCU CAN device)','F24 (MCU CAN device)',
                       '不會初始化或啟用 INTP1','CheckOnly','minimum Boot','匹配','更新中斷後'):
            if phrase == '匹配':
                self.assertIn('matching',self.text)
            else:
                self.assertIn(phrase,self.text)
        self.assertIn('#pragma interrupt app_aux_irq',self.irq_c)
        self.assertNotRegex(self.irq_c,r'\bstatic\b|\bvect\s*=|printf|\bEI\s*\(')
        self.assertIn('PIF1 = 0U;',self.irq_c)
        for index,value in enumerate(self.examples[:4]):
            self.assertEqual((value['target'],value['role'],value['transport']),
                             ('0xF13B' if index<2 else '0xF24B','DEVICE_APP','CAN'))
        self.assertEqual(self.examples[3]['version'],'0x00010001')
        for before,after in ((self.examples[4],self.examples[5]),(self.examples[6],self.examples[7])):
            self.assertNotIn('0x0A',before)
            self.assertEqual(after['0x0A'],'app_aux_irq')
            self.assertTrue(all(after[k]==v for k,v in before.items()))
        self.assertEqual(0x2000+0x0A*2,0x2014)
        self.assertEqual(struct.pack('<I',0x10501),bytes.fromhex('01 05 01 00'))
        self.assertEqual(struct.pack('<I',0x10001),bytes.fromhex('01 00 01 00'))
        self.assertTrue((F13/'generate_irq_contract.ps1').exists())
        for app in (F13_APP,F24_APP):
            header=(app/'iodefine.h').read_text(encoding='utf-8-sig')
            self.assertRegex(header,r'#define\s+INTP1\s+0x000A\b')
            self.assertRegex(header,r'#define\s+PIF1\s+')

    def temp_project(self,source,destination,irq):
        project, = source.glob('*.mtpj')
        tree=ET.parse(project)
        for node in tree.findall('.//Instance'):
            element=node.find('RelativePath')
            if node.findtext('Type')=='File' and element is not None and element.text:
                path=(source/element.text).resolve()
                element.text=str(path)
        group=tree.getroot().find('Class')
        node=ET.SubElement(group,'Instance')
        ET.SubElement(node,'Type').text='File'
        ET.SubElement(node,'RelativePath').text='app_extra_irq.c'
        tree.write(destination/project.name,encoding='utf-8')
        (destination/'app_extra_irq.c').write_text(self.irq_c,encoding='utf-8')
        (destination/'app_irq_handlers.h').write_text(irq_header(irq),encoding='utf-8')
        shutil.copyfile(source/'iodefine.h', destination/'iodefine.h')

    def test_f13_actual_generator_with_documented_metadata_and_irq(self):
        protected=list(F13.glob('*.asm'))+[F13/'can_boot_config.h',F13/f'{F13.name}.mtpj']
        before=hashed(protected)
        with tempfile.TemporaryDirectory(prefix='device_workshop_f13_') as folder:
            project=Path(folder)
            self.temp_project(F13_APP,project,self.examples[5])
            (project/'app_image_config.h').write_text(image_header(self.examples[1]),encoding='utf-8')
            args=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
                  str(F13/'generate_irq_contract.ps1'),'-Mode','App','-AppProject',str(project)]
            command(args)
            files=[project/'app_irq_table.inc',project/'app_identity.h',project/'ftable.inc']
            snapshot=hashed(files)
            command(args+['-CheckOnly'])
            self.assertEqual(snapshot,hashed(files))
            text=files[0].read_text()
            self.assertIn('BR !!_app_aux_irq ; vector 0x0A, slot 0x2014',text)
            self.assertIn('CAN_BOOT_APP_FIRMWARE_VERSION (0x00010501UL)',files[1].read_text())
            self.assertIn('CAN_BOOT_APP_FIRMWARE_BUILD (20261016UL)',files[1].read_text())
            self.assertRegex(files[2].read_text(),r'APP_IMAGE_VERSION\s+\.EQU 0x00010501')
            self.assertIn('Do not edit',files[2].read_text())
            self.reject_invalid_config(args,project,self.examples[1],files)
            files[2].write_text(files[2].read_text()+'; tampered\n')
            tampered=hashed(files)
            self.assertNotEqual(subprocess.run(args+['-CheckOnly'],capture_output=True).returncode,0)
            self.assertEqual(tampered,hashed(files))
            command(args)
        self.assertEqual(before,hashed(protected))

    def test_f24_actual_app_prepare_with_documented_header_and_irq(self):
        if not F24.exists(): self.skipTest('Select F24 device checkout for production generator tests')
        protected=list(F24.glob('*.asm'))+[F24/'can_boot_config.h',F24/f'{F24.name}.mtpj']
        before=hashed(protected)
        # The production generator deliberately uses same-volume relative paths.
        scratch=F24/'tmp/workshop_tests'
        scratch.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='device_workshop_f24_',dir=scratch) as folder:
            project=Path(folder)
            self.temp_project(F24_APP,project,self.examples[7])
            for name in ('app_main.c','iodefine.h'):
                shutil.copyfile(F24_APP/name,project/name)
            shutil.copytree(F24_APP/'src/smc_gen',project/'src/smc_gen')
            (project/'app_image_config.h').write_text(image_header(self.examples[3]),encoding='utf-8')
            args=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
                  str(F24/'tools/prepare_projects.ps1'),'-Kind','app','-Project',str(project)]
            command(args)
            files=[project/'app_irq_table.inc',project/'app_identity.h',
                   project/'generated_projection/cstart.asm',next(project.glob('*.mtpj')),
                   project/'ftable.inc',project/'ftable_RL78_F24.asm']
            snapshot=hashed(files)
            command(args+['-CheckOnly'])
            self.assertEqual(snapshot,hashed(files))
            command(args+['-ArtifactsOnly'])
            self.assertEqual(snapshot,hashed(files))
            self.assertIn('BR !!_app_aux_irq ; vector 0A',files[0].read_text())
            self.assertIn('DEVICE_APP_VERSION (65537UL)',files[1].read_text())
            self.assertIn('DEVICE_APP_BUILD (20261016UL)',files[1].read_text())
            words=[int(v,16) for v in re.findall(r'\.DB4 (0x[0-9A-F]+)',files[4].read_text())]
            self.assertEqual(len(words),64)
            self.assertEqual(words[4:7],[0x10001,20261016,0xFFFFFFFF])
            self.assertEqual(words[32:],[0xFFFFFFFF]*32)
            self.reject_invalid_config(args,project,self.examples[3],files)
            files[4].write_text(files[4].read_text()+'; tampered\n')
            tampered=hashed(files)
            self.assertNotEqual(subprocess.run(args+['-CheckOnly'],capture_output=True).returncode,0)
            self.assertEqual(tampered,hashed(files))
            command(args)
        self.assertEqual(before,hashed(protected))

    def reject_invalid_config(self,args,project,valid,files):
        snapshot=hashed(files)
        cases=[dict(valid,version='1+2'),dict(valid,version='0xFFFFFFFF'),dict(valid,build='20261016'),
               dict(valid,build=1.5),dict(valid,build=True),dict(valid,build=-1),
               dict(valid,build=4294967295),dict(valid,role='BRIDGE_APP'),
               dict(valid,transport='I2C'),dict(valid,target='0x1234'),dict(valid,unknown=1)]
        missing=dict(valid); del missing['version']; cases.append(missing)
        for invalid in cases:
            with self.subTest(invalid=invalid):
                (project/'app_image_config.h').write_text(image_header(invalid),encoding='utf-8')
                self.assertNotEqual(subprocess.run(args,capture_output=True).returncode,0)
                self.assertEqual(snapshot,hashed(files))
        (project/'app_image_config.h').write_text(image_header(valid),encoding='utf-8')

    def test_actual_bins_and_f24_metadata_producer_without_output_writes(self):
        for app,target,abi in ((F13_APP,0xF13B,0x31563146),(F24_APP,0xF24B,0x31563246)):
            image_path=app/'DefaultBuild'/f'{app.name}_update.bin'
            image=image_path.read_bytes()
            before=hashed([image_path])
            self.assertEqual(image[256:260],b'CAN\0')
            self.assertEqual(struct.unpack_from('<H',image,268)[0],target)
            self.assertEqual(struct.unpack_from('<I',image,280)[0],len(image))
            self.assertEqual(struct.unpack_from('<I',image,360)[0],abi)
            self.assertEqual(image[384:512],b'\xff'*128)
            self.assertEqual(binascii.crc32(image[:-4]),struct.unpack_from('<I',image,len(image)-4)[0])
            self.assertEqual(before,hashed([image_path]))
        with tempfile.TemporaryDirectory(prefix='device_workshop_meta_') as folder:
            project=Path(folder)
            (project/'app_image_config.h').write_text(image_header(self.examples[3]),encoding='utf-8')
            snippet=f". '{F24/'tools/device_common.ps1'}'; $meta=Get-DeviceMetadata '{project}' 516; [Convert]::ToBase64String($meta)"
            args=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-Command',snippet]
            region=base64.b64decode(command(args).stdout.strip())
            self.assertEqual(region[16:20],bytes.fromhex('01 00 01 00'))
            self.assertEqual(struct.unpack_from('<I',region,20)[0],20261016)
            self.assertEqual(region[128:],b'\xff'*128)
            invalid=dict(self.examples[3]); invalid['transport']='I2C'
            (project/'app_image_config.h').write_text(image_header(invalid),encoding='utf-8')
            self.assertNotEqual(subprocess.run(args,capture_output=True).returncode,0)
            (project/'app_image_config.h').write_text(image_header(self.examples[3]),encoding='utf-8')
            shutil.copyfile(next(F24_APP.glob('*.mtpj')),project/'sample.mtpj')
            build=project/'DefaultBuild'; build.mkdir()
            for suffix in ('.mot','.map'):
                shutil.copyfile(F24_APP/'DefaultBuild'/(F24_APP.name+suffix),build/('sample'+suffix))
            result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
                str(F24/'tools/package_images.ps1'),'-Kind','app','-Project',str(project)],capture_output=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn(b'Compiled metadata differs',result.stdout+result.stderr)
            self.assertFalse(list(build.glob('*_update.bin')))

    def test_f13_post_build_rejects_stale_compiled_metadata(self):
        with tempfile.TemporaryDirectory(prefix='device_workshop_stale_') as folder:
            parent=Path(folder)
            boot=parent/F13.name; boot.mkdir()
            project=parent/F13_APP.name; project.mkdir()
            for name in ('generate_irq_contract.ps1','app_config.ps1','app_build_report.ps1','can_boot_config.h'):
                shutil.copyfile(F13/name,boot/name)
            self.temp_project(F13_APP,project,self.examples[5])
            (project/'app_image_config.h').write_text(image_header(self.examples[1]),encoding='utf-8')
            command(['powershell.exe','-NoProfile','-File',str(boot/'generate_irq_contract.ps1'),
                     '-Mode','App','-AppProject',str(project)])
            build=project/'DefaultBuild'; build.mkdir()
            for name in ('flash2000_1FFFF.hex',F13_APP.name+'.map'):
                shutil.copyfile(F13_APP/'DefaultBuild'/name,build/name)
            shutil.copyfile(F13_APP/'generate_update_image.ps1',project/'generate_update_image.ps1')
            result=subprocess.run(['powershell.exe','-NoProfile','-File',str(project/'generate_update_image.ps1')],
                                  capture_output=True,timeout=60)
            self.assertNotEqual(result.returncode,0)
            self.assertIn(b'Compiled App metadata is stale',result.stdout+result.stderr)
            self.assertFalse(list(build.glob('*_update.bin')))
            # Fresh generated files must not legitimize an old linked IRQ table.
            (project/'app_image_config.h').write_bytes((F13_APP/'app_image_config.h').read_bytes())
            irq=read_irq_header((F13_APP/'app_irq_handlers.h').read_text(encoding='utf-8-sig'))
            irq['0x34']='r_intc0_interrupt'
            (project/'app_irq_handlers.h').write_text(irq_header(irq),encoding='utf-8')
            command(['powershell.exe','-NoProfile','-File',str(boot/'generate_irq_contract.ps1'),
                     '-Mode','App','-AppProject',str(project)])
            result=subprocess.run(['powershell.exe','-NoProfile','-File',str(project/'generate_update_image.ps1')],
                                  capture_output=True,timeout=60)
            self.assertNotEqual(result.returncode,0)
            self.assertIn(b'Compiled App IRQ table is stale',result.stdout+result.stderr)
            self.assertFalse(list(build.glob('*_update.bin')))

    def test_both_web_copies_and_bounded_read_only_sync(self):
        for name in ('slide_boot_loader_for_RL78','slide_isp_tool_for_RL78'):
            text=(WEB/name/'index.md').read_text(encoding='utf-8-sig')
            block, = PATTERN.findall(text)
            if name == 'slide_boot_loader_for_RL78':
                self.assertEqual([block],PATTERN.findall(self.text))
            else:
                self.assertLess(len(block),800)
                self.assertIn('https://released.github.io/slide_boot_loader_for_RL78/#device-metadata-vector-workshop',block)
            self.assertEqual(text.count('<a id="device-metadata-vector-workshop"></a>'),1)
        with tempfile.TemporaryDirectory(prefix='device_workshop_web_') as folder:
            web=Path(folder); pages=[]
            for name in ('slide_boot_loader_for_RL78','slide_isp_tool_for_RL78'):
                page=web/name/'index.md'; page.parent.mkdir()
                page.write_text('prefix\n<!-- DEVICE_WORKSHOP_BEGIN -->old<!-- DEVICE_WORKSHOP_END -->\nsuffix',encoding='utf-8')
                pages.append(page)
            args=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',
                  str(ROOT/'scripts/sync_device_workshop.ps1'),'-WebRoot',str(web)]
            before=hashed(pages)
            self.assertNotEqual(subprocess.run(args+['-CheckOnly'],capture_output=True).returncode,0)
            self.assertEqual(before,hashed(pages))

            command(args); command(args+['-CheckOnly'])
            for page in pages:
                text=page.read_text(encoding='utf-8-sig')
                self.assertTrue(text.startswith('prefix\n') and text.endswith('\nsuffix'))
            pages[1].write_text(pages[1].read_text(encoding='utf-8')+PATTERN.findall(self.text)[0],encoding='utf-8')
            before=hashed(pages)
            self.assertNotEqual(subprocess.run(args,capture_output=True).returncode,0)
            self.assertEqual(before,hashed(pages))

    def test_irq_keys_are_attributed_to_hardware_sources(self):
        for key,symbol in [('0x08','INTP0'),('0x0A','INTP1'),('0x16','INTST0'),
                           ('0x18','INTSR0'),('0x1E','INTTRJ0')]:
            self.assertIn(key,self.text)
            self.assertIn(symbol,self.text)
            source=(F13_APP/'iodefine.h').read_text(encoding='utf-8-sig')
            self.assertRegex(source,rf'#define\s+{symbol}\s+0x00{key[2:]}\b')
        self.assertIn('iodefine.h',self.text)
        self.assertIn('Interrupt',self.text)
        self.assertIn('r_cg_serial_user.c',self.text)

    def test_pdf_diagrams_text_and_page_bounds(self):
        path=DOC.with_suffix('.pdf')
        reader=PdfReader(path)
        self.assertEqual(sum(len(page.images) for page in reader.pages),3)
        text='\n'.join(page.extract_text() for page in reader.pages)
        for phrase in ('app_aux_irq','0x2014','ftable.inc','app_image_config.h','CheckOnly','not raw','0xF24B'):
            if phrase == 'not raw': self.assertIn('raw',text)
            else: self.assertIn(phrase,text)
        self.assertNotIn('DEVICE_WORKSHOP_',text)
        self.assertNotIn('<a id=',text)
        with pdfplumber.open(path) as pdf:
            for index,page in enumerate(pdf.pages,1):
                self.assertTrue(page.extract_text().strip())
                for char in page.chars:
                    if char['text'].strip():
                        self.assertGreaterEqual(char['x0'],-0.5,(index,char))
                        self.assertLessEqual(char['x1'],page.width+0.5,(index,char))
                        self.assertGreaterEqual(char['top'],-0.5,(index,char))
                        self.assertLessEqual(char['bottom'],page.height+0.5,(index,char))


if __name__=='__main__':
    unittest.main(verbosity=2)
