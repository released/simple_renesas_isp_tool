# RH850 R7F701684 Reset Vector 診斷與修復

## 獨立 CS+ 工程

實板驗證紀錄顯示，原 RAM helper 的 GET／單次 SET／完整讀回成功
（phase=0x40、write_attempts=1），移除 E1 後 UART 與 standalone CAN FD
Custom Transfer 恢復。這證實本次對策有效；尚未釐清持久入口何時／為何
變成 0x00028000，也尚未證明與原先 960 KiB 更新失敗的因果關係。
恢復後未另提供冷啟動 RBASE=0 的暫存器截圖，不將此讀值列為已觀察。

正常入口為 `Sample_Project_RH850_S1_ResetVector_Recovery` 獨立工程。
開啟同名 `.mtpj`、F7 建置，僅載入自己的 RAM ABS，再由 PC=FEDE0000
開始診斷。工程自備 startup、bounded 16 MHz／120 MHz 時鐘初始化、FCL、
標頭、測試與建置檢查；不依賴 CAN FD Transfer／Boot 或 Tool 的來源。
`custom_func.c` 的 hardware_init／loop 管理流程，預設只讀，單次 SET guard
維持不變。原工程來源與建置流程維持不變，既有除錯設定也予以保留。

完整操作以獨立工程根目錄的 README 為準。獨立工程的只讀診斷、受控單次
SET 與恢復流程已完成實板驗證；已恢復板子再次執行只讀測試時，預期
phase=0x11，且不會再次寫入。

以下記錄舊 helper 的設計與當時操作，供重現已完成的恢復使用；
**新操作不再把 recovery.abs 加入原 Transfer 工程。**

本工具處理 RH850/F1KM-S1 BLDC 板的特定啟動入口不一致：目前 Code Flash
Transfer 映像已讀回比對正確，手動從 PC=0 可執行，但重新上電後 RBASE
仍為 `0x00028000`。先讀取持久設定，確認是否符合這個修復條件；不推定
原先 960 KiB 更新失敗的原因。這是獨立工程診斷工具，不是 ISP Tool 更新流程。

## 使用範圍

- R7F701684、1 MiB Code Flash、38 blocks；X1=16 MHz、CPU=120 MHz。
- 預設只診斷。只有完整讀回的 Reset Vector 第一個 32-bit word 正好為
  `0x00028000`，且 live RBASE 基底也相同，才開放明確的單次寫入。
- 用既有合法 FCL T01 V2.13 SDK 建置；不另行下載／複製第三方來源。
- 修復只呼叫 `SET_RESET_VECTOR`，將第一個 word 改為 0，保留另外 12 bytes。
  寫入前重讀比對，寫入後驗證完整 16 bytes，並比對完整 32-byte OPBT 快照。
- 不呼叫 erase、page write、SET_OPB、ID／OTP／security 寫入；不自動重置或重試。
- 不更改 Boot／App／Transfer 專案、協定、Flash 分區或 MCU host。
- 舊 helper 與獨立工程的受控操作均有實板驗證紀錄；native model 與建置
  仍只驗證軟體模型及產物結構，不取代硬體驗證紀錄。

## RAM 配置

| 用途 | 地址 |
| --- | --- |
| `_RvEntry` | `0xFEDE0000` |
| RAM exception table | `0xFEDE0200` |
| 程式／常數／FCL 複製來源 | `0xFEDE1000..0xFEDE7FFF` 以內 |
| 工具 BSS／FCL 狀態 | `0xFEDF0000..0xFEDF1FFF` 以內 |
| 獨立 stack | `0xFEDF2000..0xFEDF2FFF`，初始 SP=`0xFEDF3000` |
| `g_rv` 狀態區 | `0xFEDF7000..0xFEDF70B7` |
| FCL 執行複本 | `0xFEDF8000..0xFEDFEFFF` |

避開既有工程所設定的 debugger work RAM：`0xFEBF4800` 起 9,728 bytes
（self-RAM alias 為 `0xFEDF4800..0xFEDF6DFF`），亦不使用最上端 4 KiB。
下載的 ABS／MOT 只包含指定 RAM 的程式資料；MOT 並非供 RFP 燒錄的 Flash 映像。
建置腳本會檢查實際 linker sections、symbols、FCL 複製大小及每筆 S-record。

