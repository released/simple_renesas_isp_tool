"""C-header fixtures for the independent CAN-device authoring tests."""
import re

FIELDS = {'version': 'VERSION', 'build': 'BUILD', 'target': 'TARGET_ID',
          'role': 'ROLE', 'transport': 'UPDATE_TRANSPORTS', 'version_control': 'VERSION_CONTROL'}


def image_header(values):
    values = dict(values)
    values.setdefault('version_control', 0)
    lines = ['#ifndef APP_IMAGE_CONFIG_H', '#define APP_IMAGE_CONFIG_H']
    for key, value in values.items():
        if key in ('role', 'transport'):
            literal = '1U' if value == ('DEVICE_APP' if key == 'role' else 'CAN') else '2U'
        elif isinstance(value, bool):
            literal = 'true' if value else 'false'
        elif isinstance(value, str):
            literal = value + 'UL' if re.fullmatch(r'0x[0-9A-Fa-f]+', value) else '"' + value + '"'
        else:
            literal = str(value)
        lines.append('#define APP_IMAGE_' + FIELDS.get(key, key.upper()) + ' (' + literal + ')')
    return '\n'.join(lines + ['#endif', ''])


def irq_header(mapping):
    entries = ['    X(' + key + ', ' + value + ')' for key, value in mapping.items()]
    body = '#define APP_IRQ_HANDLERS(X)'
    if entries:
        body += ' \\\n' + ' \\\n'.join(entries)
    return '#ifndef APP_IRQ_HANDLERS_H\n#define APP_IRQ_HANDLERS_H\n' + body + '\n#endif\n'


def read_irq_header(text):
    return {'0x%02X' % int(vector, 0): symbol for vector, symbol in
            re.findall(r'\bX\(\s*(0x[0-9A-Fa-f]+|[0-9]+)\s*,\s*(\w+)\s*\)', text)}


def image_examples(text):
    examples = []
    for block in re.findall(r'```c\n(.*?)\n```', text, re.S):
        if '#ifndef APP_IMAGE_CONFIG_H' not in block:
            continue
        values = {}
        for key, macro in FIELDS.items():
            literal = re.search(r'#define\s+APP_IMAGE_' + macro + r'\s+\(([^)]+)\)', block)[1]
            number = int(literal.rstrip('uUlL'), 0)
            values[key] = ('DEVICE_APP' if number == 1 else 'BRIDGE_APP') if key == 'role' else (
                ('CAN' if number == 1 else 'I2C') if key == 'transport' else
                ('0x%08X' % number if key == 'version' else '0x%04X' % number if key == 'target' else number))
        examples.append(values)
    return examples
