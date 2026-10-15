# RL78/F13 application metadata and CRC post-build

## Decision

Keep the F13 PowerShell post-build generator. The F24 SRecord projects remain a
useful CRC/layout reference, but the F13 generator also validates Intel HEX,
reads the linker map, patches format6 metadata, and supports a variable actual
image length.

For the full integration workflow rather than CRC packaging alone, read the
[RL78 Boot/App porting guide](RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md).

## F24 reference

The active embedded-CRC example is:

`D:\SourceCode\_Avery_Renesas\CS+\RL78_F24_Boot_loader_app_CRC`

- `RL78_F24_Boot_loader_app.mtpj` runs `boot_flash.bat` after a successful
  build.
- `2generateChecksum.cmd` crops the configured app range and applies
  SRecord `-crc32-l-e`.
- `3generateCRCHex.cmd` stores the four CRC bytes in the output HEX.
- `4generateCRCbin.cmd` converts the protected HEX to BIN.
- `app_HW_CRC` is not the active embedded-CRC reference because its checksum
  generation steps are commented out.

## F13 active flow

CS+ runs this post-build command from
`RL78_F13_Boot_loader_CAN_app.mtpj`:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "generate_update_image.ps1"
```

In CS+, configure it at **CC-RL Build Tool > Common Options > Build Events >
Commands executed after build**.

The script reads layout/identity constants emitted by `ftable.inc`; it does not
own a duplicate `0x2000` app start, `0x1E000` capacity, or target-ID rule. It:

1. Parses and validates every Intel HEX record and checksum.
2. Rejects duplicates, boot-range content, out-of-range content, malformed
   records, map/HEX disagreement, and missing EOF.
3. Reads the loaded application-section end from the linker map. The divided
   HEX can be padded to flash end, and valid data can end in `0xFF`, so neither
   HEX span nor the last non-`0xFF` byte is used as the logical image size.
4. Aligns the CRC address upward to four bytes and derives exact image size as
   `CRC address + 4 - application start`.
5. Validates the256-byte `CAN3` region (128-byte core with UPD1 policy and
   128 reservedFF bytes) at image offset `0x100`, then patches its exact
   image-size field.
6. Fills gaps inside the derived image with `0xFF` and calculates reflected
   CRC-32/ISO-HDLC over every byte except the final four.
7. Stores CRC32 little-endian in the final four bytes.
8. Emits the variable-length update BIN,
   `flash_app_variable_CRC.hex`, and combined boot+app artifacts for initial
   programming.

Current output files are:

| File | Purpose |
| --- | --- |
| `DefaultBuild\RL78_F13_Boot_loader_CAN_app_update.bin` | App update image loaded by the PC tool |
| `DefaultBuild\flash_app_variable_CRC.hex` | CRC-protected app HEX over its actual range |
| `boot_app.hex` / `boot_app.bin` | Initial combined boot+app programming artifacts |

The generator also removes the known obsolete fixed-range HEX/BIN names. This
prevents a saved tool path or manual file selection from confusing an old
122,880-byte format-2 artifact with the current format6 image.

When the app code grows or shrinks, the map-derived update BIN changes length
automatically. When the boot/app partition changes, update the boot project's
authoritative configuration, the app linker section addresses, and the CS+
divided-output range together; rebuild both projects and run the independent
artifact checker. The PC tool then obtains the new start/capacity from metadata
without a new MCU profile.

There is deliberately no `APP_IMAGE_SIZE_60K` or `APP_IMAGE_SIZE_110K` build
define. `APP_REGION_SIZE` is the writable capacity, not a requested BIN size,
and `IMAGE_SIZE_POST_BUILD` must remain the patch placeholder. If one build's
highest loaded section produces an aligned 60-KiB image, CRC32 occupies bytes
`61436..61439`; if a later build produces 110 KiB, CRC32 occupies bytes
`112636..112639`. In both cases the manifest size equals the emitted BIN size,
and the build fails if that size exceeds the declared capacity or flash end.

## Security boundary

The first release uses CRC-only integrity checking. Digital signatures,
authentication, and anti-rollback are not implemented. CRC protects against
corruption and interrupted transfer, not a maliciously modified image.

## Verification

After the CS+ boot and app builds, run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\verify_f13_artifacts.ps1
```

The checker independently validates boot growth, app boundaries, format6
manifest identity/layout/CAN fields, exact image length, stored CRC,
boot/app agreement, and shared-vector mappings.
