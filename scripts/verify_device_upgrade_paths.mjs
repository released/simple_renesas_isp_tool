// Check model-specific chapters independently of the paired common-topic outline.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {deviceUpgradeContent, publicUpgradeKeys} from './device_upgrade_content.mjs';

const tool = resolve(fileURLToPath(new URL('../', import.meta.url)));
const read = path => readFile(path, 'utf8').then(text =>
  text.replace(/^\uFEFF/, '').replace(/\r\n?/g, '\n'));

export async function verifyDeviceUpgradePaths(web) {
  const content = await deviceUpgradeContent(tool);
  const matrix = {F13: ['CAN', 'UART', 'I2C', 'Xmodem'],
    F24: ['CAN', 'CAN FD', 'UART', 'I2C', 'Xmodem'],
    G16: ['UART', 'I2C', 'Xmodem'], G22: ['UART', 'I2C', 'Xmodem'],
    RH850: ['CAN', 'CAN FD', 'UART', 'I2C', 'Xmodem']};
  const capacities = {F13: 119, F24: 247, G16: 23, G22: 55, RH850: 960};
  const publicScope = content.profiles.length === publicUpgradeKeys.length;
  for (const [model, buses] of Object.entries(matrix)) {
    const expected = publicScope ? buses.filter(bus => publicUpgradeKeys.includes(
      model.toLowerCase() + '-' + bus.toLowerCase().replaceAll(' ', '-'))) : buses;
    assert.deepEqual(content.profiles.filter(profile => profile.model === model).map(profile => profile.bus),
      expected, 'exact selected combinations for ' + model);
  }
  for (const platform of ['RL78', 'RH850']) {
    const root = join(web, 'slide_boot_loader_for_' + platform);
    const home = await read(join(root, 'index.md'));
    for (const page of ['can-device', 'can-fd-device', 'uart-device', 'i2c-device', 'xmodem-device']) {
      assert.ok(home.includes(page+'.html'), 'interface first-entry navigation');
      const text = await read(join(root, page + '.md'));
      const expected = content.chapter(platform, page,'index');
      assert.equal(text.split('<!-- DEVICE_UPGRADE_PATHS_BEGIN -->').length, expected ? 2 : 1,
        'exact owned path block presence');
      if (expected) {
        assert.ok(text.includes(expected), 'canonical model index ' + platform + '/' + page);
        for(const profile of content.profiles.filter(p=>p.platform===platform&&p.page===page))
          assert.ok(text.includes(profile.body),'canonical model details '+profile.key);
      }
    }
  }
  for (const profile of content.profiles) {
    const text = profile.body;
    assert.equal(profile.capacity, capacities[profile.model] * 1024);
    assert.equal(profile.app_start, profile.model === 'RH850' ? 0x10000 : 0x2000);
    for (const value of ['配對工程、接線與映像', 'Tool 更新檔', 'Flash 配置',
      'Boot 接收與驗證機制', 'App 返回 Boot 與 RAM 交接', 'architecture.html#boot-app-handoff',
      'experiments.html#', 'hardware_init()', 'loop()']) {
      assert.ok(text.includes(value), 'specific configuration and single-owner links ' + profile.key + '/' + value);
    }
    assert.ok(!text.includes('### Simple Renesas ISP Tool 操作步驟')&&!text.includes('```mermaid'),
      'no repeated operation or generic flowchart in model configuration');
  }
  await content.measuredAssets(web, async (path, expected) => {
    assert.equal(await read(path), expected.replace(/\r\n?/g, '\n'), 'selected F13 measured artifact');
  });
  console.log(`PASS ${content.profiles.length} model/transport configurations, direct mechanism/evidence links and four selected F13 reports`);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  assert.ok(process.argv[2], 'supply explicit WebRoot');
  await verifyDeviceUpgradePaths(resolve(process.argv[2]));
}