## 舊 helper 建置（歷史重現用）

在 Tool 根目錄執行，SDK 路徑只作為既有依賴輸入，原工程不會被修改：

```powershell
$sdkProject = '<既有 RH850 FCL SDK 工程根目錄>'
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_rh850_reset_vector_recovery.ps1 -SdkProject $sdkProject
```

預設使用已安裝的 CC-RH V2.07.00，可用 `-CompilerBin` 指定另一個已安裝版本。
產物位於新建的 `build/rh850_reset_vector/<時間與識別碼>/`：
`recovery.abs`、`recovery.mot`、`recovery.map`、建置紀錄及來源／產物 hash。
中間檔在具所有權紀錄的隔離目錄建置並清除；正式硬體診斷映像保留至驗證結束。

## 先執行只讀診斷

1. 保留 `build/upload.bin` 與現有 UART／暫存器紀錄。關閉 RFP，使用已連線的
   CS+／E1 與自行供電的板子。
2. 先依已驗證的方式，將 Transfer 的 PC 改為 `0x00000000`，Continue，看到
   `CAN init: OK` 後按 Stop。這一步建立 16 MHz／120 MHz 的已知時鐘狀態。
   工具本身不初始化時鐘；若實際暫存器不符，會停止於 `error=1`。
3. 在 Debug Tool 的 **Download File Settings** 暫時登錄這次的
   `recovery.abs`。將所有原有檔案的 **Download object** 設為 **No**，只有
   `recovery.abs` 為 **Yes**，它的 **Download symbol information** 也為 **Yes**。
   記錄原設定，結束後還原，不必儲存這項暫時變更到原工程。
4. 確認 **CPU Reset after download = No**、
   **Execute to the specified symbol after CPU Reset = No**、
   **Erase flash ROM before download = No**。
   不開啟 Configuration／Security／Flash Options 寫入。
5. 選 **Debug → Download**，不要 Build & Download。這次是將診斷程式載入 RAM，
   不會把診斷程式寫進 Code Flash；來源映像與實際 linker／S-record 都有檢查。
6. 將 **PC 設為 `0xFEDE0000`**，Continue。不要 Reset，也不要跳進 `RvMain`
   取代 assembly entry；entry 負責自己的 stack／BSS／RAM exception table。
7. 約一秒後暫停，在 Memory1 將 Size Notation 設為 4 bytes、Hex，查看
   `0xFEDF7000..0xFEDF70B7`。工具沒有 UART 輸出；結果只放在 RAM 狀態區。
8. **第一次先保存並提供狀態區讀值。先不要寫 command。**
   `phase=0x10` 表示所有診斷條件通過、尚未寫入，`write_attempts` 必須為 0。

