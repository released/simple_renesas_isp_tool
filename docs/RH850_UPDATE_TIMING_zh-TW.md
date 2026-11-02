# RH850 CAN／CAN FD App 更新時間

<!-- RH850_TIMING_BEGIN -->
<a id="rh850-update-timing"></a>

<a id="guide-app-development-8"></a>

### App BIN 更新耗時參考

下表採用 Firmware update report 的實際紀錄，作為估算更新時間的參考。
映像大小使用 `input_bytes`；總耗時使用 `elapsed_ms`，包含 Metadata、START、傳送、END CRC、App 啟動確認與 Tool 流程開銷。
同大小及相同映像／通訊條件採首次完成 CRC／App 確認的紀錄；其餘測次保留在來源摘要。
App BIN 包含 entry、Metadata、IRQ table、程式、填充與 CRC；64 KiB Boot 不在更新 BIN 內，此 EVM 的 App 容量為960 KiB。
Classical CAN 與 CAN FD 依 Host Baud／bus 條件分組，各自保留量測日期、Tool 與來源報告。

Classical CAN 量測條件為 R7F701684 EVM、X1 16 MHz／CPU 120 MHz／CAN 40 MHz、500 kbit/s，Host Link 115200 baud。
Host firmware 1.8.1／build 20261023；Device Boot 1.0.0／build 20261022、App 1.0.0／build 20261021。
原始 App 為35,768 bytes；填充測試 BIN 為64／192／448／960 KiB。
五次均通過 END CRC、App 啟動／映像比對及 CAN_STOP；UART timer 持續遞增，重試與已記錄的 Host 錯誤差值均為 0。

<!-- RH850_TIMING_DATA_BEGIN -->
#### Classical CAN／Host 115200 baud

量測日期：2026-11-02；Tool：0.1.0.0114。
Host Baud：115200 bit/s；報告記錄的 bus 條件：Classical CAN 500K, standard data RX filter 0x00000701 / 0x000007FF。

| App BIN | bytes | 寫入相關階段 | App 確認 | 總耗時 | 報告 |
| --- | --- | --- | --- | --- | --- |
| 34.93 KiB | 35,768 | 19.3 s | 1.5 s | 21.3 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261102_104340_302_63508_432104234_17.html) |
| 64.00 KiB | 65,536 | 34.2 s | 1.5 s | 36.2 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261102_104440_865_63508_432164796_18.html) |
| 192.00 KiB | 196,608 | 100.5 s | 2.5 s | 103.5 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261102_104640_859_63508_432284781_19.html) |
| 448.00 KiB | 458,752 | 232.9 s | 4.6 s | 238.0 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261102_105059_529_63508_432543421_20.html) |
| 960.00 KiB | 983,040 | 499.0 s | 6.5 s | 506.1 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261102_105946_691_63508_433070515_21.html) |

##### Classical CAN／Host 115200 baud：分階段原始數值

| 來源報告 | Metadata | START | transfer | END CRC | App 確認 | elapsed_ms |
| --- | --- | --- | --- | --- | --- | --- |
| 20261102_104340_302_63508_432104234_17 | 1056 ms | 15 ms | 18168 ms | 48 ms | 1489 ms | 21291 ms |
| 20261102_104440_865_63508_432164796_18 | 1055 ms | 48 ms | 33046 ms | 63 ms | 1506 ms | 36234 ms |
| 20261102_104640_859_63508_432284781_19 | 1063 ms | 63 ms | 99187 ms | 160 ms | 2515 ms | 103513 ms |
| 20261102_105059_529_63508_432543421_20 | 1060 ms | 159 ms | 231390 ms | 336 ms | 4565 ms | 238036 ms |
| 20261102_105946_691_63508_433070515_21 | 1062 ms | 336 ms | 496944 ms | 688 ms | 6488 ms | 506058 ms |

#### CAN FD／Host 115200 baud

量測日期：2026-10-27；Tool：0.1.0.0109。
Host Baud：115200 bit/s；報告記錄的 bus 條件：CAN FD 500K/2M, standard data RX filter 0x00000701 / 0x000007FF。

| App BIN | bytes | 寫入相關階段 | App 確認 | 總耗時 | 報告 |
| --- | --- | --- | --- | --- | --- |
| 29.18 KiB | 29,876 | 16.2 s | 10.1 s | 26.8 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261027_070530_320_31112_4626953_1.html) |
| 64.00 KiB | 65,536 | 34.3 s | 9.8 s | 44.6 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261027_070726_000_31112_4742640_2.html) |
| 192.00 KiB | 196,608 | 100.4 s | 10.4 s | 111.4 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261027_070952_777_31112_4889406_3.html) |
| 448.00 KiB | 458,752 | 233.9 s | 10.3 s | 244.7 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261027_071505_750_31112_5202390_4.html) |
| 960.00 KiB | 983,040 | 499.5 s | 11.8 s | 511.8 s | [量測摘要](https://released.github.io/slide_boot_loader_for_RH850/data/update_20261027_072426_940_31112_5763562_5.html) |

##### CAN FD／Host 115200 baud：分階段原始數值

| 來源報告 | Metadata | START | transfer | END CRC | App 確認 | elapsed_ms |
| --- | --- | --- | --- | --- | --- | --- |
| 20261027_070530_320_31112_4626953_1 | 1055 ms | 15 ms | 15077 ms | 32 ms | 10099 ms | 26795 ms |
| 20261027_070726_000_31112_4742640_2 | 1055 ms | 32 ms | 33151 ms | 64 ms | 9761 ms | 44578 ms |
| 20261027_070952_777_31112_4889406_3 | 1067 ms | 64 ms | 99113 ms | 160 ms | 10441 ms | 111366 ms |
| 20261027_071505_750_31112_5202390_4 | 1059 ms | 160 ms | 232331 ms | 336 ms | 10305 ms | 244718 ms |
| 20261027_072426_940_31112_5763562_5 | 1086 ms | 336 ms | 497394 ms | 689 ms | 11751 ms | 511840 ms |

<!-- RH850_TIMING_DATA_END -->

寫入相關階段為 Metadata＋START＋transfer＋END CRC；總耗時另外包含 App 確認及未單獨分階段的 Tool 開銷。
Classical CAN 使用 500 kbit/s；CAN FD 使用 nominal 500 kbit/s／data 2 Mbit/s，對應範例的資料幀使用 BRS。
PC 到 MCU host 的 Host Baud 與 CAN bus 速率分別記錄，不將 data rate 當成實際有效寫入速率。
耗時供相同 Host Baud／CAN 設定下的量級參考。

容量與 Boot 分區見 [Flash mapping](https://released.github.io/slide_boot_loader_for_RH850/porting.html#rh850-flash-layout)，封包與 page 累積見 [格式參考](https://released.github.io/slide_boot_loader_for_RH850/reference.html#rh850-can-frames)，App IRQ／交接見 [App 開發](https://released.github.io/slide_boot_loader_for_RH850/app-development.html#rh850-app-editing)。
操作後的實際結果以 [Firmware update report](https://released.github.io/slide_simple_renesas_isp_tool/status-and-reports.html#update-confirmation) 為準。
<!-- RH850_TIMING_END -->
