// Shared reader structure. Technical topic bodies remain platform-owned sources.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';
const read=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,''));
export async function bootloaderNavigation(tool,web,output,rhBodies,rlBodies={}){
  const contract=JSON.parse(await read(join(tool,'scripts/bootloader_guide_contract.json')));
  const layouts=JSON.parse(await read(join(tool,'scripts/web_guide_layout.json')));
  const catalog=JSON.parse(await read(join(tool,'scripts/web_guide_sections.json')));
  const pages=Object.keys(contract);
  const sites=['slide_boot_loader_for_RL78','slide_boot_loader_for_RH850'];
  const intro=await read(join(tool,'docs/RL78_CAN_CHAPTER_INTRO_zh-TW.md'));
  const topics=(page,index,peer)=>{
    const rows=['<!-- BOOTLOADER_TOPICS_BEGIN -->','## 本頁主題','','| 章節／規格 | 本頁 | 另一平台 |','| --- | --- | --- |'];
    for(const [label,,rl,rh] of contract[page].topics){
      const ids=[rl,rh];rows.push(`| ${label} | [閱讀](#${ids[index]}) | [${peer.includes('RH850')?'RH850':'RL78'}](../${peer}/${page}.html#${ids[1-index]}) |`);
    }
    rows.push('<!-- BOOTLOADER_TOPICS_END -->');return rows.join('\n');
  };
  function toolLinks(){
    const keys=['PUBLIC_RESOURCES','F24_APP_IRQ','COMMON_IMAGE_POLICY','UPDATE_EVIDENCE','FD_UPDATE','FD_TRANSFER','UART_TOOL_GUIDE'];
    let text='<a id="tool-links"></a>\n\n## Tool 與其他操作入口\n\n<a id="共用-tool-操作"></a>\n\n| 操作目的 | 直接入口 |\n| --- | --- |\n| 連接Host與核對BIN | [COM／Host](../slide_simple_renesas_isp_tool/operation.html#tool-operation)／[BIN欄位](../slide_simple_renesas_isp_tool/status-and-reports.html#tool-ui-fields) |\n| 更新Device App | [CAN／CAN FD](../slide_simple_renesas_isp_tool/can-update.html)／[UART／Xmodem／I2C](../slide_simple_renesas_isp_tool/serial-isp.html#serial-device-operation) |\n| 查看更新結果 | [完成提示](../slide_simple_renesas_isp_tool/status-and-reports.html#update-completion)／[HTML報告](../slide_simple_renesas_isp_tool/status-and-reports.html#update-html-report) |\n| 驗證通訊 | [Custom Transfer](../slide_simple_renesas_isp_tool/custom-transfer.html#custom-transfer) |\n| 對照資料流與Flash寫入 | [CAN更新逐層導讀](../slide_simple_renesas_isp_tool/can-protocol.html#can-update-walkthrough) |\n| 更新Host App | [Misc操作](../slide_simple_renesas_isp_tool/host-firmware.html#host-bridge-firmware) |\n';
    for(const key of keys){const c=catalog[key],page=layouts[c.owner].sections[key];text+=`\n<!-- ${key}_BEGIN -->\n<a id="${c.anchor}"></a>\n\n## ${c.title}\n\n${c.description}\n[${c.title}](https://released.github.io/${c.owner}/${page}.html#${c.anchor})\n<!-- ${key}_END -->\n`;}
    return text;
  }
  function overview(platform,config,old){
    const rh=platform==='RH850';
    const timing=rh?'rh850-update-timing':'device-update-timing';
    const bytes=rh?'rh850-metadata-bytes':'flash-metadata-can-reference';
    const irq=rh?'rh850-app-editing':'device-irq-ownership';
    const install=rh?'guide-porting-4':'device-debug-monitor';
    const peer=rh?sites[0]:sites[1];
    let text=`<!-- GUIDE_OVERVIEW -->\n[知識庫首頁](https://released.github.io/) · [${rh?'RL78':'RH850'} Bootloader](../${peer}/)\n\n<a id="article_top"></a>\n\n# ${config.pages.index.title}\n\n依閱讀目的選擇 MCU device 工程、App 設定、移植或格式參考。\n日常更新與通訊驗證見 [Simple Renesas ISP Tool](../slide_simple_renesas_isp_tool/)。\n\n## 快速查閱\n\n| 資料 | 適用內容 | 直接連結 |\n| --- | --- | --- |\n| **App BIN 更新耗時比較** | ${rh?'RH850 CAN FD 各 App BIN 大小的實測時間':'F13／F24 Classical CAN 及 F24 CAN FD 的完整比較表'} | [查看更新耗時表](app-development.html#${timing}) |\n| Metadata 每個byte與address | 欄位、範例值、來源及修改位置 | [查看格式表](reference.html#${bytes}) |\n| App 新增IRQ與執行路徑 | ISR、mapping、產表、交接與成本 | [查看IRQ教學](app-development.html#${irq}) |\n| Boot安裝與debug monitor配置 | Flash／RAM、保護範圍及燒錄檔 | [查看安裝條件](porting.html#${install}) |\n\n## 主題指南\n\n| 主題 | 內容 |\n| --- | --- |\n`;
    text=text.replace('## 快速查閱',`## 依閱讀目的查閱\n\n| 目的 | 直接入口 |\n| --- | --- |\n| 使用Tool更新App | [CAN／CAN FD操作](../slide_simple_renesas_isp_tool/can-update.html)／[UART／Xmodem／I2C操作](../slide_simple_renesas_isp_tool/serial-isp.html) |\n| 修改App或增加IRQ | [Metadata設定](app-development.html#guide-app-development-2)／[IRQ範例](app-development.html#guide-app-development-3)／[本地建置](app-development.html#guide-app-development-4) |\n| 移植到其他板子或MCU | [修改範圍與工程配置](porting.html#guide-porting-2) |\n| 查協定與欄位 | [CAN格式](reference.html#guide-reference-10)／[串列共用契約](serial-device.html#guide-serial-device-5) |\n\n## 快速查閱`);
    for(const page of pages)text+=`| [${contract[page].title}](${page}.html) | ${config.pages[page].description} |\n`;
    text+='\n## CAN 範例分類\n\n';
    if(!rh){const marker=old.match(/<!-- DEVICE_MODE_NAV_BEGIN -->[^]*?<!-- DEVICE_MODE_NAV_END -->/);assert.ok(marker);text+=marker[0]+'\n';}
    else text+='| 分類 | 工程／範例 | 操作入口 |\n| --- | --- | --- |\n| Classical CAN | [Boot／App與CAN1／CAN4](can-device.html#rh850-can-device) | [App更新](../slide_simple_renesas_isp_tool/can-update.html) |\n| CAN FD | [Boot／App、FD+BRS與raw64](can-device.html#guide-can-device-5) | [CAN FD更新](../slide_simple_renesas_isp_tool/can-update.html#can-fd-update) |\n| Custom Transfer | [CAN／CAN FD獨立Transfer工程](can-device.html#rh850-can-device) | [通訊測試](../slide_simple_renesas_isp_tool/custom-transfer.html) |\n';
    text+='\n## 串列 Bootloader 與協定格式\n\n| 介面 | Bootloader | Protocol Format |\n| --- | --- | --- |\n';
    for(const [bus,label] of [['uart','UART'],['i2c','I2C'],['xmodem','Xmodem']]){
      text+=`| ${label} | [初始化與更新流程](${bus}-device.html#${bus}-boot-flow) | [封包、範例與完成條件](${bus}-device.html#${bus}-protocol) |\n`;
    }
    text+='\n共用工程、Metadata、建置與 Frame／INFO／Status 見[串列 ISP 共用契約](serial-device.html)。\n';
    text+='\n## Basic 參考工程\n\n';
    text+=rh?'[工程基準與選擇](porting.html#guide-porting-1)說明簡易Boot導入ISP格式時的映像、IRQ、CRC與Flash核對條件。\n':'[Basic UART／I2C 範例](basic_uart_i2c/)提供獨立範例的分區、startup、向量與CRC操作。\n';
    if(rh)text+='\n## 其他 MCU 指南\n\n[RL78 Bootloader](../slide_boot_loader_for_RL78/)使用相同章節順序，提供RL78各工程的設定與規格。\n';
    else {const marker=old.match(/<!-- RH850_DEVICE_BEGIN -->[^]*?<!-- RH850_DEVICE_END -->/);assert.ok(marker);text+='\n'+marker[0]+'\n';}
    return text;
  }
  for(const [index,site] of sites.entries()){
    const config=layouts[site],platform=index?'RH850':'RL78',peer=sites[1-index];
    const template=await read(join(web,sites[0],'index.html'));
    const oldIndex=await read(join(web,site,'index.md'));
    await output(join(web,site,'index.md'),overview(platform,config,oldIndex));
    for(const [i,page] of pages.entries()){
      assert.equal(config.pages[page].title,contract[page].title,'equal chapter title '+site+'/'+page);
      let body;
      if(page==='tool-guides'){body=toolLinks();}
      else if(index){body=rhBodies[page];}
      else if(rlBodies[page]){body=rlBodies[page];}
      else {
        let old=await read(join(web,site,page+'.md'));
        old=old.replace(/<!-- BOOTLOADER_TOPICS_BEGIN -->[^]*?<!-- BOOTLOADER_TOPICS_END -->\s*/,'');
        const title=/^# [^\n]+\n\s*/m.exec(old);assert.ok(title);
        body=old.slice(title.index+title[0].length);
        const descriptionEnd=body.indexOf('\n\n');assert.ok(descriptionEnd>=0);
        body=body.slice(descriptionEnd+2);
        const footer=body.lastIndexOf('\n---\n');
        if(footer>=0&&body.slice(footer).includes('[指南入口]'))body=body.slice(0,footer);
        if(page==='can-device'){
          const block='<!-- RL78_CAN_INDEX_BEGIN -->\n'+intro.trim()+'\n<!-- RL78_CAN_INDEX_END -->';
          body=body.includes('<!-- RL78_CAN_INDEX_BEGIN -->')?body.replace(/<!-- RL78_CAN_INDEX_BEGIN -->[^]*?<!-- RL78_CAN_INDEX_END -->/,block):block+'\n\n'+body;
        }
        if(page==='tool-guides'&&!body.includes('<a id="tool-links">'))body='<a id="tool-links"></a>\n\n'+body;
      }
      const previous=pages[i-1],next=pages[i+1];
      const footer=[previous?`[上一主題：${contract[previous].title}](${previous}.html)`:null,'[指南入口](index.html)',next?`[下一主題：${contract[next].title}](${next}.html)`:null].filter(Boolean).join(' · ');
      const md=`[知識庫首頁](https://released.github.io/) · [${platform} Bootloader](index.html)\n\n<a id="article_top"></a>\n\n# ${contract[page].title}\n\n${config.pages[page].description}\n\n${topics(page,index,peer)}\n\n${body.trim()}\n\n---\n\n${footer}\n`;
      await output(join(web,site,page+'.md'),md);
    }
    for(const page of ['index',...pages]){
      let html=template.replaceAll('RL78 Bootloader',platform+' Bootloader').replace('>RL78</span>','>'+platform+'</span>');
      html=html.replace(/<title>[^]*?<\/title>/,`<title>${config.pages[page].title}｜${platform} Bootloader</title>`)
        .replace(/<meta name="description" content="[^"]*">/,`<meta name="description" content="${config.pages[page].description}">`);
      if(page!=='index')html=html.replace('<body for="html-export">',`<body for="html-export" data-markdown="${page}.md">`)
        .replaceAll('href="index.md"',`href="${page}.md"`).replaceAll('>index.md</a>',`>${page}.md</a>`);
      await output(join(web,site,page+'.html'),html);
    }
  }
}
