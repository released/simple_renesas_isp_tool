// Hash-bound UART update summaries; raw captures stay in local engineering records.
import assert from 'node:assert/strict';
import {readFile, writeFile, mkdir} from 'node:fs/promises';
import {join, resolve, dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

export async function syncF24UartEvidence(tool, web, check = false) {
  const read = path => readFile(path, 'utf8').then(s => s.replace(/^\uFEFF/, '').replace(/\r\n?/g, '\n'));
  const escape = value => String(value).replaceAll('&', '&amp;').replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;').replaceAll('"', '&quot;');
  async function output(path, text) {
    if (check) assert.equal(await read(path), text, 'F24 UART evidence projection ' + path);
    else { await mkdir(dirname(path), {recursive: true}); await writeFile(path, text); }
  }
  const rows = [];
  for (const bytes of [6724, 8192, 24576, 57344, 122880, 252928]) {
    const name = `f24_uart_${bytes}b.json`;
    const text = await read(join(tool, 'docs/data/f24_uart_update_timing', name));
    const item = JSON.parse(text);
    assert.equal(item.schema, 1); assert.equal(item.result, 'PASS');
    assert.equal(item.interface.protocol, 'UART ISP');
    assert.equal(item.interface.device_baud, 115200); assert.equal(item.interface.host_baud, 115200);
    assert.equal(item.image.target, 'R7F124FPJ5xFB'); assert.equal(item.image.bytes, bytes);
    assert.match(item.image.sha256, /^[A-F0-9]{64}$/); assert.match(item.source_report_sha256, /^[A-F0-9]{64}$/);
    assert.match(item.image.crc32, /^0x[A-F0-9]{8}$/);
    assert.equal(item.program.last_confirmed, 'app_confirmed');
    assert.equal(item.program.start, 'acknowledged'); assert.equal(item.program.end, 'acknowledged');
    assert.equal(item.program.accepted_bytes, bytes); assert.equal(item.installed_metadata_bytes_match, 256);
    assert.equal(item.mcu_app_boot_validated, true); assert.equal(item.cold_start_verified, false);
    assert.equal(item.host_counter_assessment, 'UNAVAILABLE'); assert.equal(item.link_retries, 0);
    assert.ok(!/[A-Za-z]:[\\/]|SourceCode|_Avery|COM\d+/.test(text), 'portable selected summary');
    assert.equal(item.device_uart.rx_errors + item.device_uart.rx_drops + item.device_uart.tx_timeouts, 0);
    assert.ok(Object.values(item.phases_ms).every(value => Number.isSafeInteger(value) && value >= 0));
    const target = join(web, 'slide_simple_renesas_isp_tool/data', name);
    await output(target, text);
    const cells = Object.entries(item).map(([key, value]) =>
      `<tr><th>${escape(key)}</th><td><pre>${escape(typeof value === 'object' ? JSON.stringify(value, null, 2) : value)}</pre></td></tr>`).join('\n');
    await output(target.replace(/\.json$/, '.html'), `<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>F24 UART 更新量測摘要</title><style>body{font:16px system-ui;margin:2rem;max-width:70rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0}th{width:15rem;background:#eee}@media(max-width:600px){body{margin:1rem}table,tbody,tr,th,td{display:block;width:auto}tr{margin:.8rem 0}th{border-bottom:0}}</style><h1>F24 UART 更新量測摘要</h1><p><a href="../serial-isp.html#f24-uart-update-measurements">條件與時間表</a></p><p>來源報告的選定欄位；映像與報告以 SHA256 識別。</p><table>${cells}</table></html>\n`);
    rows.push(item);
  }
  const seconds = value => (value / 1000).toFixed(3);
  const table = rows.map(item => `| ${item.image.bytes === 6724 ? '原始 App' : (item.image.bytes / 1024) + ' KiB'} | ${item.image.bytes} | ${seconds(item.phases_ms['3/5 Write'])} | ${seconds(item.phases_ms['4/5 END CRC'])} | ${seconds(item.phases_ms['5/5 Confirm App'])} | ${seconds(item.elapsed_ms)} | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_uart_${item.image.bytes}b.html) |`).join('\n');
  const block = `<!-- F24_UART_MEASURED_BEGIN -->
<a id="f24-uart-update-measurements"></a>

#### F24 UART App 更新量測

- Device：R7F124FPJ5xFB；F24 bridge；UART1 115200／8-N-1，Host Link 115200。
- Tool 0.1.0.139；Host FW 1.8.1 build 20261023。六份獨立報告與實際 BIN 的 size／CRC32 相符。
- 六次 START、END 均已確認，接受 bytes 等於映像大小；APP_READY 與全部 256-byte installed metadata 相符，UART0 有 boot-validated、update completed 及後續 counter。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
${table}

- Device UART 的 RX errors、RX drops、TX timeouts 均為 0；這些欄位是 Device 當次執行快照。
- Host counter assessment 為 **UNAVAILABLE**，不能據此推定 Host 全程零錯誤；Host Link retry 為 0。
- 247 KiB 的唯讀 App 確認有 37 次 Host BUSY 觀察，最終確認成功；沒有重送 Flash START／END。
- UART Custom Transfer 及獨立 Xmodem Transfer 均有通過紀錄；Xmodem Boot／App 的更新與 UART0 trace 證據集中於 [F24 Xmodem 更新量測](https://released.github.io/slide_simple_renesas_isp_tool/serial-isp.html#f24-xmodem-update-measurements)。
- 本表涵蓋一般更新與軟體重啟後 App 確認；冷啟動、斷電及其他故障恢復不由本批資料推定。
<!-- F24_UART_MEASURED_END -->`;
  for (const name of ['docs/ISP_UART_TOOL_GUIDE_zh-TW.md', 'docs/DEVICE_APP_UPGRADE_PATHS_zh-TW.md']) {
    const path = join(tool, name), source = await read(path);
    const matches = [...source.matchAll(/<!-- F24_UART_MEASURED_BEGIN -->[^]*?<!-- F24_UART_MEASURED_END -->/g)];
    assert.ok(matches.length <= 1, 'one canonical F24 UART measurement block per guide');
    const updated = matches.length ? source.replace(matches[0][0], block) : name.includes('DEVICE_APP_')
      ? source.replace('<!-- DEVICE_UPGRADE_F24_UART_END -->', block + '\n\n<!-- DEVICE_UPGRADE_F24_UART_END -->')
      : source.replace('<!-- UART_TOOL_GUIDE_END -->', block + '\n\n<!-- UART_TOOL_GUIDE_END -->');
    assert.ok(updated.includes(block), 'canonical insertion anchor');
    await output(path, updated);
  }
  console.log('PASS six F24 UART hash-bound measurement projections');
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  assert.ok(process.argv[2], 'supply explicit WebRoot');
  await syncF24UartEvidence(resolve(fileURLToPath(new URL('../', import.meta.url))), resolve(process.argv[2]), process.argv.includes('--check'));
}
