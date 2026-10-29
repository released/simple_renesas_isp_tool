// Canonical RH850 topic projection. Maintainer documentation generation only;
// MCU builds continue to use their local PowerShell/CS+ workflow.
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,readdir,stat} from 'node:fs/promises';
import {resolve,join,basename} from 'node:path';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {bootloaderNavigation} from './bootloader_guide_navigation.mjs';
import {serialTransportBodies} from './serial_guide_content.mjs';
const tool=resolve(fileURLToPath(new URL('../',import.meta.url)));
assert.ok(process.argv[2],'Supply the explicit WebRoot');
const web=resolve(process.argv[2]);
const check=process.argv.includes('--check');
const site='slide_boot_loader_for_RH850',root=join(web,site);
const pending=new Map();
const utf8=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,''));
const hash=b=>createHash('sha256').update(b).digest('hex').toUpperCase();
const html=s=>String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
async function output(path,text){
  let old=null;try{old=await utf8(path);}catch(e){if(e.code!=='ENOENT')throw e;}
  if(old!==text){if(check)throw Error('RH850 guide differs: '+path);pending.set(path,text);}
}
function block(source,name){
  const matches=[...source.matchAll(new RegExp('<!-- '+name+'_BEGIN -->([^]*?)<!-- '+name+'_END -->','g'))];
  assert.equal(matches.length,1,'one canonical block '+name);return matches[0][1].trim();
}
function replaceBlock(source,name,body){
  block(source,name);
  return source.replace(new RegExp('<!-- '+name+'_BEGIN -->[^]*?<!-- '+name+'_END -->'),
    '<!-- '+name+'_BEGIN -->\n'+body+'\n<!-- '+name+'_END -->');
}
function wrap(name,body){return '<!-- '+name+'_BEGIN -->\n'+body.trim()+'\n<!-- '+name+'_END -->';}
function localLinks(source){
  const names={
    'RH850_CAN_DEVICE_GUIDE_zh-TW.md':'can-device.html',
    'RH850_ISP_DEVICE_DESIGN_zh-TW.md':'serial-device.html',
    'RH850_BOOT_APP_PORTING_GUIDE_zh-TW.md':'porting.html',
    'RH850_UPDATE_TIMING_zh-TW.md':'app-development.html#rh850-update-timing',
    'RH850_IMAGE_REFERENCE_zh-TW.md':'reference.html',
  };
  source=source.replaceAll('https://released.github.io/'+site+'/','');
  for(const [from,to] of Object.entries(names))source=source.replaceAll(']('+from,']('+to);
  source=source.replaceAll('](ISP_UART_TOOL_GUIDE_zh-TW.md','](../slide_simple_renesas_isp_tool/serial-isp.html')
    .replaceAll('](SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md','](../slide_simple_renesas_isp_tool/operation.html')
    .replaceAll('](https://released.github.io/slide_simple_renesas_isp_tool/operation.html','](../slide_simple_renesas_isp_tool/operation.html')
    .replaceAll('](ISP_UART_DEVICE_DESIGN_zh-TW.md','](../slide_boot_loader_for_RL78/serial-device.html');
  return source;
}
const layout=JSON.parse(await utf8(join(tool,'scripts/web_guide_layout.json')))[site];
assert.ok(layout,'RH850 chapter layout required');
const reports=[],cachedReports=new Map();
const cacheRoot=join(tool,'docs/data/rh850_update_timing');
try{for(const filename of (await readdir(cacheRoot)).filter(n=>n.endsWith('.json'))){cachedReports.set(filename,JSON.parse(await utf8(join(cacheRoot,filename))));}}catch(e){if(e.code!=='ENOENT')throw e;}
let sourceReports=[];try{sourceReports=await readdir(join(tool,'build/reports'));}catch(e){if(e.code!=='ENOENT')throw e;}
for(const filename of sourceReports.filter(n=>n.endsWith('.json')).sort()){
  const bytes=await readFile(join(tool,'build/reports',filename));
  const report=JSON.parse(bytes.toString('utf8').replace(/^\uFEFF/,'')),u=report.update;
  if(!u||!String(u.target_before).includes('RH850')||!String(u.can).startsWith('CAN FD ')||
     !['PASS','PASS_WITH_WARNINGS'].includes(u.result)||!String(u.details).includes('app_confirmation=PASS'))continue;
  assert.ok(Number.isInteger(u.input_bytes)&&u.input_bytes>0&&u.input_bytes<=0xF0000);
  assert.ok(Number.isInteger(u.elapsed_ms)&&u.elapsed_ms>0);
  const phases=u.phases_ms;for(const key of ['metadata','start','transfer','end_crc','app_confirmation'])assert.ok(Number.isInteger(phases[key])&&phases[key]>=0,key);
  const summary={source_report:filename,source_sha256:hash(bytes),recorded_at_utc:report.recorded_at_utc,
    tool:report.tool,result:u.result,input_bytes:u.input_bytes,elapsed_ms:u.elapsed_ms,
    host_baud:u.host_baud,can:u.can,phases_ms:phases,
    image_crc32:/image_crc32=(0x[0-9A-Fa-f]+)/.exec(u.details)?.[1]||'',
    app_confirmation:'PASS',host_counter_assessment:u.host_counter_assessment,
    host_deltas:u.host_deltas,link_retries:u.link_retries};
  assert.ok(!/[A-Za-z]:[\\/]|_Avery|SourceCode/.test(JSON.stringify(summary)),'portable report summary');
  if(cachedReports.has(filename))assert.equal(cachedReports.get(filename).source_sha256,summary.source_sha256,'measured source report drift');
  cachedReports.set(filename,summary);
  await output(join(cacheRoot,filename),JSON.stringify(summary,null,2)+'\n');
}
for(const [filename,summary] of [...cachedReports].sort(([a],[b])=>a.localeCompare(b))){
  assert.ok(/^[A-Za-z0-9_.-]+\.json$/.test(filename));
  assert.equal(summary.source_report,filename);assert.ok(/^[0-9A-F]{64}$/.test(summary.source_sha256));
  assert.ok(Number.isInteger(summary.input_bytes)&&summary.input_bytes>0);
  const file=filename.replace(/\.json$/,'');
  await output(join(root,'data',filename),JSON.stringify(summary,null,2)+'\n');
  const cells=Object.entries(summary).map(([k,v])=>`<tr><th>${html(k)}</th><td><pre>${html(typeof v==='object'?JSON.stringify(v,null,2):v??'')}</pre></td></tr>`).join('\n');
  await output(join(root,'data',file+'.html'),`<!doctype html><html lang="zh-Hant"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>RH850 更新量測報告摘要</title><style>body{font:16px system-ui;margin:2rem;max-width:70rem}table{border-collapse:collapse;width:100%}th,td{border:1px solid #aaa;padding:.6rem;text-align:left;vertical-align:top}pre{white-space:pre-wrap;overflow-wrap:anywhere;margin:0}th{width:15rem;background:#eee}@media(max-width:600px){body{margin:1rem}table,tbody,tr,th,td{display:block;width:auto}tr{margin:.8rem 0}th{border-bottom:0}td pre{font-size:14px}}</style><h1>RH850 更新量測報告摘要</h1><p><a href="../app-development.html#rh850-update-timing">更新時間表</a> · <a href="${file}.json">JSON 摘要</a></p><p>欄位取自來源 report。映像大小使用本次 input_bytes；診斷與更新完成結果分別保留。來源 report 以檔名與 SHA256 識別。</p><table>${cells}</table></html>\n`);
  reports.push({filename,summary,file});
}
assert.ok(reports.length,'At least one completed RH850 CAN FD report is required');
const byImage=new Map();
for(const report of reports){const u=report.summary;const key=[u.input_bytes,u.image_crc32,u.host_baud,u.can].join('|');if(!byImage.has(key))byImage.set(key,report);}
const measured=[...byImage.values()];
assert.equal(new Set(measured.map(r=>r.summary.host_baud+'|'+r.summary.can)).size,1,'Separate differing timing conditions before comparing');
const timingRows=['| App BIN | bytes | 寫入相關階段 | App 確認 | 總耗時 | 報告 |','| --- | --- | --- | --- | --- | --- |'];
for(const {summary:u,file} of measured){
  const programming=u.phases_ms.metadata+u.phases_ms.start+u.phases_ms.transfer+u.phases_ms.end_crc;
  timingRows.push(`| ${(u.input_bytes/1024).toFixed(2)} KiB | ${u.input_bytes.toLocaleString('en-US')} | ${(programming/1000).toFixed(1)} s | ${(u.phases_ms.app_confirmation/1000).toFixed(1)} s | ${(u.elapsed_ms/1000).toFixed(1)} s | [量測摘要](https://released.github.io/${site}/data/${file}.html) |`);
}
const last=reports.at(-1).summary;
timingRows.push('',`Host Baud：${last.host_baud} bit/s；報告記錄的 bus 條件：${html(last.can)}。`,
  '寫入相關階段為 Metadata＋START＋transfer＋END CRC；總耗時另外包含 App 確認及未單獨分階段的 Tool 開銷。',
  '', '### 分階段原始數值', '', '| 來源報告 | Metadata | START | transfer | END CRC | App 確認 | elapsed_ms |', '| --- | --- | --- | --- | --- | --- | --- |');
