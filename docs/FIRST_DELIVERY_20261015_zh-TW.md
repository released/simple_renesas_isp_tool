# 第一版交付：F24 bridge / F13 CAN 更新

2026-10-15 | 工程第一版；不是產品版本改為1.0，也不是完整故障矩陣驗收。

## 1. 固定配套版本

| 元件 | 本次交付 | 注意事項 |
| --- | --- | --- |
| Windows x64 Tool | FirmwareUpgradeTool 0.1.0.0082；build 2026-10-15 17:55:32 | 保留實測EXE，不因整理文件而重新編譯 |
| F24 bridge Boot | 1.2.0 | 既有matching Boot不需重燒；Misc不更新Boot |
| F24 bridge App | 1.5.0；42696 bytes；CRC32 0x864E811E | 必須支援Host Link v2 / payload128 / burst22 / bit12 |
| F13 device Boot | 1.5.0；CAN protocol5 | 8 KiB；App start0x2000；530-byte餘裕 |
| F13 device App | 1.5.0；19244 bytes；CRC32 0xCA0DF55C | image format6；metadata256；IRQ ABI F1V1 |

第一版可交付範圍是PC UART Host Link115200 → F24 → Classical CAN500K → F13。
新版F13 v5不相容舊v4 DATA；初次須以燒錄器安裝matching Boot/App。
之後MCU、分區、IRQ ABI、metadata及更新契約不變的App，可只更新App。

## 2. 已有實板證據與限制

使用者提供 `firmware_upgrade_tool_20261015_182253.log`，已實際讀取核對：

- COM6 / Host115200成功識別F24 App1.5.0，四頁讀回完整256-byte metadata。
- F13 Boot1.5.0 / protocol5 / CAN500K；載入19244-byte App，CRC32 CA0DF55C。
- 301次burst / 301個成功COMMIT ACK，索引0..300與next address連續正確。
- 完整更新10963 ms，包含metadata與END；summary為1755 B/s。
- END回覆CRC32相符；無UART timeout/retry、UI overflow警告；CAN lost、TX
  failures、warning、passive、bus-off、recovery皆0。

```text
Update summary: 19244 bytes, 301 blocks ACKed, 10963 ms including metadata/END, 1755 B/s.
END acknowledged with matching CRC32; target is resetting to the application.
```

相較0080舊v4的約79秒，這次約快7.2倍；兩版映像19224／19244 bytes，並非
完全相同BIN，也只有單次v5結果，不能宣稱正式多次A/B或16倍實測。
先前182108 log在COM7無Host Link回應；182253改COM6即完成，不能據此判定F24 App壞掉。
COM號不是永久身分，仍以Open + Probe Host回覆為準，不自動探測其他COM。

此log只證明成功寫入及END驗證，不證明新App在reset後確實執行；仍需保存這一版
F13 UART的boot-validated / CAN update: completed / timer log，或重新Discover確認App。
原始使用者log目前未包含在交付目錄，請另行留存；不要用重建文字代替原始log。

尚未結案：實板斷電／拔線／bus-off／故障注入、IRQ runtime、完整DPI／殘影檢查、
460800／921600負載可靠性與多次效能測試。CRC不是簽章、認證或防降版。
G16 I2C只有reference探索，擦寫封鎖；UART device更新、RA/F14 backend未交付。

## 3. 上傳與安裝檔案

| 檔案（相對各專案） | 用途 |
| --- | --- |
| Tool：`build/FirmwareUpgradeTool.exe` | Windows x64 Release；不要拿Debug EXE或PDB代替 |
| F24 Boot：`firmware/RL78_F24_Bridge_boot/DefaultBuild/RL78_F24_Bridge_boot.mot` | 新板初裝用燒錄器；既有1.2.0可沿用 |
| F24 App：`firmware/RL78_F24_Bridge_app/DefaultBuild/RL78_F24_Bridge_app_bridge_update.bin` | Misc /115200更新HOST APP，完成後明確RUN並重新Probe |
| F13 App專案：`boot_app.hex` | 第一次以燒錄器安裝完整matching Boot/App；會覆蓋App，不是CAN更新BIN |
| F13 Boot專案：`DefaultBuild/boot0000_1FFF.hex` | 只含Boot區；單燒時須另外處理舊App，不能混用舊有效映像 |
| F13 App專案：`DefaultBuild/RL78_F13_Boot_loader_CAN_app_update.bin` | Tool CAN頁載入的DEVICE APP；不能拿到Misc更新host |

