# MCU CAN device 工程基準

## Golden sample

F13 與 F24 CAN device 的獨立 Boot／App 工程共同作為 metadata、向量表與建置流程的參考基準。
新增 MCU 或修改共通功能時，先對照這兩份工程，再列出新工程的硬體與分區差異。
基準涵蓋來源、建置與離線驗證；各 MCU 的實板驗證狀態分別記錄。

| 項目 | 共通基準 | MCU 專屬設定 |
| --- | --- | --- |
| Metadata 編輯 | `app_image_config.h` 的六個數值 define | App version／build、target ID、version control |
| IRQ 編輯 | `app_irq_handlers.h` 的 `APP_IRQ_HANDLERS(X)` 清單 | 合法 vector 位址與 ISR 名稱 |
| 建置 | CS+ pre-build 產表、post-build 驗證及封裝；Windows PowerShell／.NET | 編譯器、linker、startup 與 Flash driver |
| 自動產物 | `ftable.inc`、`app_identity.h`、`app_irq_table.inc`、App 更新 BIN | ASM wrapper 與 MCU 符號 |
| 編譯摘要 | post-build 產生 `DefaultBuild/app_build_report.html`；實際 BIN metadata／CRC／完整向量表 | 實際 image／iodefine.h 的中斷名稱 |
| Metadata／CRC | App offset0x100..0x1FF、256-byte 區、128-byte core、尾端 CRC32 | 分區、容量及硬體身分 |
| 向量 | Boot 固定完整轉送；App 自行產表；一般 ISR 使用 RETI | iodefine.h／硬體手冊的中斷來源 |
| CAN 更新 | metadata staging／SEAL、START、區塊 BEGIN／COMMIT、END | CAN backend、clock、pins、Flash API |
| 驗證 | 非法設定、stale metadata／向量、範圍與 CRC 拒絕；App A／B | RAM Flash 執行鏈、Boot 餘裕與實板功能 |

兩個設定 header 的完整編輯範例與核對步驟見
[Device App 設定](https://released.github.io/slide_boot_loader_for_RL78/#device-metadata-vector-workshop)。
Flash／metadata／CAN byte 規格見 [逐 byte 對照](https://released.github.io/slide_boot_loader_for_RL78/#flash-metadata-can-reference)。

## 設定讀取器

兩份 Boot repository 各自包含相同的 `app_config.ps1`，App 使用相鄰 Boot 的本機副本，
建置不依賴 Tool repository 或 Python。
Tool 的 `scripts/device_app_config.ps1` 維護共通讀取規格；`sync_device_app_config.ps1` 同步及檢查兩份副本。
`scripts/device_app_report.ps1` 同樣同步為各 Boot repository 的 `app_build_report.ps1`，
封裝完成後從實際 BIN 產生離線 HTML，不引入跨 repository 的報告依賴。
讀取器只接受文件列出的 C header 設定寫法，拒絕條件編譯、重複定義與任意運算式，
使產生器與實際 C 語意保持一致。

## 新增 MCU／修改功能的檢查順序

1. 對照 F13／F24 的設定 header、產表、封裝及 App-only 流程。
2. 列出 MCU、clock、pins、Flash driver、geometry、IRQ 來源與 ABI 的差異。
3. 共通功能調整時，同步兩份基準工程及驗證案例；硬體差異集中於各 MCU 的設定／backend。
4. 完成兩份 App 的 clean build、stale／錯誤拒絕與不同 IRQ mapping 的 A／B 建置。
5. App-only 變更確認 Boot 映像雜湊不變；共同契約變更另行同步 Boot／App 及驗證端。
6. 同步 MD／web／PDF；記錄離線與實板結果，不以建置通過代替 CAN／Flash／IRQ 驗證。

Tool repository 內可檢查共通讀取器：

```powershell
.\scripts\sync_device_app_config.ps1 -F13Boot <F13 Boot checkout> -F24Boot <F24 Boot checkout> -CheckOnly
```