for(const {filename,summary:u} of measured)timingRows.push(`| ${filename.replace(/^update_|\.json$/g,'')} | ${u.phases_ms.metadata} ms | ${u.phases_ms.start} ms | ${u.phases_ms.transfer} ms | ${u.phases_ms.end_crc} ms | ${u.phases_ms.app_confirmation} ms | ${u.elapsed_ms} ms |`);
const timingPath=join(tool,'docs/RH850_UPDATE_TIMING_zh-TW.md');
const timing=replaceBlock(await utf8(timingPath),'RH850_TIMING_DATA',timingRows.join('\n'));
await output(timingPath,timing);

// A real packaged build is a format example, never a substitute timing input.
const project=process.env.FIRMWARE_RH850_FD_APP_PROJECT;
const examplePath=join(tool,'docs/data/rh850_image_example.json');
function crc32(b){let c=0xFFFFFFFF;for(const v of b){c^=v;for(let j=0;j<8;j++)c=(c>>>1)^((c&1)?0xEDB88320:0);}return (c^0xFFFFFFFF)>>>0;}
let example;
function hexValue(n,w){return '0x'+n.toString(16).toUpperCase().padStart(w,'0');}
if(project){
 const imageFile=basename(resolve(project))+'_update.bin';
 const image=await readFile(join(resolve(project),'DefaultBuild',imageFile));
 assert.equal(crc32(image.subarray(0,-4)),image.readUInt32LE(image.length-4));
 example={source_file:imageFile,input_bytes:image.length,sha256:hash(image),crc32:hexValue(image.readUInt32LE(image.length-4),8),metadata_hex:image.subarray(0x100,0x200).toString('hex').toUpperCase()};
 await output(examplePath,JSON.stringify(example,null,2)+'\n');
}else{
 try{example=JSON.parse(await utf8(examplePath));}catch{throw Error('First metadata snapshot needs FIRMWARE_RH850_FD_APP_PROJECT; later guide sync uses docs/data/rh850_image_example.json');}
}
assert.ok(/^[A-Za-z0-9_.-]+\.bin$/.test(example.source_file)&&/^[0-9A-F]{64}$/.test(example.sha256));
assert.ok(/^[0-9A-F]{512}$/.test(example.metadata_hex));
const metadata=Buffer.from(example.metadata_hex,'hex');
assert.equal(metadata.readUInt32LE(0),0x004E4143);assert.equal(metadata[4],6);assert.equal(metadata[5],128);
assert.equal(metadata.readUInt16LE(12),0x8501);assert.equal(metadata.readUInt32LE(24),example.input_bytes);
assert.equal(metadata.readUInt32LE(28),0x10000);assert.equal(metadata.readUInt32LE(104),0x31563852);
assert.ok(metadata.subarray(128).every(v=>v===255));
await output(join(root,'data/rh850_image_example.json'),JSON.stringify(example,null,2)+'\n');
const fields=[
 [0,4,'image_magic'],[4,1,'image_format'],[5,1,'core_length'],[6,2,'flags'],[8,2,'protocol_id'],[10,1,'protocol_version'],[11,1,'checksum_type'],
 [12,2,'target_id'],[14,1,'can_mode_mask'],[15,1,'id_format'],[16,4,'app_version'],[20,4,'app_build'],[24,4,'image_size'],[28,4,'app_start'],
 [32,4,'app_capacity'],[36,4,'flash_size'],[40,4,'boot_size'],[44,4,'erase_size'],[48,4,'nominal_mask'],[52,4,'data_mask'],[56,4,'request_id'],[60,4,'response_id'],
 [64,16,'mcu_part'],[80,12,'mcu_family'],[92,4,'minimum_size'],[96,4,'UPD1_magic'],[100,1,'policy_version'],[101,1,'policy_size'],[102,1,'image_role'],[103,1,'update_transports'],
 [104,4,'irq_abi'],[108,4,'minimum_boot'],[112,4,'device_uart_baud'],[116,4,'version_control'],[120,8,'reserved_zero'],[128,128,'reserved_FF']];
