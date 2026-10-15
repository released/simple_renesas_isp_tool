"""Generate documentation tables from real BINs; never modify firmware images."""
import argparse
import binascii
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
GUIDE = ROOT / 'docs/FLASH_METADATA_CAN_FRAME_GUIDE_zh-TW.md'
# Offsets are relative to metadata, not BIN start or physical Flash zero.
POLICY = [
    (0x60, 4, 'UPD1 magic', 'UPD1 policy'), (0x64, 1, 'policy_version', 'UPD1 policy'),
    (0x65, 1, 'policy_size', 'UPD1 policy'), (0x66, 1, 'image_role', 'UPD1 policy'),
    (0x67, 1, 'update_transports', 'UPD1 policy'), (0x68, 4, 'irq_abi', 'UPD1 policy'),
    (0x6C, 4, 'minimum_boot', 'UPD1 policy'), (0x70, 4, 'device_uart_baud', 'UPD1 policy'),
    (0x74, 12, 'reserved_zero', 'UPD1 policy'), (0x80, 128, 'reserved_FF', 'reserved region'),
]
F13 = [
    (0x00, 4, 'CAN3 magic', 'IMAGE_MANIFEST_MAGIC'),
    (0x04, 1, 'image_format', 'IMAGE_MANIFEST_FORMAT_INFO'),
    (0x05, 1, 'core_length', 'IMAGE_MANIFEST_FORMAT_INFO'),
    (0x06, 2, 'flags', 'IMAGE_MANIFEST_FORMAT_INFO'),
    (0x08, 2, 'protocol_id', 'IMAGE_MANIFEST_PROTOCOL'),
    (0x0A, 1, 'protocol_version', 'IMAGE_MANIFEST_PROTOCOL'),
    (0x0B, 1, 'checksum_type', 'IMAGE_MANIFEST_PROTOCOL'),
    (0x0C, 2, 'target_id', 'IMAGE_MANIFEST_TARGET'),
    (0x0E, 1, 'can_mode_mask', 'IMAGE_MANIFEST_TARGET'),
    (0x0F, 1, 'can_id_format', 'IMAGE_MANIFEST_TARGET'),
    (0x10, 4, 'app_version', 'APP_IMAGE_VERSION'), (0x14, 4, 'app_build', 'APP_IMAGE_BUILD'),
    (0x18, 4, 'image_size', 'IMAGE_SIZE_POST_BUILD / post-build'),
    (0x1C, 4, 'app_start', 'FLASH_TABLE'), (0x20, 4, 'app_capacity', 'APP_REGION_SIZE'),
    (0x24, 4, 'flash_size', 'CODE_FLASH_SIZE'), (0x28, 4, 'boot_size', 'BOOT_RESERVED_SIZE'),
    (0x2C, 4, 'erase_size', 'FLASH_ERASE_BLOCK_SIZE'),
    (0x30, 4, 'nominal_rate_mask', 'CAN_NOMINAL_BITRATE_MASK'),
    (0x34, 4, 'data_rate_mask', 'CAN_DATA_BITRATE_MASK'),
    (0x38, 4, 'request_id', 'CAN_REQUEST_ID'), (0x3C, 4, 'response_id', 'CAN_RESPONSE_ID'),
    (0x40, 16, 'mcu_part ASCII', 'MCU_PART_0..1 / zero padding'),
    (0x50, 12, 'mcu_family ASCII', 'MCU_FAMILY_0..1 / zero padding'),
    (0x5C, 4, 'minimum_size', 'MINIMUM_IMAGE_SIZE'),
] + POLICY
F24 = [
    (0x00, 4, 'BUR1 magic', 'Put32 0'), (0x04, 1, 'contract_version', 'image[0x104]'),
    (0x05, 1, 'core_length', 'image[0x105]'), (0x06, 1, 'image_role', 'image[0x106]'),
    (0x07, 1, 'checksum_type', 'image[0x107]'), (0x08, 2, 'board_id', 'BOARD_ID / image[0x108]'),
    (0x0A, 2, 'contract_id', 'image[0x10A] / zero padding'),
    (0x0C, 4, 'image_size', 'Put32 12 / linker-derived size'),
    (0x10, 4, 'app_version', 'app_image.json / Put32 16'),
    (0x14, 4, 'app_build', 'app_image.json / Put32 20'),
    (0x18, 4, 'app_start', 'APP_START / Put32 24'),
    (0x1C, 4, 'app_capacity', 'APP_LIMIT - APP_START / Put32 28'),
    (0x20, 4, 'flash_size', 'FLASH_SIZE / Put32 32'),
    (0x24, 4, 'boot_size', 'APP_START / Put32 36'),
    (0x28, 4, 'erase_size', 'ERASE_SIZE / Put32 40'),
    (0x2C, 4, 'minimum_size', 'MINIMUM_SIZE / Put32 44'),
    (0x30, 4, 'boot_uart_baud', 'UART_BAUD / Put32 48'),
    (0x34, 12, 'mcu_family ASCII', 'ASCII CopyTo image[0x134]'),
    (0x40, 16, 'mcu_part ASCII', 'ASCII CopyTo image[0x140]'),
    (0x50, 4, 'minimum_boot_copy', 'BOOT_VERSION / Put32 80'),
    (0x54, 12, 'reserved_zero', 'core initialized zero'),
] + POLICY


