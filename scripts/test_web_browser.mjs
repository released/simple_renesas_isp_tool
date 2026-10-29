// Real installed Chromium, private profile and loopback-only preview. No dependencies.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createServer} from 'node:http';
import {existsSync} from 'node:fs';
import {mkdir,readFile,writeFile,stat} from 'node:fs/promises';
import {resolve,join,sep,extname} from 'node:path';
import {fileURLToPath} from 'node:url';
const tool=resolve(fileURLToPath(new URL('../',import.meta.url)));
const web=resolve(process.argv[2]||'');
assert.ok(process.argv[2],'supply explicit local web root');
const out=join(tool,'tmp/internal/tests/web_browser_'+Date.now());
await mkdir(out,{recursive:true});
const mime={'.html':'text/html','.md':'text/plain','.js':'text/javascript','.css':'text/css','.svg':'image/svg+xml','.png':'image/png','.jpg':'image/jpeg'};
const server=createServer(async(req,res)=>{try{
  let path=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
  if(path==='/favicon.ico'){res.writeHead(204).end();return;}
  const preview=/^\/__rh850_(serial|can)__\/(.*)$/.exec(path);
  if(preview){
    const name=preview[1]==='serial'?'RH850_ISP_DEVICE_DESIGN_zh-TW.md':'RH850_CAN_DEVICE_GUIDE_zh-TW.md';
    if(!preview[2]||preview[2]==='index.html'){
      const html=await readFile(join(web,'slide_boot_loader_for_RL78/index.html'),'utf8');
      res.writeHead(200,{'Content-Type':'text/html; charset=utf-8'}).end(html
        .replace('  <script src="guide-routes.js"></script>','')
        .replaceAll('RL78 Bootloader','RH850 '+(preview[1]==='serial'?'Serial ISP':'CAN Device')));return;
    }
    const doc=preview[2]==='index.md'?name:preview[2];
    assert.ok(!doc.includes('/')&&!doc.includes('\\')&&/\.(md|pdf)$/.test(doc),'only named document preview');
    res.writeHead(200,{'Content-Type':doc.endsWith('.pdf')?'application/pdf':'text/plain; charset=utf-8'}).end(await readFile(join(tool,'docs',doc)));return;
  }
  if(path==='/'||path==='/index.html')path='/released.github.io/index.html';
  else if(path==='/README.md'||path.startsWith('/assets/'))path='/released.github.io'+path;
  if(path.endsWith('/'))path+='index.html';
  const target=resolve(web,'.'+path);
  assert.ok(target.startsWith(web+sep),'path outside preview root');
  assert.ok((await stat(target)).isFile(),'not a file');
  res.writeHead(200,{'Content-Type':(mime[extname(target)]||'application/octet-stream')+'; charset=utf-8','Cache-Control':'no-store'}).end(await readFile(target));
}catch(e){console.error('Preview 404:',req.url,String(e));res.writeHead(404).end(String(e));}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const base='http://127.0.0.1:'+server.address().port;
const chrome=['C:/Program Files/Google/Chrome/Application/chrome.exe','C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe'].find(existsSync);
assert.ok(chrome,'installed browser required');
const profile=join(out,'profile');await mkdir(profile);
const child=spawn(chrome,['--headless=new','--disable-gpu','--no-first-run','--disable-extensions','--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{windowsHide:true,stdio:'ignore'});
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function wait(test,label,timeout=45000){const end=Date.now()+timeout;while(Date.now()<end){if(await test())return;await pause(100);}throw Error('Timeout: '+label);}
let socket;let next=0;const pending=new Map();const errors=[];const results=[];const navigation=[];
async function call(method,params={}){const id=++next;return new Promise((resolve,reject)=>{pending.set(id,{resolve,reject});socket.send(JSON.stringify({id,method,params}));});}
async function evaluate(expression){const value=await call('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});if(value.exceptionDetails)throw Error(JSON.stringify(value.exceptionDetails));return value.result.value;}
async function capture(name){const {data}=await call('Page.captureScreenshot',{format:'png'});await writeFile(join(out,name+'.png'),Buffer.from(data,'base64'));}
try{
  const portFile=join(profile,'DevToolsActivePort');let port;
  await wait(async()=>{try{const data=await readFile(portFile,'utf8');port=data.split(/\r?\n/)[0];return /^\d+\r?\n\/devtools\/browser\//.test(data);}catch(e){if(['ENOENT','EBUSY'].includes(e.code))return false;throw e;}},'private browser startup');
  const pages=await (await fetch('http://127.0.0.1:'+port+'/json/list')).json();
  socket=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
  await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
  socket.addEventListener('message',({data})=>{
    const message=JSON.parse(data);
    if(message.id&&pending.has(message.id)){const p=pending.get(message.id);pending.delete(message.id);if(message.error)p.reject(Error(message.error.message));else p.resolve(message.result);}
    if(message.method==='Runtime.exceptionThrown')errors.push(message.params.exceptionDetails.text);
    if(message.method==='Log.entryAdded'&&message.params.entry.level==='error')errors.push(message.params.entry.text+' '+(message.params.entry.url||''));
  });
  await call('Page.enable');await call('Runtime.enable');await call('Log.enable');
  const paths=process.argv.slice(3);
  for(const path of paths.length?paths:['/','/slide_boot_loader_for_RL78/','/slide_simple_renesas_isp_tool/','/__rh850_serial__/','/__rh850_can__/']){
    const requested=new URL(path,base);let expected=new URL(requested);
    const legacy=/^\/(slide_(?:boot_loader_for_(?:RL78|RH850)|simple_renesas_isp_tool))\/(?:index.html)?$/.exec(requested.pathname);
    if(legacy&&requested.hash){
      const map=JSON.parse(await readFile(join(web,legacy[1],'guide-map.json'),'utf8'));
      const item=map.aliases[decodeURIComponent(requested.hash.slice(1))];
      if(item){expected=new URL(item.page,requested);expected.hash=item.anchor;}
    }
    await call('Emulation.setDeviceMetricsOverride',{width:1366,height:900,deviceScaleFactor:1,mobile:false});
    await call('Page.navigate',{url:base+path});
    await wait(()=>evaluate('location.pathname==='+JSON.stringify(expected.pathname)),'destination route '+path);
    if(!expected.pathname.startsWith('/__')){
      let htmlPath=expected.pathname==='/'?'/released.github.io/index.html':expected.pathname;
      if(htmlPath.endsWith('/'))htmlPath+='index.html';
      const html=await readFile(join(web,htmlPath.slice(1)),'utf8');
      const title=/<title>([^]*?)<\/title>/.exec(html)?.[1];
      if(title)await wait(()=>evaluate('document.title==='+JSON.stringify(title)),'new document commit '+path);
    }
    if(/\/data\/update_[A-Za-z0-9_]+\.html$/.test(path)){
      await wait(()=>evaluate("!!document.querySelector('h1') && !!document.querySelector('table')"),'report summary '+path);
      const report=await evaluate("({title:document.title,pathname:location.pathname,width:innerWidth,scroll:document.documentElement.scrollWidth,rows:document.querySelectorAll('tr').length,text:document.body.innerText})");
      assert.ok(report.rows>10&&report.text.includes('input_bytes')&&report.text.includes('source_sha256'),'portable report fields');
      assert.ok(!/(?<![A-Za-z])[A-Za-z]:[\\/]|_Avery|SourceCode/.test(report.text),'no private report paths');
      assert.ok(report.scroll<=report.width+1,'report desktop width');
      await capture('rh850_report_desktop');
      await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:true});
      const narrow=await evaluate('({width:innerWidth,scroll:document.documentElement.scrollWidth})');
      assert.ok(narrow.scroll<=narrow.width+1,'report narrow width');await capture('rh850_report_narrow');
      delete report.text;results.push({path,...report,narrow});console.log('PASS browser report',path);continue;
    }
    await wait(()=>evaluate("!!document.querySelector('#content h1') && !document.querySelector('.loading,.render-error')"),'render '+path);
    await wait(()=>evaluate("!document.querySelector('.render-warning') && [...document.querySelectorAll('.mermaid')].every(n=>!!n.querySelector('svg'))"),'all Mermaid '+path);
    await evaluate('document.fonts.ready.then(()=>true)');
    await wait(()=>evaluate("[...document.querySelectorAll('#content img')].every(i=>i.complete)"),'all images '+path);
    // Short overviews finish before the existing200ms sidebar width transition.
    // Flush layout and await real CSS animations instead of sampling mid-transition.
    await evaluate('(()=>{document.querySelector("#content").getBoundingClientRect();return Promise.all(document.getAnimations().map(a=>a.finished.catch(()=>null))).then(()=>true);})()');
    const data=await evaluate(`(()=>{const ids=[...document.querySelectorAll('[id]')].map(n=>n.id);return {
      title:document.title,pathname:location.pathname,hash:location.hash,
      contentIds:[...document.querySelectorAll('#content a[id],#content h1[id],#content h2[id],#content h3[id]')].map(n=>n.id),
      headings:[...document.querySelectorAll('#content h1,#content h2,#content h3')].map(n=>({id:n.id,text:n.textContent,tag:n.tagName})),
      diagrams:document.querySelectorAll('.mermaid').length,tables:document.querySelectorAll('#content table').length,
      duplicateIds:ids.filter((id,i)=>ids.indexOf(id)!==i),brokenAnchors:[...document.querySelectorAll('a[href^="#"]')].filter(a=>a.hash&&!document.getElementById(decodeURIComponent(a.hash.slice(1)))) .map(a=>a.hash),
      width:innerWidth,scroll:document.documentElement.scrollWidth,images:[...document.querySelectorAll('#content img')].filter(i=>i.complete&&!i.naturalWidth).map(i=>i.src)};})()`);
    assert.deepEqual(data.duplicateIds,[],'unique DOM IDs '+path);
    assert.deepEqual(data.brokenAnchors,[],'all in-page anchors '+path);
    assert.deepEqual(data.images,[],'all loaded images '+path);
    const name=path==='/'?'home':path.replace(/[^A-Za-z0-9_-]+/g,'_');
    await capture(name+'_desktop');
    assert.ok(data.scroll<=data.width+1,'desktop document width '+path+' '+data.scroll+'/'+data.width+' evidence='+out);
    const anchor=path.includes('#')?decodeURIComponent(expected.hash.slice(1)):
      await evaluate('decodeURIComponent(document.querySelector("#toc a")?.hash.slice(1)||"")||null');
    if(anchor){
      const clicked=await evaluate(`(()=>{const a=document.querySelector('a[href="#${anchor}"]');if(!a)return false;a.click();return true;})()`);
      if(!clicked)await evaluate(`document.getElementById(${JSON.stringify(anchor)}).scrollIntoView({block:'start',behavior:'instant'})`);
      await pause(500);
      assert.equal(await evaluate('decodeURIComponent(location.hash.slice(1))'),anchor,clicked?'actual anchor click':'actual deep-link target');
      await capture(name+'_anchor');
    }
    if(path.includes('__rh850_')){
      const count=await evaluate('document.querySelectorAll(".mermaid").length');
      for(let i=0;i<count;i++){
        await evaluate(`document.querySelectorAll('.mermaid')[${i}].scrollIntoView({block:'center',behavior:'instant'})`);
        await capture(name+'_diagram_'+(i+1));
      }
    }
    await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:true});
    await evaluate('scrollTo(0,0)');await pause(300);
    // Resize retains an explicitly opened desktop sidebar; verify the actual toggle.
    if(await evaluate('document.body.hasAttribute("html-show-sidebar-toc")'))await evaluate('document.querySelector("#sidebar-toc-btn").click()');
    await pause(250);
    const narrow=await evaluate(`({width:innerWidth,scroll:document.documentElement.scrollWidth,containers:[...document.querySelectorAll('table,pre,.mermaid')].filter(n=>n.scrollWidth>n.clientWidth+1).length})`);
    assert.ok(narrow.scroll<=narrow.width+1,'narrow document width '+path);
    await capture(name+'_narrow');
    await evaluate('document.querySelector("#sidebar-toc-btn").click()');
    assert.equal(await evaluate('document.querySelector("#sidebar-toc-btn").getAttribute("aria-expanded")'),'true');
    await capture(name+'_toc');
    await evaluate('document.querySelector("#sidebar-toc-btn").click()');
    results.push({path,...data,narrow});console.log('PASS browser',path,data.diagrams,'Mermaid',data.tables,'tables');
    if(/^\/slide_boot_loader_for_(?:RL78|RH850)\/$/.test(path)){
      const shortcut=await evaluate("[...document.querySelectorAll('#content a')].find(a=>a.textContent.includes('查看更新耗時表'))?.getAttribute('href')");
      assert.ok(shortcut,'visible timing shortcut');
      await evaluate("[...document.querySelectorAll('#content a')].find(a=>a.textContent.includes('查看更新耗時表')).click()");
      const target=new URL(shortcut,base+path);
      await wait(()=>evaluate('location.pathname==='+JSON.stringify(target.pathname)+' && !!document.getElementById('+JSON.stringify(target.hash.slice(1))+') && !!document.querySelector("#content table")'),'actual timing shortcut '+path);
      navigation.push({kind:'timing-shortcut',from:path,to:target.pathname+target.hash});
      await call('Page.navigate',{url:base+path});
      await wait(()=>evaluate('location.pathname==='+JSON.stringify(path)+' && !!document.querySelector("#content h1") && !![...document.querySelectorAll("#content a")].find(a=>a.textContent.includes("查看更新耗時表"))'),'return to overview');
    } else if(/^\/slide_boot_loader_for_(?:RL78|RH850)\/[a-z-]+\.html$/.test(new URL(path,base).pathname)){
      const peerPath=path.includes('for_RH850')?'slide_boot_loader_for_RL78':'slide_boot_loader_for_RH850';
      const peerLink=await evaluate('[...document.querySelectorAll("#content a")].find(a=>a.getAttribute("href")?.startsWith('+JSON.stringify('../'+peerPath+'/')+') && a.hash)?.getAttribute("href")');
      if(peerLink){
        await evaluate('[...document.querySelectorAll("#content a")].find(a=>a.getAttribute("href")==='+JSON.stringify(peerLink)+').click()');
        const target=new URL(peerLink,base+path);
        const brand=peerPath.includes('RH850')?'RH850 Bootloader':'RL78 Bootloader';
        await wait(()=>evaluate('location.pathname==='+JSON.stringify(target.pathname)+' && !!document.getElementById('+JSON.stringify(target.hash.slice(1))+') && document.querySelector(".sidebar-brand strong")?.textContent.includes('+JSON.stringify(brand)+')'),'actual matching-platform topic');
        navigation.push({kind:'platform-topic',from:path,to:target.pathname+target.hash});
      }
    }
    if(/^\/slide_(?:boot_loader_for_(?:RL78|RH850)|simple_renesas_isp_tool)\/$/.test(path)){
      const link=await evaluate('document.querySelector("#content a[href$=\\".html\\"]")?.getAttribute("href")');
      assert.ok(link,'overview chapter link '+path);
      await evaluate('document.querySelector("#content a[href$=\\".html\\"]").click()');
      const target=new URL(link,base+path);
      const markdown=target.pathname.split('/').at(-1).replace(/\.html$/,'.md');
      await wait(()=>evaluate('location.pathname==='+JSON.stringify(target.pathname)+' && document.body.getAttribute("data-markdown")==='+JSON.stringify(markdown)+' && !!document.querySelector("#content h1")'),'actual overview chapter click '+path);
      navigation.push({from:path,to:target.pathname});
    }
  }
  assert.deepEqual(errors,[],'browser console exceptions/errors');
  await writeFile(join(out,'results.json'),JSON.stringify({results,navigation,errors},null,2));
  console.log('PASS actual browser render/anchors/DOM/tables/narrow/TOC. Evidence:',out);
}finally{
  if(socket){try{await call('Browser.close');}catch{}socket.close();}
  if(child.exitCode===null)await Promise.race([new Promise(r=>child.once('exit',r)),pause(3000)]);
  // Only the private child created by this fixture,never another browser session.
  if(child.exitCode===null)child.kill();
  server.close();
}
