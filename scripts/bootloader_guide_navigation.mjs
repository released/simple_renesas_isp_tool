// Shared reader structure. Technical topic bodies remain platform-owned sources.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';
import {deviceUpgradeContent} from './device_upgrade_content.mjs';
import {topicBodies,bootOverview,toolTopicPages} from './guide_topic_content.mjs';
// A fresh Windows checkout may use CRLF; chapter separators are parsed as LF.
const read=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,'').replace(/\r\n?/g,'\n'));
export async function bootloaderNavigation(tool,web,output,rhBodies,rlBodies={}){
  const upgrades = await deviceUpgradeContent(tool);
  await upgrades.measuredAssets(web, output);
  const contract=JSON.parse(await read(join(tool,'scripts/bootloader_guide_contract.json')));
  const layouts=JSON.parse(await read(join(tool,'scripts/web_guide_layout.json')));
  const catalog=JSON.parse(await read(join(tool,'scripts/web_guide_sections.json')));
  const pages=Object.keys(contract);
  const sites=['slide_boot_loader_for_RL78','slide_boot_loader_for_RH850'];
  const intro=await read(join(tool,'docs/RL78_CAN_CHAPTER_INTRO_zh-TW.md'));
  const topics=(page,index,peer)=>{
    const rows=['<!-- BOOTLOADER_TOPICS_BEGIN -->','<details>','<summary>章節與另一平台對照</summary>','','| 章節／規格 | 本頁 | 另一平台 |','| --- | --- | --- |'];
    for(const [label,,rl,rh] of contract[page].topics){
      const ids=[rl,rh];rows.push(`| ${label} | [閱讀](#${ids[index]}) | [${peer.includes('RH850')?'RH850':'RL78'}](../${peer}/${page}.html#${ids[1-index]}) |`);
    }
    rows.push('','</details>','<!-- BOOTLOADER_TOPICS_END -->');return rows.join('\n');
  };
  function toolLinks(){
    const keys=['PUBLIC_RESOURCES','F24_APP_IRQ','COMMON_IMAGE_POLICY','UPDATE_EVIDENCE','FD_UPDATE','FD_TRANSFER','UART_TOOL_GUIDE'];
    let text='<a id="tool-links"></a>\n\n## Tool 與其他操作入口\n\n<a id="共用-tool-操作"></a>\n\n| 操作目的 | 直接入口 |\n| --- | --- |\n| 連接Host與核對BIN | [COM／Host](../slide_simple_renesas_isp_tool/operation.html#tool-operation)／[BIN欄位](../slide_simple_renesas_isp_tool/status-and-reports.html#tool-ui-fields) |\n| 更新Device App | [CAN／CAN FD](../slide_simple_renesas_isp_tool/can-update.html)／[UART／Xmodem／I2C](../slide_simple_renesas_isp_tool/serial-isp.html#serial-device-operation) |\n| 查看更新結果 | [完成提示](../slide_simple_renesas_isp_tool/status-and-reports.html#update-completion)／[HTML報告](../slide_simple_renesas_isp_tool/status-and-reports.html#update-html-report) |\n| 驗證通訊 | [Custom Transfer](../slide_simple_renesas_isp_tool/custom-transfer.html#custom-transfer) |\n| 對照資料流與Flash寫入 | [CAN更新逐層導讀](../slide_simple_renesas_isp_tool/can-protocol.html#can-update-walkthrough) |\n| 更新Host App | [Misc操作](../slide_simple_renesas_isp_tool/host-firmware.html#host-bridge-firmware) |\n';
    for(const key of keys){const c=catalog[key],page=layouts[c.owner].sections[key];text+=`\n<!-- ${key}_BEGIN -->\n<a id="${c.anchor}"></a>\n\n## ${c.title}\n\n[${c.title}](https://released.github.io/${c.owner}/${page}.html#${c.anchor})\n<!-- ${key}_END -->\n`;}
    return text;
  }
  for(const [index,site] of sites.entries()){
    const config=layouts[site],platform=index?'RH850':'RL78',peer=sites[1-index];
    const template=await read(join(web,sites[0],'index.html'));
    const rh=catalog.RH850_DEVICE;
    const rhProjection=`<!-- RH850_DEVICE_BEGIN -->\n<a id="${rh.anchor}"></a>\n\n## ${rh.title}\n\n[${rh.title}](https://released.github.io/${rh.owner}/${rh.entry})\n<!-- RH850_DEVICE_END -->`;
    await output(join(web,site,'index.md'),bootOverview(platform,config,rhProjection));
    const selected={...(index?rhBodies:rlBodies)};
    for(const page of ['can-device','app-development']){
      if(selected[page])continue;
      const old=await read(join(web,site,page+'.md'));
      const start=/^# [^\n]+\n\s*[^\n]+\n\s*/m.exec(old);
      assert.ok(start,'existing canonical chapter '+page);
      selected[page]=old.slice(start.index+start[0].length)
        .replace(/<!-- BOOTLOADER_TOPICS_BEGIN -->[^]*?<!-- BOOTLOADER_TOPICS_END -->\s*/,'')
        .replace(/\n---\n\n\[上一主題[^]*$/,'');
    }
    if(!index){
      const block='<!-- RL78_CAN_INDEX_BEGIN -->\n'+intro.trim()+'\n<!-- RL78_CAN_INDEX_END -->';
      selected['can-device']=selected['can-device'].includes('<!-- RL78_CAN_INDEX_BEGIN -->')
        ?selected['can-device'].replace(/<!-- RL78_CAN_INDEX_BEGIN -->[^]*?<!-- RL78_CAN_INDEX_END -->/,block)
        :block+'\n\n'+selected['can-device'];
    }
    const expanded=await topicBodies(tool,platform,selected);
    for(const [i,page] of pages.entries()){
      assert.equal(config.pages[page].title,contract[page].title,'equal chapter title '+site+'/'+page);
      let body;
      if(page==='tool-guides'){body=toolLinks();}
      else if(expanded[page]){body=expanded[page];}
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
      body = body.replace(/<!-- DEVICE_UPGRADE_PATHS_BEGIN -->[^]*?<!-- DEVICE_UPGRADE_PATHS_END -->\s*/g, '');
      body = body.replace(/<!-- MODEL_DETAILS_BEGIN -->[^]*?<!-- MODEL_DETAILS_END -->\s*/g, '');
      const upgradeBody = upgrades.chapter(platform, page,'index');
      if (upgradeBody) body = upgradeBody.trim() + '\n\n' + body.trim() + '\n\n<!-- MODEL_DETAILS_BEGIN -->\n' +
        upgrades.profiles.filter(profile=>profile.platform===platform&&profile.page===page).map(profile=>profile.body).join('\n\n')+'\n<!-- MODEL_DETAILS_END -->';
      const previous=pages[i-1],next=pages[i+1];
      const footer=[previous?`[上一主題：${contract[previous].title}](${previous}.html)`:null,'[指南入口](index.html)',next?`[下一主題：${contract[next].title}](${next}.html)`:null].filter(Boolean).join(' · ');
      const md=`[知識庫首頁](https://released.github.io/) · [${platform} Bootloader](index.html)\n\n<a id="article_top"></a>\n\n# ${contract[page].title}\n\n${config.pages[page].description}\n\n${topics(page,index,peer)}\n\n${body.trim()}\n\n---\n\n${footer}\n`;
      await output(join(web,site,page+'.md'),md);
    }
    for(const page of ['index',...pages]){
      let html=template.replaceAll('RL78 Bootloader',platform+' Bootloader').replace('>RL78</span>','>'+platform+'</span>');
      html=html.replace(/<title>[^]*?<\/title>/,`<title>${config.pages[page].title}｜${platform} Bootloader</title>`)
        .replace(/<meta name="description" content="[^"]*">/,`<meta name="description" content="${config.pages[page].description}">`);
      if(page!=='index')html=html.replace('<body for="html-export"',`<body for="html-export" data-markdown="${page}.md"`)
        .replaceAll('href="index.md"',`href="${page}.md"`).replaceAll('>index.md</a>',`>${page}.md</a>`);
      await output(join(web,site,page+'.html'),html);
    }
  }
  await toolTopicPages(tool,web,output,layouts.slide_simple_renesas_isp_tool);
}
