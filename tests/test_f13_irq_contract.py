"""Audit actual CC-RL HEX and execute the small IRQ dispatcher instruction subset.

This is a deterministic stack/register model, not an RL78 hardware simulator.
Instruction/stack semantics: Renesas R01US0015EJ0230, chapters 3, 5 and 6.
"""
import json
import argparse
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import unittest
from workspace_paths import companion_path

BOOT = companion_path('F13Boot')
APP = companion_path('F13App')
OPTIONS = argparse.ArgumentParser(add_help=False)
OPTIONS.add_argument('--boot-project', type=Path, default=BOOT)
OPTIONS.add_argument('--app-project', type=Path, default=APP)
PATHS, UNITTEST_ARGS = OPTIONS.parse_known_args()
BOOT, APP = PATHS.boot_project, PATHS.app_project


def symbols(project, name):
    text = (project / 'DefaultBuild' / (name + '.map')).read_text(encoding='utf-8-sig')
    return {m[1]: (int(m[2], 16), int(m[3], 16)) for m in re.finditer(
        r'(?m)^\s*(\w+)\s*\n\s*([\da-fA-F]{8})\s+([\da-fA-F]+)\s+(?:func|none)', text)}


def hex_memory(path):
    memory, base = {}, 0
    for line in path.read_text().splitlines():
        raw = bytes.fromhex(line[1:])
        assert sum(raw) % 256 == 0
        count, address, kind = raw[0], int.from_bytes(raw[1:3], 'big'), raw[3]
        if kind == 4:
            base = int.from_bytes(raw[4:6], 'big') << 16
        elif kind == 0:
            memory.update({base + address + i: b for i, b in enumerate(raw[4:4+count])})
    return memory


