# Host Link 高速與 device UART 固定更新速度

2026-10-15 | FirmwareUpgradeTool 0.1.0.0082

## 兩段 UART，不共用速度來源

上方 COM／Baud 是 PC - FT234XD - bridge 的 Host Link；上電固定115200，
Tool可選115200／460800／921600，完成身分驗證後才切速。這不是 device
更新周邊速度，CAN 1M／I2C 100K／未來device UART各自獨立。
目前F24 I2C仍僅100K；400K與UART device adapter／port driver未啟用。
未來UART TAB只放介面設定；BIN／metadata／log仍共用，Misc永遠最後。
UART TAB應顯示「Device UART baud（BIN固定）」且唯讀，不再建立另一個PC COM。
操作時先Close COM、選Host Baud，再Open + Probe；連線期間下拉選單鎖定。
成功後Host狀態列／log顯示實際會話名義速度，不只顯示選單偏好。

F24 APP新增feature bit11。05查詢、06切速、07確認，詳見Host Link protocol。
沒有bit11的舊APP不能使用高速；BOOT無bit11，按live descriptor的固定更新
速度連線。現行BOOT／BUR1固定115200。高速APP進Misc ENTER時ACK傳完後reset，
PC改用該BOOT速度重新驗證，不把APP的高速帶進擦寫。

## PC 接收延遲與 F13 更新速度（0080）

使用者提供的 `firmware_upgrade_tool_20261015_164225.log` 是0079／F24 App1.4.1／
Host115200／CAN500K／Detailed trace開啟的實板成功案例：BIN19224 bytes、
CRC32 `0x1DE10DCD`；DATA4806筆，每筆4bytes，由16:43:34到16:45:56約142秒，
有效速度約135B/s。整體更新由16:43:32到16:45:57約145秒，包含metadata／END／清理。
UART retry與timeout皆0；F24最後CAN RX/TX各4927、lost/TX failure/bus-off皆0。
搭配使用者UART截圖可確認Boot驗證後進入V2 App；不代表斷電／故障矩陣已完成。

既有PC讀取要求512bytes，但常見CAN交易回覆僅34bytes；原本ReadInterval及
ReadTotalTimeoutConstant皆20ms，短回覆仍受讀取逾時影響。0080改採
Win32 first-byte模式：ReadInterval與ReadTotalTimeoutMultiplier皆MAXDWORD，
ReadTotalTimeoutConstant為剩餘期限與50ms的較小值。**50ms只限制無資料等待，
不是每包固定延遲**。已有資料立即返回，分段封包交給原parser組合及驗CRC；
0ms呼叫使用立即drain模式。未改讀取大小／wire limit、重送或Flash安全規則。
寫入timeout及切baud穩定等待不變；未改FTDI driver／Latency Timer。
依據 [Microsoft COMMTIMEOUTS](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts)。

