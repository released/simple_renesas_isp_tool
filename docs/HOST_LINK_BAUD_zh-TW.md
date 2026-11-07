# Host Link 與 device UART 速度設定

## 兩段 UART，不共用速度來源

上方 COM／Baud 是 PC - FT234XD - bridge 的 Host Link；上電固定115200，
Tool可選115200／460800／921600，完成身分驗證後才切速。這不是 device
更新周邊速度；CAN、I2C與device UART各自獨立。
UART / Xmodem TAB的Device baud可選擇或輸入；I2C使用所選7-bit位址及port宣告的100／400kHz。
Boot／App初始化、BIN metadata及live INFO的對應條件集中於[串列更新必要條件](https://released.github.io/slide_simple_renesas_isp_tool/#serial-update-requirements)。
BIN／metadata／log仍共用，不建立另一個PC COM；Misc維持host App更新。
操作時先Close COM、選Host Baud，再Open + Probe；連線期間下拉選單鎖定。
成功後Host狀態列／log顯示實際會話名義速度，不只顯示選單偏好。

F24 App以feature bit11表示切速能力。05查詢、06切速、07確認，詳見Host Link protocol。
沒有bit11的舊APP不能使用高速；BOOT無bit11，按live descriptor的固定更新
速度連線。現行BOOT／BUR1固定115200。高速APP進Misc ENTER時ACK傳完後reset，
PC改用該BOOT速度重新驗證，不把APP的高速帶進擦寫。

## 接收與交易時序

- PC 使用 first-byte 讀取：已有資料立即返回，分段封包由 parser 重組並驗 CRC。
- 無資料等待上限為剩餘期限與 50 ms 的較小值；不是每包固定延遲。
- Host Link framing、payload 上限與 Device Flash 安全檢查各自保持獨立。
- CAN 區塊更新與量測條件見 [CAN 區塊傳輸](CAN_BLOCK_TRANSFER_V5_zh-TW.md)及[更新結果](UPDATE_VALIDATION_zh-TW.md)。
- Windows timeout 語意見 [COMMTIMEOUTS](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts)。

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

Target Boot／App共用該工程的isp_transport_config.h，周邊初始化與產表／封裝
使用同一編譯契約。Tool解析BIN後，比對所選Device baud、live INFO及metadata；
值相符才允許更新，不修改已載入BIN，也不以切換Host Baud代替device設定。
修改位置與核對表見[串列更新必要條件](https://released.github.io/slide_simple_renesas_isp_tool/#serial-update-requirements)。

BUR1原有metadata48..51的uart_baud是bridge自身BOOT固定更新速度；不是此
DEVICE UART欄位，也不是APP暫時協商速度。現行HOST policy仍version1且
device_uart_baud=0。固定baud不相同的boot/app不能靠Tool自動改成相同。

## 硬體條件

- 名義 baud 相同不代表實體鏈路符合誤差預算；USB-UART 分頻、晶振、電壓、接線與中斷負載共同影響結果。
- 各速度的 CRC、timeout、RX overrun 及 lost counters 用於判讀實際鏈路；軟體模型不代表實體高速波形。
- Reset、Close、拔線及 Misc ENTER 各有獨立的協商／恢復路徑。
- Device Flash 寫入仍須等待完整 RAM 區塊通過 CRC；提高 Host Link baud 不略過此保護。
