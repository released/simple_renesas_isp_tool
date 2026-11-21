// Reviewed F13 serial measurements; original captures remain local.
import assert from 'node:assert/strict';
import {readFile, writeFile, mkdir} from 'node:fs/promises';
import {join, dirname, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

export async function syncF13SerialEvidence(tool, web, check = false) {
  const read = path => readFile(path, 'utf8').then(s => s.replace(/^\uFEFF/, '').replace(/\r\n?/g, '\n'));
  const escape = s => String(s).replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;');
  async function output(path, text) {
    if (check) assert.equal(await read(path), text, 'F13 serial projection: ' + path);
    else { await mkdir(dirname(path), {recursive:true}); await writeFile(path, text); }
  }
  for (const bus of ['uart', 'xmodem']) {
    const label = bus === 'uart' ? 'UART ISP' : 'Xmodem';
    const sizes = bus === 'uart' ? [10556, 24576, 57344, 121856] : [12636, 24576, 57344, 121856];
    const directory = join(tool, 'docs/data/f13_' + bus + '_update_timing');
    const rows = [];
    let transfer;
    for (const suffix of [...sizes.map(n => n + 'b'), 'transfer']) {
      const name = 'f13_' + bus + '_' + suffix + '.json';
      const text = await read(join(directory, name));
      const item = JSON.parse(text);
      assert.equal(item.schema, 1); assert.equal(item.result, 'PASS');
      assert.ok(!/[A-Za-z]:[\\/]|SourceCode|_Avery|COM\d+/.test(text));
      if (suffix === 'transfer') {
        transfer = item;
        assert.equal(item.flash_commands, false);
        assert.equal(item.payload_bytes_per_row, 8);
        assert.equal(item.device_uart_before.rx_errors, bus === 'uart' ? 1159 : 658);
        assert.deepEqual(item.device_uart_before, item.device_uart_after);
        assert.equal(item.earlier_error_cause_established, false);
        assert.match(item.source_tool_log_sha256, /^[A-F0-9]{64}$/);
        assert.match(item.source_uart_log_sha256, /^[A-F0-9]{64}$/);
      } else {
        assert.equal(item.image.target, 'R5F10BMG');
        assert.equal(item.image.bytes + 'b', suffix);
        assert.match(item.image.sha256, /^[A-F0-9]{64}$/);
        assert.match(item.source_report_sha256, /^[A-F0-9]{64}$/);
        assert.equal(item.program.last_confirmed, 'app_confirmed');
        assert.equal(item.program.start, 'acknowledged'); assert.equal(item.program.end, 'acknowledged');
        assert.equal(item.program.accepted_bytes, item.image.bytes);
        assert.equal(item.installed_metadata_bytes_match, 256);
        assert.equal(item.host_counter_assessment, 'UNAVAILABLE');
        assert.equal(item.cold_start_verified, false); assert.equal(item.uart0_z_reset_evidenced, false);
        assert.equal(item.device_uart_errors + item.device_uart_drops + item.device_uart_tx_timeouts, 0);
        rows.push(item);
      }
      const target = join(web, 'slide_simple_renesas_isp_tool/data', name);
      await output(target, text);
      const cells = Object.entries(item).map(([k,v]) => `<tr><th>${escape(k)}</th><td><pre>${escape(typeof v === 'object' ? JSON.stringify(v,null,2) : v)}</pre></td></tr>`).join('\n');
      await output(target.replace(/\.json$/, '.html'), `<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>F13 ${label} 量測摘要</title><style>body{font:16px system-ui;margin:2rem;max-width:70rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0}th{width:15rem;background:#eee}@media(max-width:600px){body{margin:1rem}table,tbody,tr,th,td{display:block;width:auto}tr{margin:.8rem 0}th{border-bottom:0}}</style><h1>F13 ${label} 量測摘要</h1><p><a href="../serial-isp.html#f13-${bus}-update-measurements">操作、量測與限制</a></p><p>選定報告欄位；成功操作與累積錯誤快照分別呈現。</p><table>${cells}</table></html>\n`);
    }
    const seconds = n => (n / 1000).toFixed(3);
    const table = rows.map((r,i) => `| ${i === 0 ? '原始 App' : r.image.bytes / 1024 + ' KiB'} | ${r.image.bytes} | ${seconds(r.phases_ms['3/5 Write'])} | ${seconds(r.phases_ms['4/5 END CRC'])} | ${seconds(r.phases_ms['5/5 Confirm App'])} | ${seconds(r.elapsed_ms)} | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_${bus}_${r.image.bytes}b.html) |`).join('\n');
    const marker = 'F13_' + bus.toUpperCase() + '_MEASURED';
    const block = `<!-- ${marker}_BEGIN -->
<a id="f13-${bus}-update-measurements"></a>

#### F13 ${label} App 更新量測

- Device：R5F10BMG；F24 bridge；Device UART1 與 Host Link 各為 115200／8-N-1。Tool 0.1.0.139，Host FW 1.8.1 build 20261023。
- 四份報告與實際 BIN 的大小、CRC32、SHA256 相符。START、END／EOT、完整接受 bytes、APP_READY 及全部 256-byte installed metadata 均已確認。
- UART0 有四次 boot-validated／update completed、後續 counter 與完整 framed RX／TX；payload 每列 8 bytes。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
${table}

- 更新後 Device UART 快照的 RX errors／drops／TX timeouts 為 0；Bridge UART1 deltas 的 RX errors／drops 為 0，每次記有 backend aborts=1。abort 計數與 Device UART 錯誤為不同觀察項。
- Host counter assessment 為 **UNAVAILABLE**，Host Link retry 為 0；不能由此推定所有 Host 計數全程零錯誤。
- 獨立 Transfer 的 PING／ECHO／counters${bus === 'xmodem' ? ' 及 CRC128 streaming' : ''} 通過，沒有 Flash 命令。${transfer.trace_rx_tx_pairs} 組 UART0 RX／TX 的 sequence、command、payload 與八位元組分行相符。[Transfer 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_${bus}_transfer.html)
- Transfer 成功段前後的累積 Device RX errors 均為 **${transfer.device_uart_before.rx_errors}**，drops／TX timeouts 為 0；不能與更新後 App 的 0 跨執行相減。${bus === 'uart' ? '較早曾有 Host command 0x61 INTERNAL_ERROR；其原因未由本批資料確立。' : ''}
- 本批涵蓋正常更新後 App 確認；z/Z 人工重置、冷啟動、斷電及其他型號的結果另行驗證。
<!-- ${marker}_END -->`;
    for (const name of ['docs/SERIAL_UPDATE_MEASUREMENTS_zh-TW.md']) {
      const path = join(tool,name), source = await read(path);
      const pattern = new RegExp('<!-- ' + marker + '_BEGIN -->[^]*?<!-- ' + marker + '_END -->','g');
      const matches = [...source.matchAll(pattern)]; assert.ok(matches.length <= 1);
      const anchor = name.includes('DEVICE_APP_') ? '<!-- DEVICE_UPGRADE_F13_' + bus.toUpperCase() + '_END -->' : '<!-- UART_TOOL_GUIDE_END -->';
      const updated = matches.length ? source.replace(matches[0][0],block) : source.replace(anchor,block+'\n\n'+anchor);
      assert.ok(updated.includes(block)); await output(path,updated);
    }
  }
  console.log('PASS F13 UART/Xmodem eight updates and two qualified Transfer summaries');
}
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  assert.ok(process.argv[2]);
  await syncF13SerialEvidence(resolve(fileURLToPath(new URL('../',import.meta.url))),resolve(process.argv[2]),process.argv.includes('--check'));
}