本輪使用實際SerialPort/Host Link程式搭配Win32 I/O模型測試短回覆、分段、
最大128-byte payload、CRC壞包、取消、逾時、tick wrap與4806筆交易。
這不是FTDI/Windows驅動/MCU實體耗時；舊log只有秒級精度，不能拆出各硬體延遲。
0080當時仍每4bytes等待一次完整PC往返。使用者171150 log同BIN／CRC，DATA約77秒、
整體約79秒；trace有缺行，不能以log筆數推算實際packet數。
0082新增泛用CAN_BURST_WAIT_RX22／bit12與F13 v5：64-byte區塊一次ACK，raw frame
可放8-byte image；19224B由4806次改為301次交易。Host Link128／frame142上限不變。
需更新F24 App1.5.0並重燒F13 Boot/App1.5.0；F24 Boot不變。
使用者182253 log已確認Host115200／CAN500K：19244B、301區塊、10963ms／1755B/s，
END CRC CA0DF55C相符、無重試或CAN錯誤；較舊約79秒快約7.2倍，但BIN略有不同且
僅單次效能結果，該筆 log 未包含完整 reset 後 App 確認。
目前正常 CAN 更新的 App 啟動確認已由 F13／F24 device 紀錄核對，見 [更新結果](https://released.github.io/slide_isp_tool_for_RL78/#f24-device-update-example)；
高速、連續壓力與多次 A/B 仍依各條件驗證。
詳見[區塊更新指南](CAN_BLOCK_TRANSFER_V5_zh-TW.md)。

比較方式：先用0080維持115200／CAN500K／相同BIN與Detailed trace設定，
保存新log以隔離本次接收改動；再關閉trace、分別測460800／921600。
每組至少重複3次，記錄DATA/整體耗時、CRC、timeout/retry、UART overflow與
CAN lost/error計數，確認reset後App啟動。Misc自身App更新仍按Boot固定115200。

## 時序與恢復

SET僅限CAN停止、無I2C保留／交易；所有判斷在main loop。舊速度ACK完整傳完
才停止UART、調整SDR並啟動。新速CONFIRM期限2秒，未完成就回115200；待確認
時拒絕其他會動作的命令。SET與CONFIRM都只送一次，避免向錯誤速度重送。
確認完成後不因閒置自行換速。正常Close／退出會盡力協商回115200；異常拔線
可能留下已確認的高速，Open只對使用者選定的COM有限探測三組速度，絕不掃描
並自動探測其他COM。Reset先從115200重新識別再協商選定速度。
失敗會停止工作並關閉COM；有界回復不保證斷線硬體能收到命令。
重連先在已驗證的bridge清理殘留CAN／I2C狀態才切速；不送target Flash命令。
若新會話序號與上一筆CONFIRM快取相撞，只有GET_HOST_INFO可換新序號再試
一次；其他NACK、SET、Flash命令不套用此規則。Reset核對原board ID。

| 選單名義baud | F24計算值（20MHz CKM0，整數截斷） | SDR00／01 |
| --- | --- | --- |
|115200 |114942 |AC00h |
|460800 |454545 |2A00h |
|921600 |909090 |1400h |

保留SC的fCLK40MHz／CKM0=fCLK/2，不改共享SAU時鐘或生成碼。後兩組分頻
誤差約-1.36%，仍須包含FTDI實際分頻、晶振、電壓、接線與中斷負載實測；
沒有以nominal相同就宣稱實體可用。UART RX/TX ring仍各256 bytes、可用255；
高速度增加滿載壓力，溢位有計數，不能視為全CAN bus持續滿載擷取器。

## 編譯時固定的 device UART policy

UPD1 的 `device_uart_baud` 位於 metadata112..115，為 LE32 的固定 device UART baud。
DEVICE UART profile 要求 role=DEVICE_APP、update mask 含 UART，baud 非0且非FFFFFFFF。
DEVICE policy3 的 version control 位於116..119；CAN device 的 UART baud 欄位為0。
HOST policy1 的112..127保持0。以上皆獨立於上方 Host Link 的三組 preset；
欄位格式集中於 [共用政策](COMMON_IMAGE_METADATA.md)。

未來target boot/app應共用該專案的編譯設定，例如uart_update_baud=921600，
boot用它初始化UART，app產表／封裝用同一設定產生metadata。可在封裝算CRC前
呼叫scripts/image_uart_policy.ps1的Set-DeviceUartImagePolicy；本輪沒有
假造一個已完成的UART target專案。Tool只解析／顯示／驗證，不能改寫已載入BIN。
未來worker必須先比對live boot、port與image_policy::AllowsDeviceUartBaud，
一致才可輸出device UART更新資料；切換PC Host Link不能解除此檢查。

BUR1原有metadata48..51的uart_baud是bridge自身BOOT固定更新速度；不是此
DEVICE UART欄位，也不是APP暫時協商速度。現行HOST policy仍version1且
device_uart_baud=0。固定baud不相同的boot/app不能靠Tool自動改成相同。

## 驗證邊界

本輪測試直接執行F24 UART C碼的ACK排空、分頻、confirm、逾時與60000ms
tick換回、RX溢位；PC測試正反路徑及SET／confirm不重送。這些不是FTDI／
MCU實體高速波形證明。實板仍須三組速度PING／大封包／完整CAN更新，確認
CRC、timeout、RX overrun與lost counters，並測Reset、Close、拔線及Misc ENTER。
0082保留F13 CAN timing、metadata staging與IRQ契約，Flash仍按4-byte word寫入；
完整 RAM 區塊通過 CRC 後，才進行 Flash 寫入。