class Contract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bs = symbols(BOOT, BOOT.name)
        cls.asym = symbols(APP, APP.name)
        cls.rom = hex_memory(BOOT / 'DefaultBuild/boot0000_1FFF.hex')
        cls.image = (APP / 'DefaultBuild' / (APP.name + '_update.bin')).read_bytes()
        cls.handlers = json.loads((APP / 'app_irq_handlers.json').read_text())

    def test_complete_vectors_and_real_reti_handlers(self):
        for v in range(4, 0x7E, 2):
            stub = self.bs[f'f13_irq_{v:02X}'][0]
            self.assertEqual(self.rom[v] | self.rom[v+1] << 8, stub)
            self.assertEqual(bytes(self.rom[stub+i] for i in range(4)),
                             b'\xfd' + self.bs['f13_irq_dispatch'][0].to_bytes(2, 'little') + bytes([v]))
            offset = v * 2
            symbol = '_' + self.handlers[f'0x{v:02X}'] if f'0x{v:02X}' in self.handlers else 'f13_app_default_isr'
            self.assertEqual(self.image[offset:offset+4], b'\xec' + self.asym[symbol][0].to_bytes(3, 'little'))
        self.assertEqual(bytes(self.rom.get(i, 255) for i in (2, 3)), b'\xff'*2)
        # The linker fills an unowned BRK slot with zero; do not forward it.
        self.assertNotEqual(self.rom.get(126,255) | self.rom.get(127,255) << 8, 0x20FC)
        self.assertNotIn('.VECTOR 0x7E', (BOOT/'f13_boot_vectors.asm').read_text())
        self.assertEqual(self.image[4:8], b'\xff'*4)
        self.assertEqual(self.image[252:256], b'\xec' + self.asym['f13_app_default_brk'][0].to_bytes(3,'little'))
        brk = self.asym['f13_app_default_brk'][0]-0x2000
        self.assertEqual(self.image[brk:brk+2], b'\x61\xec')
        for symbol in self.handlers.values():
            address, size = self.asym['_'+symbol]
            self.assertEqual(self.image[address-0x2000+size-2:address-0x2000+size], b'\x61\xfc')
        addr, size = self.bs['_f13_boot_timer_isr']
        self.assertEqual(bytes(self.rom[addr+size-2+i] for i in range(2)), b'\x61\xfc')

    def test_actual_dispatch_machine_code_preserves_frame_and_registers(self):
        rng = random.Random(0xF13)
        for vector in range(4, 0x7E, 2):
            for owner in (0, 0x55, 0xAA, 0xFF):
                for bank in range(4):
                    for sample in range(8):
                        with self.subTest(vector=vector, owner=owner, bank=bank, sample=sample):
                            self.run_dispatch(vector, owner, bank, rng)

    def run_dispatch(self, vector, owner, bank, rng):
        sp = 0xF600
        pc = self.bs[f'f13_irq_{vector:02X}'][0]
        ax, hl, es = rng.randrange(65536), rng.randrange(65536), rng.randrange(16)
        initial = ax, hl, es
        saved_pc = rng.randrange(0x2000, 0x20000)
        saved_psw = 0x80 | ((bank & 1) << 3) | ((bank & 2) << 4) | rng.randrange(8)
        hardware = saved_pc.to_bytes(3, 'little') + bytes([saved_psw])
        ram = {sp+i: value for i, value in enumerate(hardware)}
        initial_sp, z = sp, False
        def word(address): return ram[address] | ram[address+1] << 8
        def push(value, count=2):
            nonlocal sp
            sp -= count
            ram.update({sp+i: b for i,b in enumerate(value.to_bytes(count, 'little'))})
        def pop(count=2):
            nonlocal sp
            value = int.from_bytes(bytes(ram[sp+i] for i in range(count)), 'little')
            sp += count
            return value
        for _ in range(50):
            op = self.rom[pc]
            if op == 0xFD:  # CALL !addr16
                dest = self.rom[pc+1] | self.rom[pc+2] << 8
                push(pc+3, 4); pc = dest; continue
            if op == 0xC1: push(ax); pc += 1
            elif op == 0xC7: push(hl); pc += 1
            elif op == 0x8E:
                self.assertEqual(self.rom[pc+1], 0xFD)
                ax = (ax & 255) | es << 8; pc += 2
            elif op == 0x41:
                es = self.rom[pc+1]; pc += 2
            elif op == 0xA8: ax = word(sp+self.rom[pc+1]); pc += 2
            elif op == 0x16: hl = ax; pc += 1
            elif op == 0x11:
                self.assertEqual(self.rom[pc+1], 0x8B)
                ax = (ax & 255) | self.rom[(es << 16) | hl] << 8; pc += 2
            elif op == 0x70: ax = (ax & 0xFF00) | (ax >> 8); pc += 1
            elif op == 0x51: ax = (ax & 255) | self.rom[pc+1] << 8; pc += 2
            elif op == 0x31:
                self.assertEqual(self.rom[pc+1], 0x1D)
                ax = (ax << 1) & 0xFFFF; pc += 2
            elif op == 0x04:
                ax = (ax + self.rom[pc+1] + (self.rom[pc+2] << 8)) & 0xFFFF; pc += 3
            elif op == 0x40:
                self.assertEqual([self.rom[pc+1],self.rom[pc+2]], [0x10,0xF7])
                z = owner == self.rom[pc+3]; pc += 4
            elif op == 0x44:
                z = ax == (self.rom[pc+1] | self.rom[pc+2] << 8); pc += 3
            elif op in (0xDD, 0xDF, 0xEF):
                relative = self.rom[pc+1]; relative -= 256 if relative >= 128 else 0
                take = op == 0xEF or (z if op == 0xDD else not z)
                pc += 2 + (relative if take else 0)
            elif op == 0x30: ax = self.rom[pc+1] | self.rom[pc+2] << 8; pc += 3
            elif op == 0xB8:
                addr = sp+self.rom[pc+1]; ram[addr] = ax & 255; ram[addr+1] = ax >> 8; pc += 2
            elif op == 0xC0: ax = pop(); pc += 1
            elif op == 0x9E:
                self.assertEqual(self.rom[pc+1], 0xFD)
                es = ax >> 8; pc += 2
            elif op == 0xC6: hl = pop(); pc += 1
            elif op == 0xD7:
                target = pop(4) & 0xFFFFF
                expected = 0x2000+vector*2 if owner == 0x55 else self.bs[
                    '_f13_boot_timer_isr' if owner == 0xAA and vector == 0x1E else 'f13_boot_default_isr'][0]
                self.assertEqual(target, expected)
                self.assertEqual((ax,hl,es),initial)
                self.assertEqual(sp,initial_sp)
                self.assertEqual(bytes(ram[sp+i] for i in range(4)),hardware)
                # The real ISR RETI consumes the original PC20+PSW frame.
                self.assertEqual(pop(4),saved_pc | saved_psw << 24)
                self.assertEqual(sp,initial_sp+4)
                return
            else: self.fail(f'Unexpected dispatcher opcode {op:02X} at {pc:04X}')
        self.fail('Unbounded dispatcher')

    def test_generator_current_and_invalid_mapping(self):
        script = BOOT / 'generate_irq_contract.ps1'
        for mode in ('Boot','App'):
            subprocess.run(['powershell','-NoProfile','-File',str(script),'-Mode',mode,'-CheckOnly'], check=True, capture_output=True)
        original = (BOOT / 'f13_boot_vectors.asm').read_bytes()
        with tempfile.TemporaryDirectory(prefix='f13_irq_') as temp:
            dest = Path(temp)
            # Registered source paths remain absolute; never mutate the real app.
            project = (APP / (APP.name+'.mtpj')).read_text(encoding='utf-8-sig')
            project = re.sub(r'<RelativePath>(.*?)</RelativePath>',
                lambda m: '<RelativePath>'+str(APP/m[1])+'</RelativePath>', project)
            (dest/'sample.mtpj').write_text(project,encoding='utf-8')
            identity=json.loads((APP/'app_image.json').read_text())
            identity['version']='0x00020000'
            (dest/'app_image.json').write_text(json.dumps(identity),encoding='utf-8')
            for invalid in ({'0x7E':'bad'},{'0x35':'bad'},{'0x64':'bad'},{'0x34':'not_a_registered_isr'}):
                (dest/'app_irq_handlers.json').write_text(json.dumps(invalid))
                result = subprocess.run(['powershell','-NoProfile','-File',str(script),'-Mode','App','-AppProject',temp],capture_output=True)
                self.assertNotEqual(result.returncode,0)
            mapping = dict(self.handlers)
            # A different app-local mapping uses an existing normal ISR at INTAD.
            mapping['0x34'] = mapping.pop('0x08')
            (dest/'app_irq_handlers.json').write_text(json.dumps(mapping))
            subprocess.run(['powershell','-NoProfile','-File',str(script),'-Mode','App','-AppProject',temp],check=True,capture_output=True)
            self.assertIn('BR !!_r_intc0_interrupt ; vector 0x34', (dest/'app_irq_table.inc').read_text())
            self.assertIn('CAN_BOOT_APP_FIRMWARE_VERSION (0x00020000UL)',
                          (dest/'app_identity.h').read_text())
        self.assertEqual((BOOT/'f13_boot_vectors.asm').read_bytes(),original)


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]]+UNITTEST_ARGS,
                  testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
