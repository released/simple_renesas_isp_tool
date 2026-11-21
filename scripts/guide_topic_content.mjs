// Reader-purpose projections. Shared facts retain one owner and stable bookmarks.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';
const read=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,'').replace(/\r\n?/g,'\n'));
export const interfaces=[['CAN','can-update','can-device'],['CAN FD','can-fd','can-fd-device'],['I2C','i2c','i2c-device'],['UART','uart','uart-device'],['Xmodem','xmodem','xmodem-device']];
export function marked(source,name){
  const found=[...source.matchAll(new RegExp('<!-- '+name+'_BEGIN -->([^]*?)<!-- '+name+'_END -->','g'))];
  assert.equal(found.length,1,'single source block '+name);return found[0][1].trim();
}
function fullMarked(source,name){
  marked(source,name);
  return source.match(new RegExp('<!-- '+name+'_BEGIN -->[^]*?<!-- '+name+'_END -->'))[0];
}
export function timingBody(workshop){
  const start=workshop.indexOf('<a id="device-update-timing">');
  const end=workshop.indexOf('<!-- DEVICE_WORKSHOP_END -->',start);
  assert.ok(start>=0&&end>start,'canonical RL78 measured region');return workshop.slice(start,end).trim();
}
export async function topicBodies(tool,platform,bodies){
  const rh=platform==='RH850';
  const workshop=await read(join(tool,'docs/DEVICE_METADATA_VECTOR_WORKSHOP_zh-TW.md'));
  const can=await read(join(tool,'docs/RH850_CAN_DEVICE_GUIDE_zh-TW.md'));
  const architecture=await read(join(tool,'docs/BOOTLOADER_ARCHITECTURE_zh-TW.md'));
  bodies.architecture=rh?(await read(join(tool,'docs/RH850_BOOTLOADER_ARCHITECTURE_zh-TW.md'))).replace(/^# [^\n]+\n/,''):architecture.replace(/^# [^\n]+\n/,'');
  if(rh){
    bodies['can-device']='<!-- RH850_DEVICE_BEGIN -->\n'+marked(can,'RH850_CAN_INTRO')+'\n\n'+marked(can,'RH850_BOOT_FLOW')+'\n<!-- RH850_DEVICE_END -->';
    bodies['can-fd-device']=marked(can,'RH850_CAN_FD');
    bodies['app-development']=bodies['app-development'].replace(/<!-- RH850_TIMING_BEGIN -->[^]*?<!-- RH850_TIMING_END -->/,'');
  }else{
    bodies['can-device']=bodies['can-device'].replace(/<!-- FD_DEVICE_BEGIN -->[^]*?<!-- FD_DEVICE_END -->/,'');
    bodies['can-fd-device']=fullMarked(await read(join(tool,'docs/CAN_FD_UPDATE_zh-TW.md')),'FD_DEVICE');
    bodies['app-development']=bodies['app-development'].replace(/<a id="device-update-timing"><\/a>[^]*?(?=<!-- DEVICE_WORKSHOP_END -->)/,
      '[App update measurements](experiments.html#device-update-timing).\n');
  }
  const measures=await read(join(tool,'docs/SERIAL_UPDATE_MEASUREMENTS_zh-TW.md'));
  const index=['<a id="experiment-scope"></a>','','## 實驗資料與確認範圍','',
    '各表保留所選映像、傳輸條件、結果及限制；工程存在與實板確認是不同資訊。未列紀錄的組合不推定為未實作或已驗證。','',
    '| 介面 | MCU／資料 | 直接入口 |','| --- | --- | --- |'];
  if(rh){
    index.push('| CAN | RH850；按報告條件分列 | [映像大小與報告](#rh850-can-update-measurements) |',
      '| CAN FD | RH850；FD+BRS | [映像大小與報告](#rh850-can-fd-update-measurements) |',
      '| I2C／UART／Xmodem | 工程說明可查；本指南尚未列實板更新紀錄 | [各介面工程](index.html#boot-interfaces) |');
  }else{
    index.push('| CAN／CAN FD | F13／F24，按模式與 MCU 分列 | [映像大小與報告](#device-update-timing) |');
    for(const [bus,models] of [['i2c',['f13','f24','g16']],['uart',['f13','f24']],['xmodem',['f13','f24']]])
      index.push(`| ${bus==='i2c'?'I2C':bus==='uart'?'UART':'Xmodem'} | ${models.map(model=>`[${model.toUpperCase()}](#${model}-${bus}-update-measurements)`).join('／')} | [工程與機制](${bus}-device.html) |`);
    index.push('| 其他工程組合 | 本指南未列實板更新紀錄 | [依介面選擇工程](index.html#boot-interfaces) |');
  }
  // Use the caller's fresh timing projection, not its still-pending disk version.
  const rhTiming=bodies.experiments;
  bodies.experiments=index.join('\n')+'\n\n'+(rh?rhTiming:timingBody(workshop)+'\n\n'+measures.replace(/^# [^\n]+\n[^]*?(?=<a id=)/,'').replace(/^#### /gm,'### '));
  return bodies;
}
export function bootOverview(platform,config,rhProjection){
  const peer=platform==='RL78'?'RH850':'RL78';
  const rows=['<!-- GUIDE_OVERVIEW -->',`[知識庫首頁](https://released.github.io/) · [${peer} Bootloader](../slide_boot_loader_for_${peer}/)`,'','<a id="article_top"></a>','',
    '# '+config.pages.index.title,'','本指南說明 Device Boot／App 的架構、通訊機制、映像與移植。使用 Tool 更新 App 見 [ISP Tool 操作](../slide_simple_renesas_isp_tool/)。','',
    '<a id="boot-interfaces"></a>','','## 依介面進入主題','','各介面頁頂端提供 MCU 工程與實驗索引；可直接前往指定 MCU 的配置及對應紀錄。','',
    '<!-- '+(platform==='RL78'?'DEVICE_MODE_NAV':'RH850_MODE_NAV')+'_BEGIN -->',
    '| 介面 | Bootloader、MCU 與實驗入口 | Tool 操作 |','| --- | --- | --- |'];
  for(const [label,operation,page] of interfaces){
    const bus=page.replace('-device','');
    const extra=['i2c','uart','xmodem'].includes(bus)?`／[流程](${page}.html#${bus}-boot-flow)／[格式](${page}.html#${bus}-protocol)`:'';
    rows.push(`| ${label} | [機制與 MCU 索引](${page}.html)${extra} | [操作步驟](../slide_simple_renesas_isp_tool/${operation}.html) |`);
  }
  rows.push('<!-- '+(platform==='RL78'?'DEVICE_MODE_NAV':'RH850_MODE_NAV')+'_END -->','','## 依開發目的查閱','','| 目的 | 直接入口 |','| --- | --- |',
    '| 理解整體規劃與 Boot／App 交接 | [整體架構與流程圖](architecture.html) |',
    '| 編輯映像資訊或增加 IRQ | [Metadata 與 IRQ](app-development.html) |',
    '| 解讀 Metadata bytes／封包欄位 | [逐 byte 格式與範例](reference.html)／[串列共用契約](serial-device.html) |',
    '| 移植到其他板子、修改 Flash backend | [Boot／App 移植與 Flash](porting.html) |',
    '| 查看實驗條件、耗時與報告 | [各介面實驗資料](experiments.html) |',
    '| 使用 Tool、安裝映像或確認結果 | [Tool 操作入口](tool-guides.html) |');
  if(platform==='RL78')rows.push('','[Basic UART／I2C 獨立參考工程](basic_uart_i2c/)。');
  if(platform==='RL78')rows.push('',rhProjection);
  return rows.join('\n')+'\n';
}
export async function toolTopicPages(tool,web,output,layout){
  const root=join(web,'slide_simple_renesas_isp_tool');
  const template=await read(join(root,'index.html'));
  const source=await read(join(tool,'docs/TOOL_INTERFACE_OPERATIONS_zh-TW.md'));
  const allFd=await read(join(tool,'docs/CAN_FD_UPDATE_zh-TW.md'));
  const catalog=JSON.parse(await read(join(tool,'scripts/web_guide_sections.json')));
  const rh=catalog.RH850_DEVICE;
  for(const bus of ['i2c','uart','xmodem','can-fd']){
    const body=bus==='can-fd'?fullMarked(allFd,'FD_UPDATE')+'\n\n[CAN FD 通訊測試](custom-transfer.html#can-fd-custom-transfer)／[RL78 工程與實驗索引](../slide_boot_loader_for_RL78/can-fd-device.html)／[RH850 工程與實驗索引](../slide_boot_loader_for_RH850/can-fd-device.html)。':marked(source,'TOOL_'+bus.toUpperCase()).replace(/^## [^\n]+\n/m,'');
    const config=layout.pages[bus];
    await output(join(root,bus+'.md'),`[知識庫首頁](https://released.github.io/) · [ISP Tool](index.html)\n\n<a id="article_top"></a>\n\n# ${config.title}\n\n${config.description}\n\n${body}\n\n---\n\n[Tool 首頁](index.html) · [共用接線與更新流程](serial-isp.html) · [結果與報告](status-and-reports.html)\n`);
    await output(join(root,bus+'.html'),template.replace(/<title>[^]*?<\/title>/,`<title>${config.title}｜Simple Renesas ISP Tool</title>`)
      .replace(/<meta name="description" content="[^"]*">/,`<meta name="description" content="${config.description}">`)
      .replace('<body for="html-export"',`<body for="html-export" data-markdown="${bus}.md"`)
      .replaceAll('href="index.md"',`href="${bus}.md"`).replaceAll('>index.md</a>',`>${bus}.md</a>`));
  }
  // Existing CAN screenshots and workflow remain intact; FD has its own owner.
  const canPath=join(root,'can-update.md');
  await output(canPath,(await read(canPath)).replace(/<!-- FD_UPDATE_BEGIN -->[^]*?<!-- FD_UPDATE_END -->/,'[CAN FD 操作](can-fd.html)。').replace(/^# [^\n]+$/m,'# '+layout.pages['can-update'].title));
  const serialPath=join(root,'serial-isp.md');
  await output(serialPath,(await read(serialPath)).replace(/^# [^\n]+$/m,'# '+layout.pages['serial-isp'].title));
  const home=['<!-- GUIDE_OVERVIEW -->','[知識庫首頁](https://released.github.io/) · [RL78 Bootloader](../slide_boot_loader_for_RL78/) · [RH850 Bootloader](../slide_boot_loader_for_RH850/)','','<a id="article_top"></a>','','# Simple Renesas ISP Tool 操作指南','',
    '透過 MCU bridge 使用 CAN、CAN FD、I2C、UART 或 Xmodem 更新 Device App，並執行各介面的通訊測試。','',
    '[第一次使用：下載](downloads.html) → [連接並識別 Host](operation.html#tool-operation) → 選擇下列介面。','',
    '<a id="tool-interfaces"></a>','','## 依介面選擇操作','','<!-- ISP_MODE_NAV_BEGIN -->','| 介面 | Tool 操作 | MCU 工程與實驗資料 |','| --- | --- | --- |'];
  for(const [label,page,device] of interfaces)home.push(`| ${label} | [設定、更新與通訊測試](${page}.html) | [RL78](../slide_boot_loader_for_RL78/${device}.html)／[RH850](../slide_boot_loader_for_RH850/${device}.html) |`);
  home.push('<!-- ISP_MODE_NAV_END -->','','## 共用功能','','| 主題 | 直接入口 |','| --- | --- |',
    '| BIN／live 欄位、完成與錯誤判讀 | [欄位與報告](status-and-reports.html) |',
    '| I2C／UART／Xmodem 接線與共用更新流程 | [串列共用操作](serial-isp.html) |',
    '| CAN 通訊測試 | [Custom Transfer](custom-transfer.html) |',
    '| Tool、Host、Device 的資料流 | [CAN 協定逐層導讀](can-protocol.html) |',
    '| 更新 bridge 自己的 Host App | [Misc／Host 自身更新](host-firmware.html) |',
    '| Bootloader 架構、Metadata、IRQ 與移植 | [RL78](../slide_boot_loader_for_RL78/)／[RH850](../slide_boot_loader_for_RH850/) |',
    '','工程存在、支援的更新介面與實板驗證範圍分開判讀；各型號的配置與紀錄由 MCU 指南集中說明。','',
    '<!-- RH850_DEVICE_BEGIN -->','<a id="rh850-can-device"></a>','','## '+rh.title,'',`[${rh.title}](https://released.github.io/${rh.owner}/${rh.entry})`,'<!-- RH850_DEVICE_END -->');
  await output(join(root,'index.md'),home.join('\n')+'\n');
}