安裝順序：F24具備Boot1.2.0 → Misc更新App1.5.0 → Run Validated Bridge App →
重新Probe → F13初裝boot_app.hex → CAN500K載入F13 update.bin → Discover / 必要時
Enter Boot → Update → 保存END及reset後App證據。F13 TJA1050T需5V；X1為4MHz，
F24為20MHz，不能交換clock或Flash driver設定。

原始碼一起上傳時，保留完整MTPJ、SC設定、產表／封裝腳本、JSON、board／driver
及必要library。F13 App直接引用相鄰Boot的f13_board_support及CAN設定，不能只給
App資料夾就宣稱可獨立重建；初次須先build Boot，再build App產生combined HEX。
公開原始碼建議採同層checkout；不需要原開發者的工作區目錄：

```text
projects/
  simple_renesas_isp_tool/
  RL78_F13_Boot_loader_CAN/
  RL78_F13_Boot_loader_CAN_app/
```

自訂目錄可使用FIRMWARE_F13_BOOT_PROJECT／FIRMWARE_F13_APP_PROJECT；
整合測試仍執行真實F13來源，不以複本替代。一般Tool建置不要求這些測試依賴。
不要上傳個人INI／COM歷史、未去識別的log／截圖、tmp、obj、PDB、舊EXE或IDE私有設定。
所需Flash library須依原授權條款散布；此文件不代表已完成第三方授權審核。
一般clean不是發佈清洗器，會保留delivery與runtime資料，不能依賴clean自動排除個資。
本輪只準備本機交付文件；未建立repository、commit、push、GitHub Release或發布網站。

## 4. SHA-256（2026-10-15交付檔）

以下逐一對應上方檔名；文字HEX的大小不是其Flash佔用容量。

| 項目 | 檔案bytes | SHA-256 |
| --- | ---: | --- |
| Tool EXE | 3100160 | `3CBC0EF9421EB8B6AA9ADEBACA53E16AEFAEB8C51760A52230B3254B6694EE36` |
| F24 Boot MOT | 55358 | `5875537C74D971CB4F47243926780FD4C3DAB82CF236737FE5A24431CDD10F47` |
| F24 App BIN | 42696 | `B263AAC4300F94B5D385444FD4022D31CE3F8C2AE6E7FA5A0CC3095FEF84EC11` |
| F13 combined boot_app.hex | 368687 | `3DA335BAB4374E645E30670F16B62964CB37CEBC053DAD9FC1571479F3FC9787` |
| F13 Boot HEX | 23178 | `F7CB8DCC69648EE0108210F32C339E016989B0372A50A2777D2DBA0E0A7253DB` |
| F13 App BIN | 19244 | `2C8C405A04A86F8C0B8E8CAC6AB834379BD43707C6600EA9F3666699ECED36B1` |

可用 `Get-FileHash -Algorithm SHA256 -LiteralPath '<檔案>'` 核對；重新build後須重新
確認版本、配套、metadata及雜湊，不得沿用此表聲稱新BIN已板測。

操作細節見[CAN v5](CAN_BLOCK_TRANSFER_V5_zh-TW.md)、
[F13流程](F13_BOOT_UPDATE_FLOW_zh-TW.md)、[Misc更新](SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md)。

## 5. 本輪交付核對

- 六個交付檔的bytes／SHA-256全部相符；沒有重編譯Tool或韌體。
- 只重新build測試程式；Debug／Release實際F24／F13 BIN測試皆通過。
- Intel HEX checksum／EOF、Boot界線、combined HEX對Boot及App BIN的逐byte比對、
  App CRC32與128-byte reserved FF皆通過。Boot頂端530 bytes為FF；ROM實際section
  end與IRQ符號仍以先前clean build／map檢查為證，不把尾端FF當linker section大小。
- Python一般測試24項通過、1項可選CC-RL fixture未跑；F13 IRQ類別因linker map
  已清除而初始化失敗，所以本輪完整suite並非全數通過。最終文件／PDF測試6項另行通過。
  未為重建map而改動已實測產物；完整map／IRQ artifact audit無法在本輪重跑。
- 四份canonical同步檢查、196個本機web引用／版本、24支clean script安全稽核通過；
  沒有執行clean刪除。三份相關PDF（30／10／11頁）重新產生並檢視版面。

這些核對不能代替上述未完成的實板／桌面驗收；網站仍只是本機稿。
