// Project only explicitly reviewed, portable update summaries; never scan runtime reports.
import assert from 'node:assert/strict';
import {readFile,readdir,mkdir,writeFile} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const tool=resolve(fileURLToPath(new URL('../',import.meta.url)));
const web=resolve(process.argv[2]||'');
assert.ok(process.argv[2],'explicit WebRoot required');
const check=process.argv.includes('--check');
const source=join(tool,'docs/data/f24_i2c_update_timing');
const destination=join(web,'slide_simple_renesas_isp_tool/data');
const guide=join(tool,'docs/ISP_UART_TOOL_GUIDE_zh-TW.md');
const html=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const read=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,''));
async function output(path,text){
  if(check)assert.equal((await read(path)).replace(/\r\n/g,'\n'),text.replace(/\r\n/g,'\n'),'evidence projection '+path);
  else{await mkdir(resolve(path,'..'),{recursive:true});await writeFile(path,text);}
}
const rows=[];
for(const filename of (await readdir(source)).filter(n=>n.endsWith('.json')).sort()){
  assert.match(filename,/^f24_i2c_\d+b\.json$/,'owned summary filename');
  const s=JSON.parse(await read(join(source,filename)));
  assert.equal(s.schema,1);assert.equal(s.result,'PASS');
  assert.equal(s.interface.rate_hz,100000);assert.equal(s.interface.address_7bit,32);
  assert.equal(s.interface.host_baud,115200);assert.equal(s.interface.protocol,'I2C ISP');
  assert.equal(s.tool,'0.1.0.138');assert.equal(s.host,'RL78/F24 bridge FW 1.8.1 build 20261023');
  assert.equal(s.image.target,'R7F124FPJ5xFB');assert.equal(s.program.last_confirmed,'app_confirmed');
  assert.equal(s.program.start,'acknowledged');assert.equal(s.program.end,'acknowledged');
  assert.equal(s.program.accepted_bytes,s.image.bytes);assert.equal(s.installed_metadata_bytes_match,256);
  assert.equal(s.mcu_app_boot_validated,true);assert.equal(s.cold_start_verified,false);
  assert.ok(Number.isSafeInteger(s.image.bytes)&&s.image.bytes>0&&s.image.bytes<=247*1024);
  assert.match(s.source_report_sha256,/^[A-F0-9]{64}$/);assert.match(s.image.sha256,/^[A-F0-9]{64}$/);
  assert.match(s.image.crc32,/^0x[A-F0-9]{8}$/);assert.ok(s.elapsed_ms>0);
  assert.equal(s.host_counter_assessment,'UNAVAILABLE');assert.equal(s.link_retries,0);
  assert.equal(s.app_confirmation_i2c_nack_observations,s.image.bytes===122880?2:s.image.bytes===252928?7:0);
  for(const key of ['1/5 Metadata','2/5 Start','3/5 Write','4/5 END CRC','5/5 Confirm App'])
    assert.ok(Number.isSafeInteger(s.phases_ms[key])&&s.phases_ms[key]>=0,key);
  assert.equal(s.program.end_elapsed_ms,s.phases_ms['4/5 END CRC']);
  assert.ok(Object.values(s.phases_ms).reduce((a,b)=>a+b,0)<=s.elapsed_ms);
  assert.ok(!/[A-Za-z]:[\\/]|SourceCode|_Avery|COM\d+/.test(JSON.stringify(s)),'portable public summary');
  const json=JSON.stringify(s,null,2)+'\n';await output(join(destination,filename),json);
  const cells=Object.entries(s).map(([k,v])=>`<tr><th>${html(k)}</th><td><pre>${html(typeof v==='object'?JSON.stringify(v,null,2):v)}</pre></td></tr>`).join('\n');
  await output(join(destination,filename.replace(/\.json$/,'.html')),`<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>F24 I2C 更新量測摘要</title><style>body{font:16px system-ui;margin:2rem;max-width:70rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0}th{width:15rem;background:#eee}@media(max-width:600px){body{margin:1rem}table,tbody,tr,th,td{display:block;width:auto}tr{margin:.8rem 0}th{border-bottom:0}}</style><h1>F24 I2C 更新量測摘要</h1><p>來源報告的選定欄位；本機路徑與原始擷取保留於工程紀錄。</p><table>${cells}</table></html>\n`);
  rows.push({filename,...s});
}
rows.sort((a,b)=>a.image.bytes-b.image.bytes);
assert.equal(rows.length,6,'reviewed six-image batch');
assert.deepEqual(rows.map(r=>r.image.bytes),[6960,8192,24576,57344,122880,252928]);
const seconds=n=>(n/1000).toFixed(3);
const lines=rows.map(r=>{
  const label=r.image.bytes===6960?'原始 App':(r.image.bytes/1024)+' KiB';
  const url='https://released.github.io/slide_simple_renesas_isp_tool/data/'+r.filename.replace(/\.json$/,'.html');
  return `| ${label} | ${r.image.bytes} | ${seconds(r.phases_ms['3/5 Write'])} | ${seconds(r.phases_ms['4/5 END CRC'])} | ${seconds(r.phases_ms['5/5 Confirm App'])} | ${seconds(r.elapsed_ms)} | [PASS／摘要](${url}) |`;
});
const block=`<!-- F24_I2C_MEASURED_BEGIN -->
<a id="f24-i2c-update-measurements"></a>

#### F24 I2C App 更新量測

量測條件：F24 device R7F124FPJ5xFB，F24 bridge，I2C 100 kHz／7-bit 0x20，Host Link 115200。
Tool 0.1.0.138；Host FW 1.8.1／build 20261023。表格由六份選定更新報告的實際欄位整理，時間單位為秒。

| App BIN | bytes | WRITE | END CRC | App 確認 | 總耗時 | 結果與資料 |
| --- | --- | --- | --- | --- | --- | --- |
${lines.join('\n')}

- 六份映像均取得 START／END 成功回覆、APP_READY，並核對全部 256-byte installed metadata；獨立 UART0 Log 有對應的 boot-validated 啟動文字及後續 counter。
- 總耗時採報告 elapsed_ms，包含 Metadata、START、WRITE、END CRC、App 確認及流程開銷；不採載入 BIN 畫面保留的前次 Result／Elapsed。
- 120／247 KiB 在重啟後唯讀確認期間分別觀察到 2／7 次 I2C_NACK，之後均確認 App。報告 link_retries 為 0 不代表所有 I2C 查詢一次成功。
- Host counter assessment 為 UNAVAILABLE，無法據此宣稱 bus 無錯誤；這與映像／App 確認成功分開判斷。
- 最大測試 App 為 247 KiB，未進入尾端 1 KiB 保留區。結果限此映像與接線條件；冷啟動、斷電恢復、故障注入、其他 MCU 或速度另行驗證。
<!-- F24_I2C_MEASURED_END -->`;
const text=await read(guide),pattern=/<!-- F24_I2C_MEASURED_BEGIN -->[^]*?<!-- F24_I2C_MEASURED_END -->/g;
assert.equal([...text.matchAll(pattern)].length,1,'single measured table owner');
await output(guide,text.replace(pattern,()=>block));
console.log('PASS F24 I2C six reviewed update summaries/table '+(check?'(read-only)':'synchronized'));
