# 串列介面實驗資料

各表保留原始量測條件、映像身分與確認範圍。Tool 操作另見介面操作頁。

<a id="experiment-i2c"></a>

## I2C

<!-- F13_I2C_MEASURED_BEGIN -->
<a id="f13-i2c-update-measurements"></a>

#### F13 I2C 更新量測與範圍

條件為 R5F10BMG、F24 bridge、I2C 100 kHz／7-bit 0x20、Host Link 115200；Tool 0.1.0.138、Host FW 1.8.1／build 20261023。時間單位為秒。

| App BIN | bytes | WRITE | END CRC | App 確認 | 總耗時 | 結果與資料 |
| --- | --- | --- | --- | --- | --- | --- |
| 原始 App | 12316 | 21.071 | 0.188 | 1.990 | 24.127 | [PASS／摘要](https://released.github.io/slide_boot_loader_for_RL78/data/f13_i2c_12316b.html) |
| 24 KiB | 24576 | 42.026 | 0.324 | 2.007 | 45.238 | [PASS／摘要](https://released.github.io/slide_boot_loader_for_RL78/data/f13_i2c_24576b.html) |
| 56 KiB | 57344 | 98.240 | 0.650 | 2.150 | 101.918 | [PASS／摘要](https://released.github.io/slide_boot_loader_for_RL78/data/f13_i2c_57344b.html) |
| 119 KiB | 121856 | 208.912 | 1.183 | 3.385 | 214.357 | [PASS／摘要](https://released.github.io/slide_boot_loader_for_RL78/data/f13_i2c_121856b.html) |

- 四份映像均取得 START／END 成功回覆、APP_READY，並核對全部 256-byte installed metadata；UART0 有對應的 boot-validated 啟動與後續 counter。
- 119 KiB 在重啟後唯讀 App 確認期間有四次 I2C_NACK，之後確認成功；Host Link retries=0 不能解讀成每次 I2C 查詢都一次成功。
- Host counter assessment 為 UNAVAILABLE，與更新 PASS 分開判斷；總耗時來自本次 report elapsed_ms，不採載入下一個 BIN 時仍顯示的前次 Result／Elapsed。
- 結果限上述四份映像的一般更新與重啟後確認；冷啟動、斷電／故障恢復、其他速率或 MCU 另行確認。
<!-- F13_I2C_MEASURED_END -->

<!-- F24_I2C_MEASURED_BEGIN -->
<a id="f24-i2c-update-measurements"></a>

#### F24 I2C App 更新量測

量測條件：F24 device R7F124FPJ5xFB，F24 bridge，I2C 100 kHz／7-bit 0x20，Host Link 115200。
Tool 0.1.0.138；Host FW 1.8.1／build 20261023。表格由六份選定更新報告的實際欄位整理，時間單位為秒。

| App BIN | bytes | WRITE | END CRC | App 確認 | 總耗時 | 結果與資料 |
| --- | --- | --- | --- | --- | --- | --- |
| 原始 App | 6960 | 12.566 | 0.203 | 1.990 | 15.666 | [PASS／摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_i2c_6960b.html) |
| 8 KiB | 8192 | 14.711 | 0.186 | 2.074 | 17.890 | [PASS／摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_i2c_8192b.html) |
| 24 KiB | 24576 | 44.282 | 0.325 | 1.994 | 47.493 | [PASS／摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_i2c_24576b.html) |
| 56 KiB | 57344 | 99.547 | 0.541 | 2.039 | 103.055 | [PASS／摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_i2c_57344b.html) |
| 120 KiB | 122880 | 215.953 | 0.998 | 2.806 | 220.635 | [PASS／摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_i2c_122880b.html) |
| 247 KiB | 252928 | 439.099 | 1.850 | 4.536 | 446.359 | [PASS／摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_i2c_252928b.html) |

- 六份映像均取得 START／END 成功回覆、APP_READY，並核對全部 256-byte installed metadata；獨立 UART0 Log 有對應的 boot-validated 啟動文字及後續 counter。
- 總耗時採報告 elapsed_ms，包含 Metadata、START、WRITE、END CRC、App 確認及流程開銷；不採載入 BIN 畫面保留的前次 Result／Elapsed。
- 120／247 KiB 在重啟後唯讀確認期間分別觀察到 2／7 次 I2C_NACK，之後均確認 App。報告 link_retries 為 0 不代表所有 I2C 查詢一次成功。
- Host counter assessment 為 UNAVAILABLE，無法據此宣稱 bus 無錯誤；這與映像／App 確認成功分開判斷。
- 最大測試 App 為 247 KiB，未進入尾端 1 KiB 保留區。結果限此映像與接線條件；冷啟動、斷電恢復、故障注入、其他 MCU 或速度另行驗證。
<!-- F24_I2C_MEASURED_END -->

<!-- G16_I2C_MEASURED_BEGIN -->
<a id="g16-i2c-update-measurements"></a>

#### G16 I2C App 更新量測

配對接線、操作步驟與 Boot／App 流程見 [G16 × I2C](https://released.github.io/slide_boot_loader_for_RL78/i2c-device.html#upgrade-g16-i2c)。

- Device：R5F121BCAFP，HOCO／CPU 16 MHz；F24 bridge；I2C 100 kHz／7-bit 0x20；Host Link 115200。Tool 0.1.0.139，Host FW 1.8.1 build 20261023。
- 兩份報告與對應 BIN 的長度、CRC32 相符，映像及來源報告由 SHA256 識別。START／END 成功、完整接受 bytes、APP_READY 與全部 256-byte installed metadata 均已確認。
- 獨立 UART0 擷取有兩次 boot-validated／update completed 與後續 counter；唯讀 metadata 回覆與兩份 BIN 的 256 bytes 分別相符。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 原始 App | 12116 | 20.736 | 0.356 | 1.992 | 23.959 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_i2c_12116b.html) |
| 23 KiB | 23552 | 40.147 | 0.539 | 2.039 | 43.606 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_i2c_23552b.html) |

- 最大完整 App BIN 為 23 KiB，範圍至 `0x07BFF`；Boot 8 KiB 與尾端 `0x07C00..0x07FFF` 保留區不在 App 更新範圍。
- 第二次更新從執行中的 App 經 ENTER 返回 Boot。UART0 擷取有 ENTER RX，沒有對應 TX trace；Tool 的 Boot 確認與後續更新成功分開記錄，不以此推定回覆遺失原因。
- Host counter assessment 為 **UNAVAILABLE**，Host Link retry 為 0；本段未觀察到 App 確認 I2C_NACK，仍不能推定所有 bus 計數全程零錯誤。
- 獨立 Transfer 的本批擷取涵蓋 Discover／INFO、一組 RX／TX、每列 8 bytes，沒有 Flash 命令；PING／ECHO／counters 尚未由此擷取證實。[Discover 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_i2c_transfer.html)
- 結果限正常更新後 App 確認。UART0 z/Z 人工重置、冷啟動、斷電恢復、其他介面／型號與故障注入另行驗證。
<!-- G16_I2C_MEASURED_END -->

<a id="experiment-uart"></a>

## UART

<!-- F13_UART_MEASURED_BEGIN -->
<a id="f13-uart-update-measurements"></a>

#### F13 UART ISP App 更新量測

- Device：R5F10BMG；F24 bridge；Device UART1 與 Host Link 各為 115200／8-N-1。Tool 0.1.0.139，Host FW 1.8.1 build 20261023。
- 四份報告與實際 BIN 的大小、CRC32、SHA256 相符。START、END／EOT、完整接受 bytes、APP_READY 及全部 256-byte installed metadata 均已確認。
- UART0 有四次 boot-validated／update completed、後續 counter 與完整 framed RX／TX；payload 每列 8 bytes。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 原始 App | 10556 | 15.750 | 0.176 | 1.944 | 18.761 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_uart_10556b.html) |
| 24 KiB | 24576 | 36.784 | 0.303 | 1.960 | 39.940 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_uart_24576b.html) |
| 56 KiB | 57344 | 85.957 | 0.576 | 2.024 | 89.445 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_uart_57344b.html) |
| 119 KiB | 121856 | 182.768 | 1.136 | 3.209 | 188.001 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_uart_121856b.html) |

