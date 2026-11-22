// Explicit reviewed G16 UART/Xmodem evidence; raw captures remain local.
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {join,dirname,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

export async function syncG16SerialEvidence(tool,web,check=false){
  const read=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,'').replace(/\r\n?/g,'\n'));
  const escape=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  async function output(path,text){
    if(check)assert.equal(await read(path),text,'G16 serial projection '+path);
    else{await mkdir(dirname(path),{recursive:true});await writeFile(path,text);}
  }
  for(const bus of ['uart','xmodem']){
    const label=bus==='uart'?'UART ISP':'Xmodem';
    const sizes=bus==='uart'?[10240,23552]:[12304,23552];
    const rows=[];let transfer;
    for(const suffix of [...sizes.map(n=>n+'b'),'transfer']){
      const name='g16_'+bus+'_'+suffix+'.json';
      const text=await read(join(tool,'docs/data/g16_'+bus+'_update_timing',name));
      const item=JSON.parse(text);
      assert.equal(item.schema,1);assert.equal(item.result,'PASS');
      assert.ok(!/[A-Za-z]:[\\/]|SourceCode|_Avery|COM\d+/.test(text),'portable public summary');
      assert.equal(item.uart0_z_reset_evidenced,false);
      if(suffix==='transfer'){
        transfer=item;assert.equal(item.device,'R5F121BCAFP');assert.equal(item.flash_commands,false);
        assert.equal(item.uart0_trace_evidenced,false);
        assert.deepEqual(item.device_uart_before,item.device_uart_after);
        if(bus==='uart'){
          assert.equal(item.crc128_stream_evidenced,false);
          assert.equal(item.transfer_test_pass_count,3);
          assert.equal(item.ping_echo_counters_evidenced,true);
        }else{
          assert.equal(item.scope,'Xmodem Custom Transfer');
          assert.equal(item.verification_basis,'board-confirmed');
          assert.equal(item.transfer_test_passed,true);
        }
        assert.equal(item.device_uart_before.rx_errors,bus==='uart'?119:104);
      }else{
        assert.equal(item.image.target,'R5F121BCAFP');assert.equal(item.image.bytes+'b',suffix);
        assert.equal(item.program.last_confirmed,'app_confirmed');
        assert.equal(item.program.start,'acknowledged');assert.equal(item.program.end,'acknowledged');
        assert.equal(item.program.accepted_bytes,item.image.bytes);assert.equal(item.installed_metadata_bytes_match,256);
        assert.equal(item.interface.device_baud,115200);assert.equal(item.interface.host_baud,115200);
        assert.equal(item.payload_bytes_per_row,8);assert.ok(item.uart0_matching_metadata_groups>0);
        assert.equal(item.mcu_app_boot_validated,true);assert.equal(item.cold_start_verified,false);
        assert.equal(item.host_counter_assessment,'UNAVAILABLE');assert.equal(item.link_retries,0);
        assert.equal(item.device_uart_errors+item.device_uart_drops+item.device_uart_tx_timeouts,0);
        assert.equal(item.bridge_backend_aborts,1);
        assert.match(item.image.sha256,/^[A-F0-9]{64}$/);assert.match(item.source_report_sha256,/^[A-F0-9]{64}$/);
        rows.push(item);
      }
      const target=join(web,'slide_simple_renesas_isp_tool/data',name);
      await output(target,text);
      const cells=Object.entries(item).map(([k,v])=>`<tr><th>${escape(k)}</th><td><pre>${escape(typeof v==='object'?JSON.stringify(v,null,2):v)}</pre></td></tr>`).join('\n');
      await output(target.replace(/\.json$/,'.html'),`<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>G16 ${label} 量測摘要</title><style>body{font:16px system-ui;margin:2rem;max-width:70rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0}th{background:#eee}@media(max-width:600px){body{margin:1rem}table,tbody,tr,th,td{display:block;width:auto}}</style><h1>G16 ${label} 量測摘要</h1><p><a href="../../slide_boot_loader_for_RL78/experiments.html#g16-${bus}-update-measurements">實驗條件與限制</a></p><p>選定報告欄位；Device 累積快照、Bridge 觀察與更新結果分別呈現。</p><table>${cells}</table></html>\n`);
    }
    const sec=n=>(n/1000).toFixed(3);
    const table=rows.map((r,i)=>`| ${i===0?'原始 App':'23 KiB'} | ${r.image.bytes} | ${sec(r.phases_ms['3/5 Write'])} | ${sec(r.phases_ms['4/5 END CRC'])} | ${sec(r.phases_ms['5/5 Confirm App'])} | ${sec(r.elapsed_ms)} | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_${bus}_${r.image.bytes}b.html) |`).join('\n');
    const marker='G16_'+bus.toUpperCase()+'_MEASURED';
    const block=`<!-- ${marker}_BEGIN -->
<a id="g16-${bus}-update-measurements"></a>

#### G16 ${label} App 更新量測

配對工程、接線與 Boot／App 機制見 [G16 × ${label}](uart-device.html#upgrade-g16-uart)。

- Device：R5F121BCAFP，HOCO／CPU 16 MHz；F24 bridge；Device UART1 與 Host Link 各為 115200／8-N-1。Tool 0.1.0.146，Host FW 1.8.1 build 20261023。
- ${bus==='uart'?'兩份':'三份'}報告與實際 BIN 的長度、CRC32 相符，來源報告及映像由 SHA256 識別。START、END／EOT、完整接受 bytes、APP_READY 與全部 256-byte installed metadata 均已確認。
- UART0 擷取各有兩次 boot-validated／update completed、後續 counter 與完整 RX／TX。payload 每列最多 8 bytes；四頁 metadata 組合分別與兩份 BIN 的 256 bytes 相同。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
${table}

${bus==='xmodem'?'- 原始 App 另有一次 14.949 秒的 PASS 報告；表列相同映像的另一次 14.917 秒紀錄。兩次均確認 APP_READY／installed metadata。\n':''}- 最大完整 App BIN 為 23 KiB，範圍至 \`0x07BFF\`；Boot 8 KiB 與 \`0x07C00..0x07FFF\` 保留區不在 App 更新範圍。第二份映像從執行中的 App 經 ENTER 返回 Boot 更新。
- 更新後 Device UART 快照的 RX errors／drops／TX timeouts 為 0；Bridge RX errors／drops 為 0，backend aborts=1 仍保留。Host counter assessment 為 **UNAVAILABLE**、Host Link retry 為 0，不推定全程零錯誤。
- ${bus==='uart'?'獨立 UART Transfer 的 PING／ECHO／counters 與 final INFO 三次通過，沒有 Flash 命令。':'獨立 Xmodem Transfer 已通過實板驗證；Discover／INFO 三次成功，沒有 Flash 命令。'}[Transfer 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_${bus}_transfer.html)
- Transfer 成功段的累積 Device RX errors 保持 **${transfer.device_uart_before.rx_errors}**，drops／TX timeouts 為 0；此批未提供 Transfer UART0 擷取，不能與 App 的 0 跨執行相減。
- 結果限正常更新後 App 確認；z/Z 人工重置、冷啟動、斷電恢復及其他介面／型號另行驗證。
<!-- ${marker}_END -->`.replace('(uart-device.html#upgrade-g16-uart)',`(${bus}-device.html#upgrade-g16-${bus})`);
    const path=join(tool,'docs/SERIAL_UPDATE_MEASUREMENTS_zh-TW.md');
    const source=await read(path);
    const pattern=new RegExp('<!-- '+marker+'_BEGIN -->[^]*?<!-- '+marker+'_END -->','g');
    const matches=[...source.matchAll(pattern)];assert.ok(matches.length<=1);
    const anchor='<!-- F24_'+bus.toUpperCase()+'_MEASURED_END -->';
    assert.ok(matches.length||source.includes(anchor),'same-interface measurement owner');
    await output(path,matches.length?source.replace(matches[0][0],block):source.replace(anchor,anchor+'\n\n'+block));
  }
  console.log('PASS reviewed G16 UART/Xmodem evidence and qualified Transfer scopes'+(check?' (read-only)':''));
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  assert.ok(process.argv[2],'explicit WebRoot required');
  await syncG16SerialEvidence(resolve(fileURLToPath(new URL('../',import.meta.url))),resolve(process.argv[2]),process.argv.includes('--check'));
}