const hex=(n,w)=>'0x'+n.toString(16).toUpperCase().padStart(w,'0');
const origin=name=>name==='image_size'?'tools/device_build.ps1':/^app_(version|build)$|^version_control$/.test(name)?'app_image_config.h':
 /protocol|can_mode|id_format|nominal|data_mask|request_id|response_id/.test(name)?'can_boot_config.h + hal/image/image_metadata_profile.c':'image_metadata_config.h + hal/image/image_metadata_profile.c';
const metaRows=[`範例來源：<code>${example.source_file}</code>，${example.input_bytes} bytes；SHA256＝<code>${example.sha256}</code>。`,
 '封裝 BIN 的 CRC32 與 metadata size 已相互核對；此範例值不是可直接沿用的 MCU／Flash 預設。', '', '### MCU CAN device：Metadata 欄位與產生端', '', '| Metadata offset | bytes | 欄位 | 編輯／產生來源 |','| --- | --- | --- | --- |'];
const perByte=new Map();
for(const [at,size,name] of fields){metaRows.push(`| ${hex(at,2)}..${hex(at+size-1,2)} | ${size} | ${name} | ${origin(name)} |`);for(let i=0;i<size;i++){assert.ok(!perByte.has(at+i));perByte.set(at+i,[name,i]);}}
assert.equal(perByte.size,256);
metaRows.push('','<a id="guide-reference-6"></a>','','### 逐 byte 實體位址與範例值','');
for(let start=0;start<256;start+=32){
 metaRows.push('',`#### Metadata ${hex(start,2)}..${hex(start+31,2)}`,'','| offset | Flash address | byte | 欄位內 byte |','| --- | --- | --- | --- |');
 for(let at=start;at<start+32;at++){const [name,i]=perByte.get(at);metaRows.push(`| ${hex(at,2)} | ${hex(0x10100+at,8)} | ${metadata[at].toString(16).toUpperCase().padStart(2,'0')} | ${name}[${i}] |`);}
}
const referencePath=join(tool,'docs/RH850_IMAGE_REFERENCE_zh-TW.md');
const reference=replaceBlock(await utf8(referencePath),'RH850_METADATA_BYTES',metaRows.join('\n'));
await output(referencePath,reference);
const can=await utf8(join(tool,'docs/RH850_CAN_DEVICE_GUIDE_zh-TW.md'));
const porting=await utf8(join(tool,'docs/RH850_BOOT_APP_PORTING_GUIDE_zh-TW.md'));
const serialPath=join(tool,'docs/RH850_ISP_DEVICE_DESIGN_zh-TW.md');
const wire=block(await utf8(join(tool,'docs/ISP_UART_DEVICE_DESIGN_zh-TW.md')),'SERIAL_WIRE_REFERENCE')
 .replaceAll('slide_boot_loader_for_RL78','slide_boot_loader_for_RH850')
 .replace('RL78硬體錯誤／丟棄／TX計數飽和於65535，RH850飽和於0xFFFFFFFF；欄位仍統一序列化為32bits。','RH850硬體錯誤／丟棄／TX計數飽和於0xFFFFFFFF，欄位序列化為32bits。')
 .replaceAll('IICA ISR','RIIC ISR');
