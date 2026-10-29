// Shared authoring source; platform wiring/Flash/IRQ values stay in their own guide.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';

const read = path => readFile(path, 'utf8').then(text => text.replace(/^\uFEFF/, ''));
export async function serialTransportBodies(tool, platform, {includeTitle=false}={}) {
  assert.ok(['RL78', 'RH850'].includes(platform), 'known guide platform');
  const source = await read(join(tool, 'docs/ISP_SERIAL_TRANSPORT_GUIDE_zh-TW.md'));
  const result = {};
  for (const bus of ['uart', 'i2c', 'xmodem']) {
    const marker = bus.toUpperCase() + '_DEVICE';
    const matches = [...source.matchAll(new RegExp('<!-- ' + marker + '_BEGIN -->([^]*?)<!-- ' + marker + '_END -->', 'g'))];
    assert.equal(matches.length, 1, 'one canonical transport body ' + bus);
    let body = matches[0][1].trim();
    body = body.replace('RL78硬體錯誤／丟棄／TX計數飽和於65535，RH850飽和於0xFFFFFFFF；欄位仍統一序列化為32bits。',
      platform === 'RL78' ? 'RL78 硬體錯誤／丟棄／TX 計數飽和於 65535，欄位統一序列化為 32 bits。' :
        'RH850 硬體錯誤／丟棄／TX 計數飽和於 0xFFFFFFFF，欄位統一序列化為 32 bits。');
    body = body.replaceAll('IICA／RIIC', platform === 'RL78' ? 'IICA' : 'RIIC');
    // The page title is already supplied by the paired navigation contract.
    if(!includeTitle)body = body.replace(/^## (UART|I2C|Xmodem) Bootloader 與協定格式\s*$/m, '');
    result[bus + '-device'] = '<!-- ' + marker + '_BEGIN -->\n' + body.trim() + '\n<!-- ' + marker + '_END -->';
  }
  return result;
}

export async function serialPdfSource(tool, platform) {
  const source = platform === 'RL78' ? 'ISP_UART_DEVICE_DESIGN_zh-TW.md' : 'RH850_ISP_DEVICE_DESIGN_zh-TW.md';
  let text = await read(join(tool, 'docs', source));
  text += '\n\n' + Object.values(await serialTransportBodies(tool, platform,{includeTitle:true})).join('\n\n') + '\n';
  const site = 'https://released.github.io/slide_boot_loader_for_' + platform + '/';
  // These targets are in the combined PDF; other links remain real web URLs.
  text = text.replace(/\]\(([^\s)]+)\)/g, (all, url) => {
    let local = url.startsWith(site) ? url.slice(site.length) : url;
    if (/^(?:serial|uart|i2c|xmodem)-device\.html#/.test(local)) {
      const anchor = local.split('#')[1];
      assert.ok(text.includes('id="' + anchor + '"'), 'PDF internal destination ' + anchor);
      return '](#' + anchor + ')';
    }
    if (/^[a-z-]+\.html(?:#|$)/.test(local)) return '](' + site + local + ')';
    return all;
  });
  return text;
}
