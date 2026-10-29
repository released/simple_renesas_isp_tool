// One-time mechanical migration; refresh only regenerates legacy routes from
// the committed map. Canonical chapters continue to use their PowerShell syncs.
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
const tool=resolve(fileURLToPath(new URL('../',import.meta.url)));
const web=resolve(process.argv[2]||'');
const mode=process.argv[3];
assert.ok(process.argv[2]&&['--migrate','--refresh'].includes(mode),'supply web root and --migrate/--refresh');
const layout=JSON.parse(await readFile(join(tool,'scripts/web_guide_layout.json'),'utf8'));
const bootTopics=JSON.parse(await readFile(join(tool,'scripts/bootloader_guide_contract.json'),'utf8'));
const escapeHtml=s=>s.replaceAll('&','&amp;').replaceAll('"','&quot;').replaceAll('<','&lt;');
const slug=s=>s.trim().toLowerCase().replace(/[`~!@#$%^&*()+=\[\]{}\\|;:'",.<>/?，。；：「」『』（）]/g,'').replace(/\s+/g,'-');
function outline(source){
  let fence='';const result=[];let offset=0;
  for(const line of source.split('\n')){
    const f=/^\s*(`{3,}|~{3,})/.exec(line);
    if(f){if(!fence)fence=f[1][0];else if(fence===f[1][0])fence='';}
    else if(!fence&&/^#{1,3}\s+/.test(line))result.push({offset,line});
    offset+=line.length+1;
  }
  return result;
}
function aliasesScript(aliases){
  const routes='Object.assign(Object.create(null),'+JSON.stringify(aliases).replaceAll('<','\\u003c')+')';
  return `// Generated from guide-map.json; routes stay within this guide.\n(function(){\n  "use strict";\n  var routes=${routes};\n  function destination(item){\n    var target=new URL(item.page,location.href);\n    if(target.origin!==location.origin)return null;\n    target.search=location.search;target.hash=item.anchor;return target;\n  }\n  function route(){\n    var hash;try{hash=decodeURIComponent(location.hash.slice(1));}catch(e){return;}\n    if(document.getElementById(hash))return;\n    var item=routes[hash];if(!item)return;\n    var target=destination(item);if(!target)return;\n    if(target.pathname!==location.pathname||target.hash!==location.hash)location.replace(target.href);\n  }\n  addEventListener("guide:ready",function(){\n    document.querySelectorAll('#content a[href^="#"]').forEach(function(link){\n      var id;try{id=decodeURIComponent(link.hash.slice(1));}catch(e){return;}\n      if(document.getElementById(id)||!routes[id])return;\n      var target=destination(routes[id]);if(target)link.href=target.href;\n    });route();\n  });\n  addEventListener("hashchange",route);\n  if(!document.body.hasAttribute("data-markdown"))route();\n}());\n`;
}
let baseline;
if(mode==='--migrate'){
  assert.ok(process.argv[4],'supply the pre-migration browser results.json');
  baseline=JSON.parse(await readFile(process.argv[4],'utf8'));
}
for(const [site,config] of Object.entries(layout)){
  if(process.argv[5]&&site!==process.argv[5])continue;
  const root=join(web,site);
  if(mode==='--refresh'){
    const map=JSON.parse(await readFile(join(root,'guide-map.json'),'utf8'));
    map.pages=config.pages;
    const seen=new Set(),owners=new Map();
    for(const page of Object.keys(config.pages)){
      const source=await readFile(join(root,page+'.md'),'utf8');
      for(const match of source.matchAll(/<a id="([^"]+)"><\/a>/g)){
        const id=match[1];if(id==='article_top')continue;
        assert.ok(!seen.has(id),'duplicate stable anchor across chapters '+site+'/'+id);seen.add(id);
        map.aliases[id]={page:page+'.html',anchor:id};
        owners.set(id,page+'.html');
      }
    }
    // Existing heading aliases can point at a stable anchor moved to another chapter.
    for(const target of Object.values(map.aliases)){
      if(owners.has(target.anchor))target.page=owners.get(target.anchor);
    }
    if(/^slide_boot_loader_for_(?:RL78|RH850)$/.test(site)){
      for(const [old,bus,anchor] of [['uart診斷格式','uart','serial-uart-diagnostics'],['i2c-mailbox','i2c','i2c-protocol'],['xmodem-crc128資料與診斷','xmodem','xmodem-protocol'],['xmodem-串流完成','xmodem','xmodem-boot-flow']]){
        map.aliases[old]={page:bus+'-device.html',anchor};
      }
    }
    if(/^slide_boot_loader_for_(?:RL78|RH850)$/.test(site)){
      const platform=site.endsWith('RH850')?3:2;
      for(const [page,entry] of Object.entries(bootTopics)){
        for(const [,heading,...ids] of entry.topics){
          map.aliases[slug(heading)]={page:page+'.html',anchor:ids[platform-2]};
        }
      }
    }
    await writeFile(join(root,'guide-map.json'),JSON.stringify(map,null,2)+'\n');
    await writeFile(join(root,'guide-routes.js'),aliasesScript(map.aliases));
    continue;
  }
  let original=await readFile(join(root,'index.md'),'utf8');
  original=original.replaceAll('\r\n','\n');
  assert.ok(!original.includes('<!-- GUIDE_OVERVIEW -->'),'already split; migration will not overwrite authored pages');
  const inventory=baseline.results.find(r=>r.path==='/'+site+'/');
  assert.ok(inventory,'missing rendered baseline '+site);
  const headings=outline(original);
  assert.equal(headings.length,inventory.headings.length,'baseline heading/source count '+site);
  headings.forEach((h,i)=>Object.assign(h,inventory.headings[i]));
  const snapshot=join(tool,'tmp/internal/verification/web_guide_split',site);
  await mkdir(snapshot,{recursive:true});
  await writeFile(join(snapshot,'original.md'),original);
  const template=await readFile(join(root,'index.html'),'utf8');
  await writeFile(join(snapshot,'original.html'),template);
  const chunks=[];
  function range(page,start,end){
    const first=original.indexOf(start);assert.ok(first>=0,'missing '+start);
    const last=end?original.indexOf(end,first+start.length):original.length;
    assert.ok(last>first,'invalid end '+end);
    assert.ok(!chunks.some(c=>first<c.end&&last>c.start),'overlapping chapter range '+start);
    chunks.push({page,start:first,end:last,text:original.slice(first,last).trim()});
  }
  if(site.includes('isp_tool')){
    range('operation','<a id="tool-operation"></a>','<a id="custom-transfer"></a>');
    range('custom-transfer','<a id="custom-transfer"></a>','<a id="f13-device-update"></a>');
    range('can-update','<a id="f13-device-update"></a>','<!-- CAN_WALKTHROUGH_BEGIN -->');
    range('serial-isp','## 4. I2C Master','<a id="host-bridge-firmware"></a>');
    range('host-firmware','<a id="host-bridge-firmware"></a>','## 6. 連線與錯誤資訊');
    range('operation','## 6. 連線與錯誤資訊','<!-- COMMON_IMAGE_POLICY_BEGIN -->');
    range('device-guides','## Device firmware 與工程說明','<!-- F13_BOOT_DECISION_BEGIN -->');
  }else{
    range('can-device','## F13 (MCU CAN device)：Boot 啟動與更新','<!-- F24_DEVICE_BEGIN -->');
    range('basic_uart_i2c','<a id="uart-i2c-examples"></a>','## Tool 與其他操作入口');
    range('tool-guides','## Tool 與其他操作入口','<!-- PUBLIC_RESOURCES_BEGIN -->');
  }
  for(const [section,page] of Object.entries(config.sections)){
    if(page==='index')continue;
    const start=original.indexOf('<!-- '+section+'_BEGIN -->');
    assert.ok(start>=0,'missing section '+section);
    if(chunks.some(c=>start>=c.start&&start<c.end))continue;
    const end=original.indexOf('<!-- '+section+'_END -->',start)+('<!-- '+section+'_END -->').length;
    assert.ok(end>start,'missing section end '+section);
    assert.ok(!chunks.some(c=>start<c.end&&end>c.start),'overlapping marker '+section);
    chunks.push({page,start,end,text:original.slice(start,end)});
  }
  chunks.sort((a,b)=>a.start-b.start);
  const map={version:1,pages:config.pages,aliases:{}};
  const assigned=new Map();
  for(const c of chunks){
    if(c.page==='basic_uart_i2c'){
      for(const m of c.text.matchAll(/<a id="([^"]+)"><\/a>/g))map.aliases[m[1]]={page:'basic_uart_i2c/',anchor:m[1]};
      map.aliases['uart-i2c-examples']={page:'basic_uart_i2c/',anchor:'article_top'};
      for(const h of headings.filter(h=>h.offset>=c.start&&h.offset<c.end))map.aliases[h.id]={page:'basic_uart_i2c/',anchor:'article_top'};
      continue;
    }
    if(!assigned.has(c.page))assigned.set(c.page,[]);
    assigned.get(c.page).push(c);
    for(const m of c.text.matchAll(/<a id="([^"]+)"><\/a>/g))map.aliases[m[1]]={page:c.page+'.html',anchor:m[1]};
  }
  const pages=Object.keys(config.pages).filter(p=>p!=='index');
  for(const [index,page] of pages.entries()){
    const info=config.pages[page];const list=assigned.get(page)||[];
    assert.ok(list.length,'empty chapter '+page);
    const body=list.map(c=>c.text).join('\n\n');
    // Predict the unchanged renderer's IDs from actual pre-migration text and
    // source positions,including manual anchors and duplicate-heading suffixes.
    const currentIds=new Set([...body.matchAll(/<a id="([^"]+)"><\/a>/g)].map(m=>m[1]));
    const used=new Set(['article_top']);
    for(const c of list){
      for(const h of headings.filter(h=>h.offset>=c.start&&h.offset<c.end)){
        const before=original.slice(c.start,h.offset).replace(/<!--[^]*?-->/g,'').trimEnd();
        const manual=/<a id="([^"]+)"><\/a>$/.exec(before);
        const preferred=manual?manual[1]:slug(h.text);
        if(manual)currentIds.delete(manual[1]);
        let id=preferred,suffix=2;while(used.has(id)||currentIds.has(id))id=preferred+'-'+suffix++;
        used.add(id);map.aliases[h.id]={page:page+'.html',anchor:id};
      }
    }
    const previous=pages[index-1],next=pages[index+1];
    const nav=[previous?`[上一主題：${config.pages[previous].title}](${previous}.html)`:null,
      '[指南入口](index.html)',next?`[下一主題：${config.pages[next].title}](${next}.html)`:null].filter(Boolean).join(' · ');
    const markdown=`[知識庫首頁](https://released.github.io/) · [${config.pages.index.title}](index.html)\n\n<a id="article_top"></a>\n\n# ${info.title}\n\n${info.description}\n\n${body}\n\n---\n\n${nav}\n`;
    await writeFile(join(root,page+'.md'),markdown);
    let html=template.replace(/<title>[^]*?<\/title>/,`<title>${escapeHtml(info.title)}｜${escapeHtml(config.pages.index.title)}</title>`)
      .replace(/<meta name="description" content="[^"]*">/,`<meta name="description" content="${escapeHtml(info.description)}">`)
      .replace('<body for="html-export">',`<body for="html-export" data-markdown="${page}.md">`)
      .replace(/  <script src="\/notices_can\/assets\/app.js"><\/script>/,
        '  <script src="guide-routes.js"></script>\n  <script src="/notices_can/assets/app.js"></script>')
      .replace(/href="index.md"/g,`href="${page}.md"`)
      .replace(/>index.md<\/a>/g,`>${page}.md</a>`);
    await writeFile(join(root,page+'.html'),html);
  }
  // Every original explicit or renderer-generated content anchor must survive.
  if(!site.includes('isp_tool'))map.aliases['mcu-can-device']={page:'can-device.html',anchor:'article_top'};
  for(const id of inventory.contentIds)assert.ok(id==='article_top'||map.aliases[id],'unassigned legacy anchor '+id);
  const modeMarker=site.includes('isp_tool')?'ISP_MODE_NAV':'DEVICE_MODE_NAV';
  const modeBlock=original.match(new RegExp('<!-- '+modeMarker+'_BEGIN -->[^]*?<!-- '+modeMarker+'_END -->'))[0];
  const intro=site.includes('isp_tool')?
    'Simple Renesas ISP Tool 是個人開發與分享的 MCU 韌體更新及通訊測試工具。\n透過 MCU bridge 連接 device，依需求選擇下載、操作、更新或通訊驗證。\nDevice Boot／App 開發見 [Bootloader 指南](../slide_boot_loader_for_RL78/)。':
    '依閱讀目的選擇 MCU device 工程、App 設定、移植或格式參考。\n日常更新與通訊驗證見 [ISP Tool 指南](../slide_simple_renesas_isp_tool/)。';
  let overview=`<!-- GUIDE_OVERVIEW -->\n[知識庫首頁](https://released.github.io/)\n\n<a id="article_top"></a>\n\n# ${config.pages.index.title}\n\n${intro}\n\n## 主題指南\n\n| 主題 | 內容 |\n| --- | --- |\n`;
  for(const page of pages)overview+=`| [${config.pages[page].title}](${page}.html) | ${config.pages[page].description} |\n`;
  overview+='\n## CAN 範例分類\n\n'+modeBlock+'\n';
  if(!site.includes('isp_tool'))overview+='\n## Basic 參考工程\n\n[Basic UART／I2C 範例](basic_uart_i2c/)提供獨立範例的分區、startup、向量與 CRC 操作。\n';
  overview+='\n<!-- RH850_DEVICE_BEGIN -->\n<!-- RH850_DEVICE_END -->\n';
  overview=overview.replace(/\]\(#([^\s)]+)\)/g,(all,id)=>{
    const target=map.aliases[id];return target?`](${target.page}#${target.anchor})`:all;
  });
  await writeFile(join(root,'index.md'),overview);
  await writeFile(join(root,'index.html'),template.replace(/  <script src="\/notices_can\/assets\/app.js"><\/script>/,
    '  <script src="guide-routes.js"></script>\n  <script src="/notices_can/assets/app.js"></script>'));
  await writeFile(join(root,'guide-map.json'),JSON.stringify(map,null,2)+'\n');
  await writeFile(join(root,'guide-routes.js'),aliasesScript(map.aliases));
  // A migration ledger keeps exact source spans;it is private,not a second
  // public monolithic guide or an authoring input for later synchronization.
  await writeFile(join(snapshot,'migration.json'),JSON.stringify({chunks:chunks.map(c=>({page:c.page,start:c.start,end:c.end})),headings},null,2));
  console.log('SPLIT',site,pages.length,'chapters',Object.keys(map.aliases).length,'legacy anchors');
}
