# Multi-bus bridge implementation plan

Status: implementation in progress; hardware upgrade validation pending.

Shared-console follow-up: CAN/I2C (future UART device) retain one BIN/metadata/
log/progress pool; tabs expose only bus settings/actions. Misc is last, with
explicit HOST BRIDGE UART destination and separate BUR1 contract, optional
feature10/INFO50 and board-advertised geometry. F24 split boot/app share the
canonical SC hardware source; no new register logic in UI. See
BRIDGE_SELF_UPDATE_PROTOCOL.md and SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md.

## Scope and baseline

The existing CAN500K Transfer benchmark and F13 boot/app1.1.1 must remain
unchanged. The PC product is FirmwareUpgradeTool in simple_renesas_isp_tool.
CAN and I2C share the serial session, image/log/progress controls and status
presentation, but retain separate bus configuration and target protocols.

Implementation order: rename/migrate settings; capability/port contract; generic
I2C staged transaction and F24 backend; worker/UI tabs and status theme;
reference-protocol discovery, mocks/builds and documentation verification.

## Layer ownership

UI -> worker -> target protocol -> CanHost/I2cHost client -> Host Link v2 ->
Bridge API -> board backend/registers. UI and target profiles never access MCU
registers or select pins by board-ID switch statements. RA2L1/RA6 must implement
the same Bridge_I2cBackend APIs as F24, using their own FSP/registers underneath.

Port identifiers are backend-local numeric IDs, not schematic CAN connector
numbers. GET_PORT_INFO reports count, ID, flags, supported speeds, transfer
limits and route label. UI never assumes RA6 port counts from its MCU family;
only enabled, routed firmware ports are selectable. Legacy CAN-only firmware
without the catalog feature gets one implicit CAN0 and no I2C ports.

The supplied schematic has RA6 CAN0/1 and IIC0/1 routes, F24 IICA0 P62/P63,
and RA2 IIC0. This is board wiring evidence, not implemented RA firmware.
F24 generated IICA0 uses SCL=P62, SDA=P63, CPU40MHz and 100kHz settings.
Do not copy RA register values, slave IDs or flash layout into the F24 bridge.

## Additive Host Link v2 contract

Frame version/payload limits and existing CAN commands remain unchanged.
Feature bit8=I2C master, bit9=port catalog. Added status12=I2C NACK,
13=arbitration lost, 14=I2C bus error; timeout remains6.

GET_PORT_INFO (04): request [bus,index], bus0=CAN/1=I2C. Response32 bytes:
bus,count,id,flags; LE32 rate-mask; LE16 max-TX/max-RX; 20-byte NUL-padded
ASCII route label. Flags bit0=enabled, bit1=I2C repeated-start, bit2=CAN FD. Count0 is valid
for an unavailable bus (index must0), bounded max16 ports per bus.
SELECT_CAN_PORT (14): [id], echo response, CAN stopped only.
SET_I2C_CONFIG (40): [port,speed,reserved0,reserved0], echo response;
speed0=100kHz,1=400kHz. Initial F24 advertises only100kHz.

I2C_TRANSACTION (41) has suboperations:

- BEGIN0: [0,port,address7,flags,txlenLE16,rxlenLE16,timeoutLE16,gapLE16].
  Address08..77, flagbit0=repeated-start; nonzero total length, lengths<=512,
  timeout1..30000ms, gap<=timeout; repeated-start requires TX+RX and gap0.
- CHUNK1: [1,offsetLE16,data1..125], contiguous offsets only. No I2C traffic.
- EXECUTE2: [2], starts the one complete physical transaction asynchronously;
  response only after completion, [rxlenLE16]. Never repeat a side effect.
- FETCH3: [3,offsetLE16,count1..125], response[offsetLE16,data], completed RX only.
- ABORT4: [4], idempotent cleanup/release. Empty response.

