// Verify reader structure and real platform specifications, not only menu labels.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
const tool=resolve(fileURLToPath(new URL('../',import.meta.url)));
const read=p=>readFile(p,'utf8').then(s=>s.replace(/^\uFEFF/,'').replaceAll('\r\n','\n'));
const escape=s=>s.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');
export async function verifyBootloaderParity(web){
  const contract=JSON.parse(await read(join(tool,'scripts/bootloader_guide_contract.json')));
  const layout=JSON.parse(await read(join(tool,'scripts/web_guide_layout.json')));
  const sites=['slide_boot_loader_for_RL78','slide_boot_loader_for_RH850'];
  let topics=0;
  for(const [index,site] of sites.entries()){
    assert.deepEqual(Object.keys(layout[site].pages),['index',...Object.keys(contract)],'same chapter order');
    const home=await read(join(web,site,'index.md'));
    const timing=index?'rh850-update-timing':'device-update-timing';
    assert.ok(home.includes(`app-development.html#${timing}`)&&home.indexOf('查看更新耗時表')<home.indexOf('## 主題指南'),'prominent timing shortcut '+site);
    assert.ok(!home.includes('| 原始 App'),'one full measured-table owner');
    for(const [page,config] of Object.entries(contract)){
      const md=await read(join(web,site,page+'.md'));
      const body=md.replace(/<!-- BOOTLOADER_TOPICS_BEGIN -->[^]*?<!-- BOOTLOADER_TOPICS_END -->/,'');
      const prose=body.replace(/^(`{3,}|~{3,})[^\n]*\n[^]*?^\1\s*$/gm,'');
      assert.equal(layout[site].pages[page].title,config.title);
      assert.ok(body.includes('# '+config.title+'\n'),'equal reader title');
      for(const [label,heading,rl,rh] of config.topics){
        const id=index?rh:rl;
        assert.ok(new RegExp('^#{2,5} '+escape(heading)+'\\s*$','m').test(prose),'real topic body '+site+'/'+page+'/'+label);
        assert.ok(body.includes('id="'+id+'"'),'real topic anchor '+id);
        const peer=sites[1-index],peerId=index?rl:rh;
        assert.ok(md.includes(`../${peer}/${page}.html#${peerId}`),'counterpart topic link');
        topics++;
      }
    }
  }
  const htmls=await Promise.all(sites.map(s=>read(join(web,s,'index.html'))));
  for(const page of Object.keys(contract)){
    const documents=await Promise.all(sites.map(s=>read(join(web,s,page+'.md'))));
    const headings=text=>[...text.replace(/<!-- BOOTLOADER_TOPICS_BEGIN -->[^]*?<!-- BOOTLOADER_TOPICS_END -->/,'')
      .replace(/^(`{3,}|~{3,})[^\n]*\n[^]*?^\1\s*$/gm,'').matchAll(/^#{2,3} (.+)$/gm)].map(m=>m[1]);
    const [rl,rh]=documents.map(headings);
    for(const heading of rl.filter(h=>!/(?:F13|F24|RL78)/.test(h))){
      assert.ok(rh.includes(heading),'RL78 baseline chapter requires RH850 counterpart: '+page+'/'+heading);
    }
  }
  assert.equal(htmls[0].match(/<style>[^]*?<\/style>/)[0],htmls[1].match(/<style>[^]*?<\/style>/)[0],'same theme');
  const rhApp=await read(join(web,sites[1],'app-development.md'));
  for(const value of ['0x8501','0x10174','0x1045C','512','EIRET','app_image_config.h','app_irq_handlers.h','test_bins/auto','app_build_report.html','image_metadata_profile.c'])assert.ok(rhApp.includes(value),'RH850 spec '+value);
  const rhPort=await read(join(web,sites[1],'porting.md'));
  for(const value of ['0xFEDF4800','0xFEDF8000','R_FCL_CopySections','R_FCL_CMD_PREPARE_ENV','960','Device_Crc32Update'])assert.ok(rhPort.includes(value),'RH850 Flash/RAM spec '+value);
  const rhSerial=(await Promise.all(['serial','uart','i2c','xmodem'].map(page=>read(join(web,sites[1],page+'-device.md'))))).join('\n');
  for(const value of ['RLIN31','RIIC1','10174','Frame byte','111','CRC16','0xFFFFFFFF'])assert.ok(rhSerial.includes(value),'RH850 serial spec '+value);
  console.log(`PASS Bootloader parity:${Object.keys(contract).length} equal chapters /${topics/2} paired topics /${topics} real body anchors /same theme /direct timing and protocol shortcuts and platform specs`);
}
if(process.argv[1]&&resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  assert.ok(process.argv[2],'supply explicit WebRoot');await verifyBootloaderParity(resolve(process.argv[2]));
}