- 更新後 Device UART 快照的 RX errors／drops／TX timeouts 為 0；Bridge UART1 deltas 的 RX errors／drops 為 0，每次記有 backend aborts=1。abort 計數與 Device UART 錯誤為不同觀察項。
- Host counter assessment 為 **UNAVAILABLE**，Host Link retry 為 0；不能由此推定所有 Host 計數全程零錯誤。
- 獨立 Transfer 的 PING／ECHO／counters 通過，沒有 Flash 命令。8 組 UART0 RX／TX 的 sequence、command、payload 與八位元組分行相符。[Transfer 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_uart_transfer.html)
- Transfer 成功段前後的累積 Device RX errors 均為 **1159**，drops／TX timeouts 為 0；不能與更新後 App 的 0 跨執行相減。較早曾有 Host command 0x61 INTERNAL_ERROR；其原因未由本批資料確立。
- 本批涵蓋正常更新後 App 確認；z/Z 人工重置、冷啟動、斷電及其他型號的結果另行驗證。
<!-- F13_UART_MEASURED_END -->

<!-- F24_UART_MEASURED_BEGIN -->
<a id="f24-uart-update-measurements"></a>

#### F24 UART App 更新量測

- Device：R7F124FPJ5xFB；F24 bridge；UART1 115200／8-N-1，Host Link 115200。
- Tool 0.1.0.139；Host FW 1.8.1 build 20261023。六份獨立報告與實際 BIN 的 size／CRC32 相符。
- 六次 START、END 均已確認，接受 bytes 等於映像大小；APP_READY 與全部 256-byte installed metadata 相符，UART0 有 boot-validated、update completed 及後續 counter。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 原始 App | 6724 | 10.573 | 0.128 | 1.930 | 13.615 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_uart_6724b.html) |
| 8 KiB | 8192 | 12.208 | 0.144 | 1.946 | 15.187 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_uart_8192b.html) |
| 24 KiB | 24576 | 36.835 | 0.256 | 1.961 | 39.942 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_uart_24576b.html) |
| 56 KiB | 57344 | 86.038 | 0.480 | 1.962 | 89.369 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_uart_57344b.html) |
| 120 KiB | 122880 | 184.479 | 0.945 | 2.635 | 188.949 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_uart_122880b.html) |
| 247 KiB | 252928 | 380.445 | 1.842 | 9.557 | 392.734 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_uart_252928b.html) |

