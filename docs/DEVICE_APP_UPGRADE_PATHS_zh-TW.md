# MCU 與介面工程配置索引

各組合保留配對工程、映像配置及 RAM 交接位址；Tool 操作、共用流程與實驗資料依連結查閱。
依型號與介面選擇章節，不需要先讀完其他 MCU。
工程行為與硬體驗證範圍分開描述；正常更新成功不代表冷啟動、斷電恢復或所有故障條件均通過。

| 型號與周邊 | 章節 |
| --- | --- |
| F13 / CAN | [工程設定與機制入口](#upgrade-f13-can) |
| F13 / UART | [工程設定與機制入口](#upgrade-f13-uart) |
| F13 / I2C | [工程設定與機制入口](#upgrade-f13-i2c) |
| F13 / Xmodem | [工程設定與機制入口](#upgrade-f13-xmodem) |
| F24 / CAN | [工程設定與機制入口](#upgrade-f24-can) |
| F24 / CAN FD | [工程設定與機制入口](#upgrade-f24-can-fd) |
| F24 / UART | [工程設定與機制入口](#upgrade-f24-uart) |
| F24 / I2C | [工程設定與機制入口](#upgrade-f24-i2c) |
| F24 / Xmodem | [工程設定與機制入口](#upgrade-f24-xmodem) |
| G16 / I2C | [工程設定與機制入口](#upgrade-g16-i2c) |
| RH850 / CAN | [工程設定與機制入口](#upgrade-rh850-can) |
| RH850 / CAN FD | [工程設定與機制入口](#upgrade-rh850-can-fd) |

<!-- DEVICE_UPGRADE_F13_CAN_BEGIN -->
<a id="upgrade-f13-can"></a>

### F13 × CAN：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R5F10BMG／`0xF13B` |
| Boot 工程 | `RL78_F13_Boot_loader_CAN` |
| App 工程 | `RL78_F13_Boot_loader_CAN_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F13_Boot_loader_CAN_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `119 KiB`；8 KiB Boot、App 最大 119 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | Classical 500K；standard control 0x700／reply 0x701，raw 0x720..0x727（各 8 bytes） |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF700` |

- [Tool CAN 操作](https://released.github.io/slide_simple_renesas_isp_tool/can-update.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/can-device.html#f13-boot-decision)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#device-update-timing)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F13_CAN_END -->

<!-- DEVICE_UPGRADE_F13_UART_BEGIN -->
<a id="upgrade-f13-uart"></a>

### F13 × UART：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R5F10BMG／`0xF13B` |
| Boot 工程 | `RL78_F13_ISP_Boot_loader_UART` |
| App 工程 | `RL78_F13_ISP_Boot_loader_UART_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F13_ISP_Boot_loader_UART_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `119 KiB`；8 KiB Boot、App 最大 119 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | UART framed ISP，115200／8-N-1 為範例值 |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool UART 操作](https://released.github.io/slide_simple_renesas_isp_tool/uart.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/uart-device.html#uart-boot-flow)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#f13-uart-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F13_UART_END -->

<!-- DEVICE_UPGRADE_F13_I2C_BEGIN -->
<a id="upgrade-f13-i2c"></a>

### F13 × I2C：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R5F10BMG／`0xF13B` |
| Boot 工程 | `RL78_F13_ISP_Boot_loader_I2C` |
| App 工程 | `RL78_F13_ISP_Boot_loader_I2C_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F13_ISP_Boot_loader_I2C_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `119 KiB`；8 KiB Boot、App 最大 119 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | I2C framed ISP；0x20／100 kHz 為範例值，Boot／App／BIN／Tool 必須一致 |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool I2C 操作](https://released.github.io/slide_simple_renesas_isp_tool/i2c.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/i2c-device.html#i2c-boot-flow)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#f13-i2c-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F13_I2C_END -->

<!-- DEVICE_UPGRADE_F13_XMODEM_BEGIN -->
<a id="upgrade-f13-xmodem"></a>

### F13 × Xmodem：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R5F10BMG／`0xF13B` |
| Boot 工程 | `RL78_F13_ISP_Boot_loader_Xmodem` |
| App 工程 | `RL78_F13_ISP_Boot_loader_Xmodem_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F13_ISP_Boot_loader_Xmodem_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `119 KiB`；8 KiB Boot、App 最大 119 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | XMODEM-CRC128 raw blocks，framed control 與 115200／8-N-1 |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool Xmodem 操作](https://released.github.io/slide_simple_renesas_isp_tool/xmodem.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/xmodem-device.html#xmodem-boot-flow)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#f13-xmodem-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F13_XMODEM_END -->

<!-- DEVICE_UPGRADE_F24_CAN_BEGIN -->
<a id="upgrade-f24-can"></a>

### F24 × CAN：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R7F124FPJ5xFB／`0xF24B` |
| Boot 工程 | `RL78_F24_Boot_loader_CAN` |
| App 工程 | `RL78_F24_Boot_loader_CAN_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F24_Boot_loader_CAN_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `247 KiB`；8 KiB Boot、App 最大 247 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | Classical 500K；standard control 0x700／reply 0x701，raw 0x720..0x727（各 8 bytes） |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool CAN 操作](https://released.github.io/slide_simple_renesas_isp_tool/can-update.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/can-device.html#f13-boot-decision)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#device-update-timing)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F24_CAN_END -->

<!-- DEVICE_UPGRADE_F24_CAN_FD_BEGIN -->
<a id="upgrade-f24-can-fd"></a>

### F24 × CAN FD：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R7F124FPJ5xFB／`0xF24B` |
| Boot 工程 | `RL78_F24_Boot_loader_CAN_FD` |
| App 工程 | `RL78_F24_Boot_loader_CAN_FD_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F24_Boot_loader_CAN_FD_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `247 KiB`；8 KiB Boot、App 最大 247 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | nominal 500K、data 2M、BRS；standard control 0x700／reply 0x701，raw 0x720（64 bytes） |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool CAN FD 操作](https://released.github.io/slide_simple_renesas_isp_tool/can-fd.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/can-fd-device.html#can-fd-device-engineering)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#device-update-timing)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F24_CAN_FD_END -->

<!-- DEVICE_UPGRADE_F24_UART_BEGIN -->
<a id="upgrade-f24-uart"></a>

### F24 × UART：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R7F124FPJ5xFB／`0xF24B` |
| Boot 工程 | `RL78_F24_ISP_Boot_loader_UART` |
| App 工程 | `RL78_F24_ISP_Boot_loader_UART_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F24_ISP_Boot_loader_UART_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `247 KiB`；8 KiB Boot、App 最大 247 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | UART framed ISP，115200／8-N-1 為範例值 |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool UART 操作](https://released.github.io/slide_simple_renesas_isp_tool/uart.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/uart-device.html#uart-boot-flow)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#f24-uart-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F24_UART_END -->

<!-- DEVICE_UPGRADE_F24_I2C_BEGIN -->
<a id="upgrade-f24-i2c"></a>

### F24 × I2C：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R7F124FPJ5xFB／`0xF24B` |
| Boot 工程 | `RL78_F24_ISP_Boot_loader_I2C` |
| App 工程 | `RL78_F24_ISP_Boot_loader_I2C_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F24_ISP_Boot_loader_I2C_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `247 KiB`；8 KiB Boot、App 最大 247 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | I2C framed ISP；0x20／100 kHz 為範例值，Boot／App／BIN／Tool 必須一致 |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool I2C 操作](https://released.github.io/slide_simple_renesas_isp_tool/i2c.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/i2c-device.html#i2c-boot-flow)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#f24-i2c-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F24_I2C_END -->

<!-- DEVICE_UPGRADE_F24_XMODEM_BEGIN -->
<a id="upgrade-f24-xmodem"></a>

### F24 × Xmodem：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R7F124FPJ5xFB／`0xF24B` |
| Boot 工程 | `RL78_F24_ISP_Boot_loader_Xmodem` |
| App 工程 | `RL78_F24_ISP_Boot_loader_Xmodem_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_F24_ISP_Boot_loader_Xmodem_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `247 KiB`；8 KiB Boot、App 最大 247 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | XMODEM-CRC128 raw blocks，framed control 與 115200／8-N-1 |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool Xmodem 操作](https://released.github.io/slide_simple_renesas_isp_tool/xmodem.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/xmodem-device.html#xmodem-boot-flow)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#f24-xmodem-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_F24_XMODEM_END -->

<!-- DEVICE_UPGRADE_G16_I2C_BEGIN -->
<a id="upgrade-g16-i2c"></a>

### G16 × I2C：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R5F121BCAFP／`0x016B` |
| Boot 工程 | `RL78_G16_ISP_Boot_loader_I2C` |
| App 工程 | `RL78_G16_ISP_Boot_loader_I2C_app` |
| CC-RL option bytes | User `EEF7F9`／OCD `85`；停用 WDT，G16 OCD 低 7 位固定為 `0000101` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_1FFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/RL78_G16_ISP_Boot_loader_I2C_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x02000`，容量 `23 KiB`；8 KiB Boot、App 最大 23 KiB；最後 1 KiB 為保留區，App 容量包含 vectors、Metadata、padding 與 CRC。 |
| 資料介面 | I2C framed ISP；0x20／100 kHz 為範例值，Boot／App／BIN／Tool 必須一致 |
| RAM 交接 | `0xFF710..0xFF71F`；返回 Boot request：`0xFF71C` |

- [Tool I2C 操作](https://released.github.io/slide_simple_renesas_isp_tool/i2c.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RL78/i2c-device.html#i2c-boot-flow)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RL78/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RL78/experiments.html#g16-i2c-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RL78/porting.html)。

<!-- DEVICE_UPGRADE_G16_I2C_END -->

<!-- DEVICE_UPGRADE_RH850_CAN_BEGIN -->
<a id="upgrade-rh850-can"></a>

### RH850 × CAN：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R7F701684／`0x8501` |
| Boot 工程 | `Sample_Project_RH850_S1_Boot_loader_CAN` |
| App 工程 | `Sample_Project_RH850_S1_Boot_loader_CAN_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_FFFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/Sample_Project_RH850_S1_Boot_loader_CAN_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x10000`，容量 `960 KiB`；64 KiB Boot、App 最大 960 KiB；Flash／debugger 保留配置以配對工程與燒錄器設定核對。 |
| 資料介面 | Classical 500K；standard control 0x700／reply 0x701，raw 0x720..0x727（各 8 bytes） |
| RAM 交接 | `0xFEDFFFF0..0xFEDFFFFF`；返回 Boot request：`signature／request／反碼／owner` |

- [Tool CAN 操作](https://released.github.io/slide_simple_renesas_isp_tool/can-update.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RH850/can-device.html#guide-can-device-4)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RH850/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RH850/experiments.html#rh850-can-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RH850/porting.html)。

<!-- DEVICE_UPGRADE_RH850_CAN_END -->

<!-- DEVICE_UPGRADE_RH850_CAN_FD_BEGIN -->
<a id="upgrade-rh850-can-fd"></a>

### RH850 × CAN FD：工程設定

#### 配對工程、接線與映像

| 項目 | 此組合的設定 |
| --- | --- |
| Device／target | R7F701684／`0x8501` |
| Boot 工程 | `Sample_Project_RH850_S1_Boot_loader_CAN_FD` |
| App 工程 | `Sample_Project_RH850_S1_Boot_loader_CAN_FD_app` |
| 初次安裝 | Boot 工程的 `DefaultBuild/boot0000_FFFF.hex`，由 programmer 安裝；確認 MCU 與擦除範圍 |
| Tool 更新檔 | App 工程的 `DefaultBuild/Sample_Project_RH850_S1_Boot_loader_CAN_FD_app_update.bin`；HEX、boot_app 合併檔與 Transfer HEX 不放入 App 更新欄 |
| Flash 配置 | App 起點 `0x10000`，容量 `960 KiB`；64 KiB Boot、App 最大 960 KiB；Flash／debugger 保留配置以配對工程與燒錄器設定核對。 |
| 資料介面 | nominal 500K、data 2M、BRS；standard control 0x700／reply 0x701，raw 0x720（64 bytes） |
| RAM 交接 | `0xFEDFFFF0..0xFEDFFFFF`；返回 Boot request：`signature／request／反碼／owner` |

- [Tool CAN FD 操作](https://released.github.io/slide_simple_renesas_isp_tool/can-fd.html)提供接線、Discover、Enter Boot、更新與結果確認。
- [Boot 接收與驗證機制](https://released.github.io/slide_boot_loader_for_RH850/can-fd-device.html#can-fd-device-engineering)集中描述此介面的接收、CRC、更新命令與完成回覆。
- [App 返回 Boot 與 RAM 交接](https://released.github.io/slide_boot_loader_for_RH850/architecture.html#boot-app-handoff)說明 ENTER 與 UART0 z/Z 的差異；實際保留位址以本節表格為準。
- [此組合的實驗資料與限制](https://released.github.io/slide_boot_loader_for_RH850/experiments.html#rh850-can-fd-update-measurements)與工程設定分開呈現，沒有實板紀錄的組合不推定為已驗證。
- 程式入口為配對工程的 `custom_func.c`：`hardware_init()`／`loop()`；移植修改位置見 [Boot／App 移植](https://released.github.io/slide_boot_loader_for_RH850/porting.html)。

<!-- DEVICE_UPGRADE_RH850_CAN_FD_END -->

