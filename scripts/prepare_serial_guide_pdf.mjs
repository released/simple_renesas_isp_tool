// Prepare the existing combined platform PDF from the same web chapter sources.
import assert from 'node:assert/strict';
import {writeFile,lstat} from 'node:fs/promises';
import {resolve,relative,sep,isAbsolute,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {serialPdfSource} from './serial_guide_content.mjs';
const tool=resolve(fileURLToPath(new URL('../',import.meta.url)));
const [platform,destination]=process.argv.slice(2);
assert.ok(['RL78','RH850'].includes(platform)&&destination,'supply platform and owned local output path');
const output=resolve(destination),local=relative(resolve(tool,'tmp'),output);
assert.ok(local&&!isAbsolute(local)&&!local.startsWith('..')&&!local.startsWith(sep)&&output.endsWith('.md'),'local generated PDF input only');
for(let folder=dirname(output);;folder=dirname(folder)){
  assert.ok(!(await lstat(folder)).isSymbolicLink(),'no linked PDF input ancestor');
  if(folder===tool)break;
}
await writeFile(output,await serialPdfSource(tool,platform));
console.log('PASS prepared serial PDF source:',platform);
