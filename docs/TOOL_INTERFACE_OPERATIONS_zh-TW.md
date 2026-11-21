# Tool 各介面操作

<!-- TOOL_I2C_BEGIN -->
<a id="i2c-operation"></a>

## I2C 操作

1. 依[共用接線表](serial-isp.html#serial-wiring)連接 SCL／SDA、共地及相容上拉，再完成 [Open + Probe Host](operation.html#tool-operation)。
2. 選 **I2C** tab、實接 **Bridge port**、**I2C speed** 與未左移的 **Slave (7-bit)**；值須符合已安裝的 Boot／App 及 BIN。範例預設為 100 kHz／`0x20`，並非固定限制。
3. 更新 App 選 **I2C ISP**，依[共用更新流程](serial-isp.html#serial-app-update)執行；測試獨立 Transfer 選 **Custom Transfer → Discover Test Target → Transfer Test**，不載入 BIN、不擦寫 Flash。

| 需要查閱的資料 | 直接入口 |
| --- | --- |
| MCU 工程、Boot／App 機制、實驗資料 | [RL78 I2C](../slide_boot_loader_for_RL78/i2c-device.html)／[RH850 I2C](../slide_boot_loader_for_RH850/i2c-device.html) |
| 位址／速率與映像要求 | [通訊設定與更新必要條件](serial-isp.html#serial-update-requirements) |
| NACK、timeout 或 App 未確認 | [錯誤與未知結果](status-and-reports.html#tool-error-guidance) |

G16 Reference 是另一個唯讀 workflow，不提供 ISP erase／write／jump；見[Reference 探索](serial-isp.html#4-i2c-master)。
<!-- TOOL_I2C_END -->

<!-- TOOL_UART_BEGIN -->
<a id="uart-operation"></a>

## UART 操作

1. 依[共用接線表](serial-isp.html#serial-wiring)交叉連接資料 TX／RX，診斷 UART0 另接 terminal，再完成 [Open + Probe Host](operation.html#tool-operation)。
2. 選 **UART / Xmodem** tab、實接 **Bridge port** 與 **Device baud**，Workflow 選 **UART ISP**。Host Baud 與 Device baud 分開設定；Device 使用 8-N-1，範例預設 115200。
3. 依[共用更新流程](serial-isp.html#serial-app-update)更新配對 App BIN；測試獨立 UART Transfer 時改選 **Custom Transfer → Discover Test Target → Transfer Test**，不擦寫 Flash。

| 需要查閱的資料 | 直接入口 |
| --- | --- |
| MCU 工程、Boot／App 機制、實驗資料 | [RL78 UART](../slide_boot_loader_for_RL78/uart-device.html)／[RH850 UART](../slide_boot_loader_for_RH850/uart-device.html) |
| 可選 baud 與映像條件 | [通訊設定與更新必要條件](serial-isp.html#serial-update-requirements) |
| 獨立 Log、RX／TX 診斷及 z/Z | [Device 串列診斷](serial-isp.html#serial-device-diagnostics) |
<!-- TOOL_UART_END -->

<!-- TOOL_XMODEM_BEGIN -->
<a id="xmodem-operation"></a>

## Xmodem 操作

1. 完成[資料 UART 接線](serial-isp.html#serial-wiring)與 [Open + Probe Host](operation.html#tool-operation)。Xmodem 使用同一個 **UART / Xmodem** tab，不另開 COM。
2. 設定 **Bridge port** 與 **Device baud**；更新 App 時 Workflow 選 **XMODEM-CRC**，使用配對 Xmodem App BIN，依[共用更新流程](serial-isp.html#serial-app-update)執行。
3. 測試獨立 Xmodem Transfer 時選 **Xmodem Transfer → Discover Test Target → Transfer Test**。Tool 執行 PING／ECHO／counters 與 CRC128 streaming，資料進入診斷 RAM sink，不擦寫 Flash。

| 需要查閱的資料 | 直接入口 |
| --- | --- |
| MCU 工程、Boot／App 機制、實驗資料 | [RL78 Xmodem](../slide_boot_loader_for_RL78/xmodem-device.html)／[RH850 Xmodem](../slide_boot_loader_for_RH850/xmodem-device.html) |
| CRC16、block、EOT 與 ACK | [Xmodem 協定格式](../slide_boot_loader_for_RL78/xmodem-device.html#xmodem-protocol) |
| 完成、取消與未知結果 | [共用報告與結果判讀](status-and-reports.html) |
<!-- TOOL_XMODEM_END -->