- Device UART 的 RX errors、RX drops、TX timeouts 均為 0；這些欄位是 Device 當次執行快照。
- Host counter assessment 為 **UNAVAILABLE**，不能據此推定 Host 全程零錯誤；Host Link retry 為 0。
- 247 KiB 的唯讀 App 確認有 37 次 Host BUSY 觀察，最終確認成功；沒有重送 Flash START／END。
- UART Custom Transfer 及獨立 Xmodem Transfer 均有通過紀錄；Xmodem Boot／App 的更新與 UART0 trace 證據集中於 [F24 Xmodem 更新量測](https://released.github.io/slide_simple_renesas_isp_tool/serial-isp.html#f24-xmodem-update-measurements)。
- 本表涵蓋一般更新與軟體重啟後 App 確認；冷啟動、斷電及其他故障恢復不由本批資料推定。
<!-- F24_UART_MEASURED_END -->

<!-- G16_UART_MEASURED_BEGIN -->
<a id="g16-uart-update-measurements"></a>

#### G16 UART ISP App 更新量測

配對工程、接線與 Boot／App 機制見 [G16 × UART ISP](uart-device.html#upgrade-g16-uart)。

- Device：R5F121BCAFP，HOCO／CPU 16 MHz；F24 bridge；Device UART1 與 Host Link 各為 115200／8-N-1。Tool 0.1.0.146，Host FW 1.8.1 build 20261023。
- 兩份報告與實際 BIN 的長度、CRC32 相符，來源報告及映像由 SHA256 識別。START、END／EOT、完整接受 bytes、APP_READY 與全部 256-byte installed metadata 均已確認。
- UART0 擷取各有兩次 boot-validated／update completed、後續 counter 與完整 RX／TX。payload 每列最多 8 bytes；四頁 metadata 組合分別與兩份 BIN 的 256 bytes 相同。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 原始 App | 10240 | 15.271 | 0.256 | 1.944 | 18.359 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_uart_10240b.html) |
| 23 KiB | 23552 | 35.247 | 0.496 | 1.945 | 38.577 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_uart_23552b.html) |