def byte_fields(schema):
    result = {}
    for offset, size, name, origin in schema:
        for index in range(size):
            address = offset + index
            if address in result or not 0 <= address < 256:
                raise ValueError('Overlapping/out-of-range metadata field')
            result[address] = (f'{name}[{index}]', origin)
    if set(result) != set(range(256)):
        raise ValueError('Incomplete 256-byte metadata schema')
    return result


def read_image(path, kind):
    data = Path(path).read_bytes()
    if len(data) < 516 or len(data) % 4:
        raise ValueError('Example image must be at least 516 bytes and aligned4')
    meta = data[0x100:0x200]
    expected = b'CAN3' if kind == 'F13' else b'BUR1'
    if meta[:4] != expected or meta[5] != 128 or meta[128:] != b'\xff' * 128:
        raise ValueError('Unsupported metadata header/reserved region')
    if (kind == 'F13' and (meta[4] != 6 or meta[10] != 5)) or (kind == 'F24' and meta[4] != 3):
        raise ValueError('Unsupported example image contract')
    size_offset, start_offset = (0x18, 0x1C) if kind == 'F13' else (0x0C, 0x18)
    size = int.from_bytes(meta[size_offset:size_offset + 4], 'little')
    start = int.from_bytes(meta[start_offset:start_offset + 4], 'little')
    crc = binascii.crc32(data[:-4])
    if size != len(data) or crc != int.from_bytes(data[-4:], 'little'):
        raise ValueError('Example BIN size/CRC mismatch')
    return dict(data=data, meta=meta, size=size, start=start, crc=crc,
                metadata_crc=binascii.crc32(meta), sha=hashlib.sha256(data).hexdigest().upper())


def schema_table(schema):
    lines = ['| Metadata offset | bytes | 欄位 | 產生端來源符號／位置 |', '| --- | --- | --- | --- |']
    for offset, size, name, origin in schema:
        lines.append(f'| 0x{offset:02X}..0x{offset + size - 1:02X} | {size} | {name} | {origin} |')
    return '\n'.join(lines)


def render_tables(f13, f24):
    lines = ['<!-- METADATA_BYTES_BEGIN -->',
             '### F13 (MCU CAN device)：CAN3 欄位與產生端', schema_table(F13),
             '### F24 (MCU CAN host)：BUR1 欄位與產生端', schema_table(F24),
             '### 逐 byte 實體位址與範例值',
             '方括號是欄位內byte索引；LE數值的[0]是最低有效byte。ASCII依字元順序，reserved依原始順序。',
             '範例值取配套BIN，不是所有專案的固定值；每個metadata byte皆各列一行，包含128個reserved FF。']
    a, b = byte_fields(F13), byte_fields(F24)
    for begin in range(0, 256, 32):
        lines += [f'#### Metadata 0x{begin:02X}..0x{begin + 31:02X}',
                  '| offset | F13 address | value | F13欄位[byte] | F24 host address | value | F24欄位[byte] |',
                  '| --- | --- | --- | --- | --- | --- | --- |']
        for offset in range(begin, begin + 32):
            lines.append(f'| 0x{offset:02X} | 0x{f13["start"] + 0x100 + offset:05X} | {f13["meta"][offset]:02X} | {a[offset][0]} | 0x{f24["start"] + 0x100 + offset:05X} | {f24["meta"][offset]:02X} | {b[offset][0]} |')
    lines += ['### 本表範例BIN與CRC位置',
              '| 項目 | F13 (MCU CAN device) | F24 (MCU CAN host) |', '| --- | --- | --- |']
    for label, values in [('image_size', [f'0x{x["size"]:X}' for x in (f13, f24)]),
                          ('stored image CRC32', [f'0x{x["crc"]:08X}' for x in (f13, f24)]),
                          ('CRC32 Flash address', [f'0x{x["start"] + x["size"] - 4:05X}..0x{x["start"] + x["size"] - 1:05X}' for x in (f13, f24)]),
                          ('metadata staging CRC32', [f'0x{x["metadata_crc"]:08X}' for x in (f13, f24)])]:
        lines.append(f'| {label} | {values[0]} | {values[1]} |')
    for name, item in [('F13', f13), ('F24 host', f24)]:
        lines.append(f'{name} 範例BIN SHA-256：`{item["sha"]}`。')
    lines.append('<!-- METADATA_BYTES_END -->')
    return '\n\n'.join(lines[:1]) + '\n' + '\n\n'.join(lines[1:5]) + '\n\n' + '\n'.join(lines[5:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--f13-image', type=Path, required=True)
    parser.add_argument('--f24-image', type=Path, required=True)
    parser.add_argument('--guide', type=Path, default=GUIDE)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    block = render_tables(read_image(args.f13_image, 'F13'), read_image(args.f24_image, 'F24'))
    original = args.guide.read_text(encoding='utf-8-sig')
    pattern = r'<!-- METADATA_BYTES_BEGIN -->.*?<!-- METADATA_BYTES_END -->'
    matches = list(re.finditer(pattern, original, re.S))
    if len(matches) != 1:
        raise ValueError('Expected exactly one managed metadata table block')
    if args.check:
        if matches[0].group() != block:
            raise ValueError('Metadata reference differs from selected example BINs')
    else:
        args.guide.write_text(re.sub(pattern, lambda _: block, original, flags=re.S), encoding='utf-8')
    print('PASS: 256 byte-address rows, two schemas and example CRCs generated/checked; BINs unchanged.')


if __name__ == '__main__':
    main()
