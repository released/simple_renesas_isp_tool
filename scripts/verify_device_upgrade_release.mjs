// Publication checks are stricter than the full local engineering preview.
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';
import {publicUpgradeKeys} from './device_upgrade_content.mjs';
import {verifyDeviceUpgradePaths} from './verify_device_upgrade_paths.mjs';

const tool = resolve(fileURLToPath(new URL('../', import.meta.url)));
assert.ok(process.argv[2], 'supply the exact publication WebRoot');
const profiles = JSON.parse(await readFile(join(tool, 'docs/data/device_upgrade_paths.json'), 'utf8'));
assert.deepEqual(profiles.map(profile => profile.key).sort(), [...publicUpgradeKeys].sort(),
  'Publication requires the exact board-accepted guide subset; keep other engineering paths local.');
await verifyDeviceUpgradePaths(resolve(process.argv[2]));
console.log('PASS exact accepted guide publication scope; no engineering-only role is included');