官方 CS+ 預設下載後會 Reset／執行到指定 symbol，因此上述設定必須明確關閉：
[CS+ RH850 Download 說明](https://tool-support.renesas.com/autoupdate/support/onlinehelp/csp/V8.06.00/CS%2B.chm/DebugTool-RH850.chm/Output/db_kinou_prog2-nav-1.html)。

## 狀態區

| Offset | 名稱 | 說明 |
| --- | --- | --- |
| `+00` | magic | `0x52563031` |
| `+04` | phase | 下表 |
| `+08` | error | 下表；正常為 0 |
| `+0C` | fcl_status | 原始 `r_fcl_status_t`；OK=0 |
| `+10` | rbase | 診斷開始前的 live RBASE |
| `+14` | command | 預設 0；明確啟用寫入的欄位 |
| `+18` | confirm_before | 確認原值；必須先設定這欄，再設定 command |
| `+1C` | write_attempts | 0 或 1；不是成功次數 |
| `+20` | block_count | 預期 38 |
| `+24` | last_block_end | 預期 `0x000FFFFF` |
| `+28` | last_command | 最後送出的 FCL enum 數值 |
| `+2C` | heartbeat | 執行時遞增，不代表寫入進度 |
| `+30..3F` | before[4] | 完整 16-byte 原設定 |
| `+40..4F` | after[4] | SET 成功後才讀取的 16-byte 設定 |
| `+50..5F` | desired[4] | 本次準備寫入的 16 bytes |
| `+60..7F` | options_before[8] | 完整 32-byte GET_OPB 結果 |
| `+80..9F` | options_after[8] | SET 成功後的 OPBT 快照 |
| `+A0..AF` | device_name | FCL 實際讀取的 16-byte 型號字串 |
| `+B0/+B4` | trap_pc／trap_cause | RAM exception handler 保存的 FEPC／FEIC |

| phase | 意義 |
| --- | --- |
| `1 / 2` | 時鐘檢查／FCL 準備中 |
| `0x10` | 診斷通過，等待明確寫入；尚未更改持久設定 |
| `0x11` | 持久第一個 word 已為 0；不寫入，需另查 live RBASE 差異 |
| `0x20` | 單次 SET 正在執行 |
| `0x30` | SET 已返回 OK，正在讀回確認 |
| `0x40` | Reset Vector 及 OPBT 讀回通過；此階段尚未包含冷啟動驗證 |
| `0xE0` | 終止；查看 error／fcl_status，不自動重試 |
| `0xF0` | RAM exception trap；查看 trap_pc／trap_cause |

error：1=時鐘不符；2=FCL 準備失敗；3=診斷命令失敗；4=型號不符；
5=容量不符；6=Reset Vector／RBASE 不符預設故障條件；7=寫入確認不符；
8=快照與新讀值不同；9=SET 失敗；10=讀回失敗／不符；11=OPBT 比對不符。

## 診斷確認後才執行單次修復

只在 `phase=0x10`、`error=0`、`write_attempts=0`，且 `before[0]=0x00028000`
與實際型號已確認時操作。保持 CPU 暫停：

1. 將 `0xFEDF7018`（confirm_before，32-bit）寫為 `0x00028000`。
2. 將 `0xFEDF7014`（command，32-bit）寫為 `0x52535430`，最後才寫此欄。
3. Continue。工具會重新檢查時鐘／RBASE 並讀取快照，符合後只送出一次 SET_RESET_VECTOR。
4. 等待 `phase=0x40` 或終止狀態；在 `0x20/0x30` 期間保持供電，不按
   Stop／Reset／斷電。若一直停在操作中，保留現場，不重送或重跑 entry。
5. 若 SET 失敗（error=9），結果未知。依 FCL 手冊，這時 GET_RESET_VECTOR
   的結果也不能當成可靠驗證；工具會停止，不再 GET、不自動重試。
6. 若 `phase=0x40`，先保存狀態區，再結束除錯、移除 E1 目標端、重新上下電。
   修復後的即時 RBASE 要等 reset 才重新載入；工具的 rbase 欄刻意保留原始快照。
7. 確認 UART 自行出現 Transfer banner／CAN init，並驗證 Tool Transfer。
   必要時 Hot Plug-in 確認 RBASE=0。這才算完成實板恢復。
8. 還原 CS+ 暫時的下載清單。修復不會把板子由 Transfer 換成 Boot；之後要測更新，
   才另行安裝正確的 Boot，使用原始 App 先驗證，再測大容量映像。

診斷程式會覆寫自己的 RAM 範圍；執行後不要直接跳回舊 Transfer／Boot 的 C 流程。
成功後用完整重新上電重新初始化 RAM。失敗則保留供電與讀值，先判讀結果。

## 驗證與限制

```powershell
$sdkProject = '<既有 RH850 FCL SDK 工程根目錄>'
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\test_rh850_reset_vector_recovery.ps1 -SdkProject $sdkProject
```

native runner 編譯實際 `recovery.c`，FCL／硬體為模型，涵蓋未授權、既有設定、
各項 guard、完整設定快照、單次 SET、讀回、vendor error 與終止後不重試。
target build 使用實際 SDK，檢查真正輸出；保留工具鏈授權警告的原文，不宣稱零警告。
原 MCU 工程、Tool EXE、操作 log／report／upload.bin 均不屬於清除範圍。

[FCL 手冊 §4.4.24／4.4.25](https://www.renesas.com/en/document/apn/rh850-code-flash-libraries)：
SET／GET 的 Reset Vector buffer 都是 16 bytes；SET 失敗後 GET 不可靠。
[Flash Hardware Interface §8.4.1](https://www.renesas.com/en/document/mah/rh850f1k-users-manual-flash-hardware-interface)：
Variable Reset Vector 與一般程式映像不同，更新後需 reset 才套用。