- 最大完整 App BIN 為 23 KiB，範圍至 `0x07BFF`；Boot 8 KiB 與 `0x07C00..0x07FFF` 保留區不在 App 更新範圍。第二份映像從執行中的 App 經 ENTER 返回 Boot 更新。
- 更新後 Device UART 快照的 RX errors／drops／TX timeouts 為 0；Bridge RX errors／drops 為 0，backend aborts=1 仍保留。Host counter assessment 為 **UNAVAILABLE**、Host Link retry 為 0，不推定全程零錯誤。
- 獨立 UART Transfer 的 PING／ECHO／counters 與 final INFO 三次通過，沒有 Flash 命令。[Transfer 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_uart_transfer.html)
- Transfer 成功段的累積 Device RX errors 保持 **119**，drops／TX timeouts 為 0；此批未提供 Transfer UART0 擷取，不能與 App 的 0 跨執行相減。
- 結果限正常更新後 App 確認；z/Z 人工重置、冷啟動、斷電恢復及其他介面／型號另行驗證。
<!-- G16_UART_MEASURED_END -->

<a id="experiment-xmodem"></a>

## Xmodem

<!-- F13_XMODEM_MEASURED_BEGIN -->
<a id="f13-xmodem-update-measurements"></a>

#### F13 Xmodem App 更新量測

- Device：R5F10BMG；F24 bridge；Device UART1 與 Host Link 各為 115200／8-N-1。Tool 0.1.0.139，Host FW 1.8.1 build 20261023。
- 四份報告與實際 BIN 的大小、CRC32、SHA256 相符。START、END／EOT、完整接受 bytes、APP_READY 及全部 256-byte installed metadata 均已確認。
- UART0 有四次 boot-validated／update completed、後續 counter 與完整 framed RX／TX；payload 每列 8 bytes。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 原始 App | 12636 | 11.108 | 0.192 | 1.944 | 15.143 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_xmodem_12636b.html) |
| 24 KiB | 24576 | 21.721 | 0.287 | 1.958 | 25.862 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_xmodem_24576b.html) |
| 56 KiB | 57344 | 51.094 | 0.576 | 2.022 | 55.588 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_xmodem_57344b.html) |
| 119 KiB | 121856 | 108.767 | 1.136 | 3.208 | 115.010 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_xmodem_121856b.html) |

- 更新後 Device UART 快照的 RX errors／drops／TX timeouts 為 0；Bridge UART1 deltas 的 RX errors／drops 為 0，每次記有 backend aborts=1。abort 計數與 Device UART 錯誤為不同觀察項。
- Host counter assessment 為 **UNAVAILABLE**，Host Link retry 為 0；不能由此推定所有 Host 計數全程零錯誤。
- 獨立 Transfer 的 PING／ECHO／counters 及 CRC128 streaming 通過，沒有 Flash 命令。11 組 UART0 RX／TX 的 sequence、command、payload 與八位元組分行相符。[Transfer 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f13_xmodem_transfer.html)
- Transfer 成功段前後的累積 Device RX errors 均為 **658**，drops／TX timeouts 為 0；不能與更新後 App 的 0 跨執行相減。
- 本批涵蓋正常更新後 App 確認；z/Z 人工重置、冷啟動、斷電及其他型號的結果另行驗證。
<!-- F13_XMODEM_MEASURED_END -->

<!-- F24_XMODEM_MEASURED_BEGIN -->
<a id="f24-xmodem-update-measurements"></a>

#### F24 Xmodem App 更新量測

- Device：R7F124FPJ5xFB；F24 bridge；UART1 115200／8-N-1，Host Link 115200。
- Tool 0.1.0.139；Host FW 1.8.1 build 20261023。六份獨立報告與實際 BIN 的 size／CRC32 相符。
- 六次 START、END 均已確認，接受 bytes 等於映像大小；APP_READY 與全部 256-byte installed metadata 相符，UART0 有 boot-validated、update completed 及後續 counter。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 原始 App | 7856 | 6.883 | 0.127 | 1.944 | 10.851 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_xmodem_7856b.html) |
| 8 KiB | 8192 | 7.155 | 0.127 | 1.956 | 11.138 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_xmodem_8192b.html) |
| 24 KiB | 24576 | 21.464 | 0.240 | 1.943 | 25.544 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_xmodem_24576b.html) |
| 56 KiB | 57344 | 50.549 | 0.480 | 1.958 | 54.884 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_xmodem_57344b.html) |
| 120 KiB | 122880 | 108.125 | 0.928 | 2.648 | 113.598 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_xmodem_122880b.html) |
| 247 KiB | 252928 | 222.702 | 1.841 | 9.531 | 235.969 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_xmodem_252928b.html) |

