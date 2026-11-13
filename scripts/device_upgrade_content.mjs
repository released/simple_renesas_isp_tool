// Model/transport paths are authored once, then embedded in existing chapters.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';

// Physical acceptance selects this complete public subset; the full local
// engineering matrix remains independently checked and is never inferred live.
export const publicUpgradeKeys = Object.freeze([
  'f13-can', 'f13-i2c', 'f24-can', 'f24-can-fd', 'f24-i2c', 'rh850-can', 'rh850-can-fd'
]);

export async function deviceUpgradeContent(tool) {
  const read = path => readFile(path, 'utf8').then(text =>
    text.replace(/^\uFEFF/, '').replace(/\r\n?/g, '\n'));
  const profiles = JSON.parse(await read(join(tool, 'docs/data/device_upgrade_paths.json')));
  const source = await read(join(tool, 'docs/DEVICE_APP_UPGRADE_PATHS_zh-TW.md'));
  if (profiles.length === publicUpgradeKeys.length) {
    assert.deepEqual(profiles.map(profile => profile.key).sort(), [...publicUpgradeKeys].sort(),
      'exact board-accepted public guide subset');
  } else {
    assert.equal(profiles.length, 20, 'complete current engineering model/transport scope');
  }
  const keys = new Set();
  for (const profile of profiles) {
    assert.ok(!keys.has(profile.key), 'unique upgrade path');
    keys.add(profile.key);
    const marker = 'DEVICE_UPGRADE_' + profile.key.toUpperCase().replaceAll('-', '_');
    const matches = [...source.matchAll(new RegExp(
      '<!-- ' + marker + '_BEGIN -->([^]*?)<!-- ' + marker + '_END -->', 'g'))];
    assert.equal(matches.length, 1, 'one authored path ' + profile.key);
    profile.body = matches[0][1].trim();
    assert.ok(profile.body.includes('id="upgrade-' + profile.key + '"'), 'stable path anchor');
    assert.ok(profile.body.includes('`' + profile.boot + '`'), 'exact Boot project');
    assert.ok(profile.body.includes('`' + profile.app + '`'), 'exact App project');
    assert.equal((profile.body.match(/```mermaid/g) || []).length, 2,
      'independent Boot and App diagrams ' + profile.key);
  }
  const index = platform => {
    const rows = ['<a id="device-upgrade-paths"></a>', '', '## 依型號與周邊更新 App', '',
      '各入口包含接線、配對映像、Tool 操作、Boot 接收驗證及 App 返回 Boot 流程。', '',
      '| 型號 | Classical CAN | CAN FD | UART ISP | I2C ISP | Xmodem |',
      '| --- | --- | --- | --- | --- | --- |'];
    const selected = profiles.filter(profile => profile.platform === platform);
    for (const model of new Set(selected.map(profile => profile.model))) {
      const cells = ['CAN', 'CAN FD', 'UART', 'I2C', 'Xmodem'].map(bus => {
        const profile = selected.find(item => item.model === model && item.bus === bus);
        return profile ? `[操作與機制](${profile.page}.html#upgrade-${profile.key})` : '—';
      });
      rows.push(`| ${model} | ${cells.join(' | ')} |`);
    }
    rows.push('', '「—」表示本指南沒有該型號與介面的配對工程。');
    return rows.join('\n');
  };
  const chapter = (platform, page) => {
    const selected = profiles.filter(profile => profile.platform === platform && profile.page === page);
    if (!selected.length) return '';
    const rows = ['<!-- DEVICE_UPGRADE_PATHS_BEGIN -->', '## 依型號閱讀完整更新流程', '',
      '| 型號與介面 | 章節 |', '| --- | --- |'];
    for (const profile of selected) {
      rows.push(`| ${profile.model} / ${profile.bus} | [操作、Boot 與 App](#upgrade-${profile.key}) |`);
    }
    return rows.join('\n') + '\n\n' + selected.map(profile => profile.body).join('\n\n') +
      '\n<!-- DEVICE_UPGRADE_PATHS_END -->';
  };
  const measuredAssets = async (web, output) => {
    const profile = profiles.find(item => item.key === 'f13-i2c');
    assert.equal(profile.measurements.length, 4, 'four explicitly reviewed F13 I2C reports');
    const escape = value => String(value).replaceAll('&', '&amp;')
      .replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
    for (const name of profile.measurements) {
      assert.match(name, /^f13_i2c_\d+b\.json$/);
      const source = await read(join(tool, 'docs/data/f13_i2c_update_timing', name));
      const summary = JSON.parse(source);
      assert.equal(summary.result, 'PASS');
      assert.equal(summary.image.target, 'R5F10BMG');
      assert.equal(summary.program.last_confirmed, 'app_confirmed');
      assert.equal(summary.program.start, 'acknowledged');
      assert.equal(summary.program.end, 'acknowledged');
      assert.equal(summary.program.accepted_bytes, summary.image.bytes);
      assert.equal(summary.installed_metadata_bytes_match, 256);
      assert.equal(summary.mcu_app_boot_validated, true);
      assert.equal(summary.cold_start_verified, false);
      assert.equal(summary.interface.address_7bit, 32);
      assert.equal(summary.interface.rate_hz, 100000);
      assert.equal(summary.host_counter_assessment, 'UNAVAILABLE');
      assert.match(summary.source_report_sha256, /^[0-9A-F]{64}$/);
      assert.match(summary.image.sha256, /^[0-9A-F]{64}$/);
      assert.ok(!/[A-Za-z]:[\\/]|SourceCode|_Avery|COM\d+/.test(source), 'portable selected evidence');
      const root = join(web, 'slide_boot_loader_for_RL78/data');
      await output(join(root, name), source);
      const rows = Object.entries(summary).map(([key, value]) =>
        `<tr><th>${escape(key)}</th><td><pre>${escape(typeof value === 'object' ? JSON.stringify(value, null, 2) : value)}</pre></td></tr>`).join('\n');
      await output(join(root, name.replace('.json', '.html')),
        `<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>F13 I2C 更新量測摘要</title><style>body{font:16px system-ui;margin:2rem;max-width:70rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0}th{width:15rem;background:#eee}@media(max-width:600px){body{margin:1rem}table,tbody,tr,th,td{display:block;width:auto}tr{margin:.8rem 0}th{border-bottom:0}}</style><h1>F13 I2C 更新量測摘要</h1><p><a href="../i2c-device.html#f13-i2c-update-measurements">更新時間表與條件</a></p><p>來源報告的選定欄位；映像與來源報告由 SHA256 識別。</p><table>${rows}</table></html>\n`);
    }
  };
  return {profiles, index, chapter, measuredAssets};
}
