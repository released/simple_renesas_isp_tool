// Explicitly reviewed G16 I2C summaries; never discover reports from runtime folders.
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {join,resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';

export async function syncG16I2cEvidence(tool,web,check=false) {
  const read=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,'').replace(/\r\n?/g,'\n'));
  const escape=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  async function output(path,text) {
    if(check)assert.equal(await read(path),text.replace(/\r\n?/g,'\n'),'G16 I2C evidence projection '+path);
    else {await mkdir(dirname(path),{recursive:true});await writeFile(path,text);}
  }
  const rows=[];
  let transfer;
  for(const suffix of ['12116b','23552b','transfer']) {
    const name='g16_i2c_'+suffix+'.json';
    const text=await read(join(tool,'docs/data/g16_i2c_update_timing',name));
    const s=JSON.parse(text);
    assert.equal(s.schema,1);assert.equal(s.result,'PASS');
    assert.ok(!/[A-Za-z]:[\\/]|SourceCode|_Avery|COM\d+/.test(text),'portable public fields');
    assert.equal(s.payload_bytes_per_row,8);assert.equal(s.uart0_z_reset_evidenced,false);
    if(suffix==='transfer') {
      transfer=s;
      assert.equal(s.device,'R5F121BCAFP');assert.equal(s.scope,'Discover / INFO only');
      assert.equal(s.discovery_passed,true);assert.equal(s.flash_commands,false);
      assert.equal(s.ping_echo_counters_evidenced,false);assert.equal(s.trace_rx_tx_pairs,1);
      assert.match(s.source_tool_log_sha256,/^[A-F0-9]{64}$/);assert.match(s.source_uart_log_sha256,/^[A-F0-9]{64}$/);
    } else {
      assert.equal(s.image.target,'R5F121BCAFP');assert.equal(s.image.bytes+'b',suffix);
      assert.equal(s.interface.protocol,'I2C ISP');assert.equal(s.interface.rate_hz,100000);
      assert.equal(s.interface.address_7bit,32);assert.equal(s.interface.host_baud,115200);
      assert.equal(s.program.last_confirmed,'app_confirmed');
      assert.equal(s.program.start,'acknowledged');assert.equal(s.program.end,'acknowledged');
      assert.equal(s.program.accepted_bytes,s.image.bytes);assert.equal(s.installed_metadata_bytes_match,256);
      assert.equal(s.mcu_app_boot_validated,true);assert.equal(s.cold_start_verified,false);
      assert.equal(s.host_counter_assessment,'UNAVAILABLE');assert.equal(s.link_retries,0);
      assert.equal(s.app_confirmation_i2c_nack_observations,0);assert.equal(s.uart0_enter_tx_trace_evidenced,false);
      assert.match(s.image.sha256,/^[A-F0-9]{64}$/);assert.match(s.source_report_sha256,/^[A-F0-9]{64}$/);
      assert.match(s.image.crc32,/^0x[A-F0-9]{8}$/);
      assert.ok(s.elapsed_ms>0 && Object.values(s.phases_ms).every(n=>Number.isSafeInteger(n)&&n>=0));
      assert.equal(s.program.end_elapsed_ms,s.phases_ms['4/5 END CRC']);
      assert.ok(Object.values(s.phases_ms).reduce((a,b)=>a+b,0)<=s.elapsed_ms);
      rows.push(s);
    }
    const target=join(web,'slide_simple_renesas_isp_tool/data',name);
    await output(target,text);
    const cells=Object.entries(s).map(([k,v])=>`<tr><th>${escape(k)}</th><td><pre>${escape(typeof v==='object'?JSON.stringify(v,null,2):v)}</pre></td></tr>`).join('\n');
    await output(target.replace(/\.json$/,'.html'),`<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>G16 I2C 量測摘要</title><style>body{font:16px system-ui;margin:2rem;max-width:70rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0}th{width:15rem;background:#eee}@media(max-width:600px){body{margin:1rem}table,tbody,tr,th,td{display:block;width:auto}tr{margin:.8rem 0}th{border-bottom:0}}</style><h1>G16 I2C 量測摘要</h1><p><a href="../serial-isp.html#g16-i2c-update-measurements">操作、量測與條件</a></p><p>選定報告欄位；Discover、更新與 UART0 擷取範圍分別呈現。</p><table>${cells}</table></html>\n`);
  }
  const seconds=n=>(n/1000).toFixed(3);
  const table=rows.map((r,i)=>`| ${i===0?'原始 App':'23 KiB'} | ${r.image.bytes} | ${seconds(r.phases_ms['3/5 Write'])} | ${seconds(r.phases_ms['4/5 END CRC'])} | ${seconds(r.phases_ms['5/5 Confirm App'])} | ${seconds(r.elapsed_ms)} | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_i2c_${r.image.bytes}b.html) |`).join('\n');
  const block=`<!-- G16_I2C_MEASURED_BEGIN -->
<a id="g16-i2c-update-measurements"></a>

#### G16 I2C App 更新量測

配對接線、操作步驟與 Boot／App 流程見 [G16 × I2C](https://released.github.io/slide_boot_loader_for_RL78/i2c-device.html#upgrade-g16-i2c)。

- Device：R5F121BCAFP，HOCO／CPU 16 MHz；F24 bridge；I2C 100 kHz／7-bit 0x20；Host Link 115200。Tool 0.1.0.139，Host FW 1.8.1 build 20261023。
- 兩份報告與對應 BIN 的長度、CRC32 相符，映像及來源報告由 SHA256 識別。START／END 成功、完整接受 bytes、APP_READY 與全部 256-byte installed metadata 均已確認。
- 獨立 UART0 擷取有兩次 boot-validated／update completed 與後續 counter；唯讀 metadata 回覆與兩份 BIN 的 256 bytes 分別相符。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
${table}

- 最大完整 App BIN 為 23 KiB，範圍至 \`0x07BFF\`；Boot 8 KiB 與尾端 \`0x07C00..0x07FFF\` 保留區不在 App 更新範圍。
- 第二次更新從執行中的 App 經 ENTER 返回 Boot。UART0 擷取有 ENTER RX，沒有對應 TX trace；Tool 的 Boot 確認與後續更新成功分開記錄，不以此推定回覆遺失原因。
- Host counter assessment 為 **UNAVAILABLE**，Host Link retry 為 0；本段未觀察到 App 確認 I2C_NACK，仍不能推定所有 bus 計數全程零錯誤。
- 獨立 Transfer 的本批擷取涵蓋 Discover／INFO、一組 RX／TX、每列 8 bytes，沒有 Flash 命令；PING／ECHO／counters 尚未由此擷取證實。[Discover 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_i2c_transfer.html)
- 結果限正常更新後 App 確認。UART0 z/Z 人工重置、冷啟動、斷電恢復、其他介面／型號與故障注入另行驗證。
<!-- G16_I2C_MEASURED_END -->`;
  for(const name of ['docs/SERIAL_UPDATE_MEASUREMENTS_zh-TW.md']) {
    const path=join(tool,name),text=await read(path);
    const pattern=/<!-- G16_I2C_MEASURED_BEGIN -->[^]*?<!-- G16_I2C_MEASURED_END -->/g;
    const matches=[...text.matchAll(pattern)];assert.ok(matches.length<=1);
    const anchor=name.includes('DEVICE_APP_')?'<!-- DEVICE_UPGRADE_G16_I2C_END -->':'<!-- F24_I2C_MEASURED_END -->';
    assert.ok(matches.length||text.includes(anchor),'G16 evidence insertion owner');
    const updated=matches.length?text.replace(matches[0][0],block):name.includes('DEVICE_APP_')?text.replace(anchor,block+'\n\n'+anchor):text.replace(anchor,anchor+'\n\n'+block);
    await output(path,updated);
  }
  assert.ok(transfer);
  console.log('PASS G16 I2C two reviewed updates and qualified Discover-only Transfer evidence'+(check?' (read-only)':''));
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  assert.ok(process.argv[2],'explicit WebRoot required');
  await syncG16I2cEvidence(resolve(fileURLToPath(new URL('../',import.meta.url))),resolve(process.argv[2]),process.argv.includes('--check'));
}
