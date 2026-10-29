<a id="guide-can-device-1"></a>

### 工程與用途

| 模式／MCU | Transfer | Boot | App |
| --- | --- | --- | --- |
| Classical CAN／F13 | `RL78_F13_CAN_Transfer` | `RL78_F13_Boot_loader_CAN` | `RL78_F13_Boot_loader_CAN_app` |
| Classical CAN／F24 | `RL78_F24_CAN_Transfer` | `RL78_F24_Boot_loader_CAN` | `RL78_F24_Boot_loader_CAN_app` |
| CAN FD／F24 | `RL78_F24_CAN_FD_Transfer` | `RL78_F24_Boot_loader_CAN_FD` | `RL78_F24_Boot_loader_CAN_FD_app` |

| MCU／模式 | Boot reserve | 預設完整App容量 | 編譯的bus條件 |
| --- | --- | --- | --- |
| F13 Classical CAN | 8KiB | 119KiB | nominal500K |
| F24 Classical CAN | 8KiB | 247KiB | nominal500K |
| F24 CAN FD | 8KiB | 247KiB | nominal500K／data2M、BRS |

Boot、App與獨立Transfer各自建置。下載／產物選擇集中於 [下載入口](https://released.github.io/slide_simple_renesas_isp_tool/downloads.html)。
板端設定與啟動分支依本頁的F13／F24工程說明；完整地址與尾端保護集中於 [移植與Flash](porting.html)。