const serialCanonical=replaceBlock(await utf8(serialPath),'RH850_SERIAL_WIRE',wire);
await output(serialPath,serialCanonical);
const serial=serialCanonical.replace(/^<a id="article_top"><\/a>\s*# [^\n]+\n/,'');
const bodies={
 ...await serialTransportBodies(tool,'RH850'),
 'can-device':wrap('RH850_DEVICE',['RH850_CAN_INTRO','RH850_BOOT_FLOW','RH850_CAN_FD'].map(k=>block(can,k)).join('\n\n')),
 'app-development':wrap('RH850_APP_CHAPTER',block(can,'RH850_APP'))+'\n\n'+wrap('RH850_TIMING',block(timing,'RH850_TIMING')),
 'serial-device':wrap('RH850_SERIAL_CHAPTER',serial),
 'porting':wrap('RH850_PORTING_CHAPTER',block(porting,'RH850_PORTING').replace('<!-- RH850_FLASH_SLOT -->',block(can,'RH850_FLASH'))),
 'reference':wrap('RH850_REFERENCE_CHAPTER',block(reference,'RH850_REFERENCE')),
 'tool-guides':`## 共用 Tool 操作\n\n| 目的 | 說明位置 |\n| --- | --- |\n| Tool 與 host 韌體 | [下載入口](../slide_simple_renesas_isp_tool/downloads.html) |\n| COM 與 MCU host 識別 | [連線與操作](../slide_simple_renesas_isp_tool/operation.html) |\n| CAN／CAN FD 更新 | [CAN 更新操作](../slide_simple_renesas_isp_tool/can-update.html) |\n| UART／Xmodem／I2C 更新 | [串列操作](../slide_simple_renesas_isp_tool/serial-isp.html) |\n| BIN／LIVE、完成狀態與報告 | [欄位與更新報告](../slide_simple_renesas_isp_tool/status-and-reports.html) |\n| CAN Custom Transfer | [通訊驗證](../slide_simple_renesas_isp_tool/custom-transfer.html) |\n\nCAN Transfer 使用對應模式的獨立 Transfer 映像，選 Custom Transfer → Discover Test Target → Transfer Test；不載入 App BIN。\nDevice 的實體 CAN port 由接線與編譯設定決定，見 [EVM 與 CAN port](can-device.html#rh850-evm-can-ports)。\n\nUART／Xmodem／I2C 的 Boot、App、Transfer 各自是單一資料介面工程，另保留文字 Log；配置見 [串列工程](serial-device.html)。\nApp 更新使用該 App 封裝的 <code>*_update.bin</code>；燒錄器安裝檔與更新 BIN 的用途見 [建置產物](app-development.html#rh850-build-products)。\n`,
};
const pages=Object.keys(layout.pages).filter(n=>n!=='index');
await bootloaderNavigation(tool,web,output,Object.fromEntries(Object.entries(bodies).map(([page,body])=>[page,localLinks(body)])),await serialTransportBodies(tool,'RL78'));
const mapPath=join(root,'guide-map.json');
try{await stat(mapPath);}catch(e){if(e.code!=='ENOENT')throw e;if(check)throw e;pending.set(mapPath,JSON.stringify({version:1,pages:layout.pages,aliases:{}},null,2)+'\n');}
if(!check){
 for(const [path,text] of pending){await mkdir(resolve(path,'..'),{recursive:true});await writeFile(path,text);}
 const result=spawnSync(process.execPath,[join(tool,'scripts/split_web_guides.mjs'),web,'--refresh'],{encoding:'utf8',windowsHide:true});
 assert.equal(result.status,0,result.stdout+result.stderr);
}
console.log(`PASS RH850 canonical chapters/report summaries/256 metadata bytes: ${pages.length} chapters, ${reports.length} measured report(s)${check?' (read-only)':''}`);
