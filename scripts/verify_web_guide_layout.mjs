// Read-only chapter,source-placement and legacy-bookmark verification.
import assert from 'node:assert/strict';
import {readFile,stat} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {runInNewContext} from 'node:vm';
import {verifyBootloaderParity} from './verify_bootloader_parity.mjs';
const tool=resolve(fileURLToPath(new URL('../',import.meta.url)));
assert.ok(process.argv[2],'supply the local web root');
const web=resolve(process.argv[2]);
const layout=JSON.parse(await readFile(join(tool,'scripts/web_guide_layout.json'),'utf8'));
const rendered=process.argv[3]?JSON.parse(await readFile(process.argv[3],'utf8')):null;
let chapters=0,aliases=0;
for(const [site,config] of Object.entries(layout)){
  const root=join(web,site);
  const map=JSON.parse(await readFile(join(root,'guide-map.json'),'utf8'));
  assert.deepEqual(map.pages,config.pages,'chapter catalog '+site);
  const files={};
  for(const page of Object.keys(config.pages)){
    const md=await readFile(join(root,page+'.md'),'utf8');files[page]=md;
    const html=await readFile(join(root,page+'.html'),'utf8');
    for(const source of [md,html])assert.ok(!/(?<![A-Za-z])[A-Za-z]:[\\/]|_Avery|SourceCode/.test(source),'no private path '+page);
    assert.ok(html.includes('guide-routes.js'),'legacy routing '+site+'/'+page);
    assert.ok(page==='index'||html.includes('data-markdown="'+page+'.md"'),'source '+page);
    assert.ok(md.includes('id="article_top"'),'page top '+page);
    if(page==='index')assert.ok(md.split('\n').length<100,'overview must remain short');
    else {assert.ok(md.includes('](index.html)'),'overview backlink '+page);chapters++;}
    for(const match of md.matchAll(/\]\(([^\s)]+)\)/g)){
      const ref=match[1];if(/^(?:https?:|mailto:|#)/.test(ref))continue;
      const path=ref.split('#')[0];
      const target=path.startsWith('/')?join(web,path.slice(1)):join(root,path);
      await stat(target);
    }
    if(rendered){
      const route='/'+site+'/'+(page==='index'?'':page+'.html');
      const actual=rendered.results.find(r=>r.path===route);
      assert.ok(actual,'missing actual browser result '+route);
      assert.equal(actual.pathname,route,'no unexpected page redirect '+route);
    }
  }
  for(const [section,page] of Object.entries(config.sections)){
    const begin='<!-- '+section+'_BEGIN -->',end='<!-- '+section+'_END -->';
    let count=0;
    for(const [name,md] of Object.entries(files)){
      const found=md.split(begin).length-1;count+=found;
      if(found){assert.equal(name,page,'section destination '+section);assert.equal(md.split(end).length-1,1);}
    }
    assert.equal(count,1,'one section per site '+section);
  }
  for(const [id,target] of Object.entries(map.aliases)){
    assert.ok(id&&target.anchor,'nonempty legacy bookmark');
    assert.ok(target.page==='basic_uart_i2c/'||Object.hasOwn(config.pages,target.page.replace(/\.html$/,'')),
      'allowlisted local destination '+target.page);
    assert.ok(!/[?#\\]|\.\./.test(target.page),'no query,path traversal or external route');
    if(rendered){
      const path='/'+site+'/'+(target.page==='index.html'?'':target.page);
      const actual=rendered.results.find(r=>r.path===path);
      assert.ok(actual,'missing browser destination '+path);
      assert.ok(actual.contentIds.includes(target.anchor),'missing legacy target '+site+'/#'+id+' -> '+target.page+'#'+target.anchor);
    }
    aliases++;
  }
  const script=await readFile(join(root,'guide-routes.js'),'utf8');
  assert.ok(!/(?<![A-Za-z])[A-Za-z]:[\\/]|_Avery|SourceCode/.test(script),'no generated private path '+site);
  assert.ok(script.includes(JSON.stringify(map.aliases).replaceAll('<','\\u003c')),'generated route map parity');
  function route(hash,existing=false){
    const url=new URL('https://released.github.io/'+site+'/?view=example'+hash);
    let replaced=null;const handlers={};
    const location={href:url.href,hash:url.hash,origin:url.origin,pathname:url.pathname,search:url.search,
      replace(value){replaced=value;}};
    runInNewContext(script,{URL,location,document:{body:{hasAttribute(){return false;}},
      getElementById(){return existing?{}:null;}},addEventListener(name,fn){handlers[name]=fn;}});
    return replaced;
  }
  for(const [id,target] of Object.entries(map.aliases)){
    const expected=new URL(target.page,'https://released.github.io/'+site+'/');
    expected.search='?view=example';expected.hash=target.anchor;
    assert.equal(route('#'+encodeURIComponent(id)),expected.href,'actual route JS '+id);
  }
  for(const hash of ['#constructor','#__proto__','#unknown-guide-bookmark','#%E0%A4']){
    assert.equal(route(hash),null,'unknown/malformed fragment does not redirect '+hash);
  }
  assert.equal(route('#'+Object.keys(map.aliases)[0],true),null,'existing local anchor wins');
  if(site!=='slide_boot_loader_for_RH850'){
    const link=files.index.match(/<!-- RH850_DEVICE_BEGIN -->([^]*?)<!-- RH850_DEVICE_END -->/);
    assert.ok(link&&link[1].includes('slide_boot_loader_for_RH850/'),'independent RH850 navigation');
    assert.ok(link[1].length<700&&!link[1].includes('```mermaid'),'short RH850 navigation only');
  }
}
const app=await readFile(join(web,'notices_can/assets/app.js'),'utf8');
assert.ok(app.includes('getAttribute("data-markdown") || "index.md"'),'default renderer contract');
assert.ok(app.includes('"guide:ready"'),'navigation ready event');
const homeUrls=[];
for(const file of ['README.md','README.html']){
  const source=await readFile(join(web,'released.github.io',file),'utf8');
  const urls=[...source.matchAll(/https:\/\/released\.github\.io\/(slide_(?:boot_loader_for_(?:RL78|RH850)|simple_renesas_isp_tool))\/[^\s)"<>]*/g)];
  homeUrls.push(urls.map(m=>m[0]).sort());
  for(const match of urls){
    const url=new URL(match[0]);const path=url.pathname.endsWith('/')?url.pathname+'index.html':url.pathname;
    await stat(join(web,path.slice(1)));
    if(url.hash){
      const map=JSON.parse(await readFile(join(web,match[1],'guide-map.json'),'utf8'));
      assert.ok(map.aliases[decodeURIComponent(url.hash.slice(1))],'homepage bookmark '+url.href);
    }
  }
}
assert.deepEqual(homeUrls[0],homeUrls[1],'knowledge-base MD/HTML guide link parity');
await verifyBootloaderParity(web);
console.log('PASS',chapters,'chapter pages,',aliases,'actual-JS routes,placements/assets/short overviews and independent MCU guides',rendered?'and actual browser destination IDs':'(browser IDs checked separately)');