UART chunks assemble a whole I2C packet; they must NOT create intermediate I2C
STOP boundaries. BEGIN/CHUNK do not touch the target. Staging expires after a
bounded idle interval. Active CAN/I2C transactions serialize; another bus
cannot change configuration while a transfer is active. Identical active
EXECUTE retries wait for the original result; completed retries replay it.
Changed payload with the same sequence is SEQUENCE_ERROR.

## Reference protocol and risks

The supplied G16 RA6M3 sample configures7-bit address09 and ~100kHz. It uses
00 communication setup, 55/repeated-start/C3 boot check, inquiry
01 00 01 00 FF 03 -> 81 00 02 00 00 FE 03. Packet length/address fields are
big-endian and integrity is an 8-bit additive two's-complement checksum, NOT
the CAN manifest CRC32 nor the Host Link CRC16. Maximum image payload256
produces a262-byte packet: staged transport is mandatory.

Reference erase/write/status/read/signature/area/authentication and app jump
must remain PC target-protocol responsibilities. No blind bus-wide address
scan, guessed image range, default authentication or silent flash erase.
Programming requires identified flash geometry and a compatible target/image
contract; generic I2C transfer success is not firmware-update success.

Validate pull-up voltage, common ground, open-drain lines, rise time, clock
stretching, NACK/address conventions and external power before hardware use.
Address09 is configurable, never shift it in UI; only backend emits address<<1.
Repeated-start retains bus ownership until the final STOP. ISR moves bounded
bytes/captures completion or errors; no logs, command parsing or waits in ISR.
Timeout/abort releases the peripheral; it cannot guarantee recovery from a
slave physically holding SDA LOW. WDT remains out of scope.

## Verification gates

### G16 source audit and revised safe scope

The adjacent supplied slave project was inspected read-only. Its active
boot_main.c reports label RL78_G16_FPB (not a silicon part number) and legacy
type02, one code area1000..7FFF, erase1024, write/read256. Its application
linker generates CRC16/CCITT-MSB(init0000), little-endian2 bytes at7FFC;
the final4-byte slot is not a CRC32. Inquiry advances the stage named
AUTHENTICATION_PHASE without validating an ID; command30 and READ15 have
no active receive-handler case. The master read-back block is #if0.

The slave boot-entry function has no active CRC check, address validation
narrows32-bit fields to16 bits, ERASE loops over the whole app regardless of
the requested range, and SWITCH_APP may skip CRC after the final write.
Consequently the tool must not expose a usable flash-start action for this
known reference revision. Complete non-flash discovery/signature/geometry and
local CRC16 validation now, plus command encoders and tests. Actual erase,
write, read-back, app-entry/power-loss recovery require a corrected slave
contract and a separately verified programming workflow. No reference source
will be modified or reflashed by this task.

The UI must distinguish BIN fields from live target fields: raw G16 BIN has
no MCU/version/address manifest; those offline values are N/A. A checksum
match does not authenticate the BIN or prove target flash compatibility.


- [x] Active tool/F24 project moved; product references renamed; existing INI
  copied unchanged. Old directory retains only the locked original schematic.
- [x] Release/Debug x64 builds and full tests PASS, zero warnings/errors.
- [ ] Startup/DPI/CAN-I2C tab visual checks NOT RUN, native desktop pipe absent.
- [x] Port catalog old-host fallback, I2C-only host and multi-port mock checks.
- [x] Real engine I2C512-byte bounds,262-byte continuity and timeout/abort;
  PC cancellation/byte-identical EXEC retry/errors; real backend register/ISR mocks.
  Actual F24 Host Link dispatcher replay/sequence behavior remains source-reviewed
  and compile-checked, not directly executed in a dispatcher harness or hardware.
- [x] F24 clean build43, Error0/Warning2 (existing license); generated edits
  only in preserved Config_IICA0_user include/IRQ sections.
- [x] Workspace clean-script audit:24 scripts PASS.
- [x] Four new Mermaid flows and eight-page Traditional Chinese PDF rendered
  and visually reviewed; F13 guide16 flows/22 pages regenerated after rename.
- [ ] Physical F24 I2C waveform/reference discovery and CAN regression.
- [ ] I2C target programming, power-loss/recovery and release evidence.