- Device UART 的 RX errors、RX drops、TX timeouts 均為 0；這些欄位是 Device 當次執行快照。
- Host counter assessment 為 **UNAVAILABLE**，不能據此推定 Host 全程零錯誤；Host Link retry 為 0。
- 247 KiB 的唯讀 App 確認有 37 次 Host BUSY 觀察，最終確認成功；沒有重送 Flash START／END。
- 獨立 Xmodem Transfer 通過 PING、96-byte ECHO、389-byte CRC32 sink／CRC128 串流與 counters；UART0 的 9 組 framed RX／TX、sequence／command、完整 payload 及每列 8 bytes 可互相核對。[Transfer 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/f24_xmodem_transfer.html)
- Transfer 成功段前後的 Device UART RX errors 均為 **44**，RX drops／TX timeouts 為 0；這是同一次執行的累積快照，不能解讀成全程零錯誤，也不能與更新後 App 的 0 跨執行相減。
- 原始連線階段曾有 Host command 0x61 TIMEOUT 及讀取中斷 995；重新 Open + Probe 後才取得上述 Transfer 成功紀錄。錯誤原因沒有由本批資料確立。
- 六次 App 重啟均有 boot-validated、update completed、UART0 console 與後續 counter；提供的 log 沒有 z/Z 人工重置訊息，故不將功能宣告視為該操作的實板證據。
- 本表涵蓋一般更新與軟體重啟後 App 確認；冷啟動、斷電及其他故障恢復不由本批資料推定。
<!-- F24_XMODEM_MEASURED_END -->

<!-- G16_XMODEM_MEASURED_BEGIN -->
<a id="g16-xmodem-update-measurements"></a>

#### G16 Xmodem App 更新量測

配對工程、接線與 Boot／App 機制見 [G16 × Xmodem](xmodem-device.html#upgrade-g16-xmodem)。

- Device：R5F121BCAFP，HOCO／CPU 16 MHz；F24 bridge；Device UART1 與 Host Link 各為 115200／8-N-1。Tool 0.1.0.146，Host FW 1.8.1 build 20261023。
- 三份報告與實際 BIN 的長度、CRC32 相符，來源報告及映像由 SHA256 識別。START、END／EOT、完整接受 bytes、APP_READY 與全部 256-byte installed metadata 均已確認。
- UART0 擷取各有兩次 boot-validated／update completed、後續 counter 與完整 RX／TX。payload 每列最多 8 bytes；四頁 metadata 組合分別與兩份 BIN 的 256 bytes 相同。

| 映像 | Bytes | Write 秒 | END CRC 秒 | App 確認秒 | 總秒數 | 報告 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 原始 App | 12304 | 10.773 | 0.287 | 1.945 | 14.917 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_xmodem_12304b.html) |
| 23 KiB | 23552 | 20.616 | 0.480 | 1.944 | 24.952 | [PASS 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_xmodem_23552b.html) |

- 原始 App 另有一次 14.949 秒的 PASS 報告；表列相同映像的另一次 14.917 秒紀錄。兩次均確認 APP_READY／installed metadata。
- 最大完整 App BIN 為 23 KiB，範圍至 `0x07BFF`；Boot 8 KiB 與 `0x07C00..0x07FFF` 保留區不在 App 更新範圍。第二份映像從執行中的 App 經 ENTER 返回 Boot 更新。
- 更新後 Device UART 快照的 RX errors／drops／TX timeouts 為 0；Bridge RX errors／drops 為 0，backend aborts=1 仍保留。Host counter assessment 為 **UNAVAILABLE**、Host Link retry 為 0，不推定全程零錯誤。
- 獨立 Xmodem Transfer 的三次 Discover／INFO 通過；此批 log 未包含 Transfer Test／CRC128 streaming，因此不列為該項已驗證。[Transfer 摘要](https://released.github.io/slide_simple_renesas_isp_tool/data/g16_xmodem_transfer.html)
- Transfer 成功段的累積 Device RX errors 保持 **104**，drops／TX timeouts 為 0；此批未提供 Transfer UART0 擷取，不能與 App 的 0 跨執行相減。
- 結果限正常更新後 App 確認；z/Z 人工重置、冷啟動、斷電恢復及其他介面／型號另行驗證。
<!-- G16_XMODEM_MEASURED_END -->
