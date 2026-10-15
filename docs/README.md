# 文件閱讀入口

`docs` 保留目前使用者與 firmware／Tool 開發者需要的指南及規格。
歷史驗證報告、一次性開發計畫與建置紀錄已移到 `tmp/internal`，不是使用前必讀文件。

## 操作與移植

| 需求 | 文件 |
| --- | --- |
| 第一版上傳檔案、配套版本、實測與SHA-256 | [2026-10-15交付說明](FIRST_DELIVERY_20261015_zh-TW.md) |
| 把自己的 RL78 專案整合成 Boot/App | [移植指南](RL78_BOOT_APP_PORTING_GUIDE_zh-TW.md)／[PDF](RL78_BOOT_APP_PORTING_GUIDE_zh-TW.pdf) |
| F13 Boot→App 條件與 CAN 更新 | [流程指南](F13_BOOT_UPDATE_FLOW_zh-TW.md)／[PDF](F13_CAN_Bootloader_Update_Guide_zh-TW.pdf) |
| 共用畫面、F24 分區、Misc 更新 Host App | [Bridge 指南](SHARED_CONSOLE_BRIDGE_GUIDE_zh-TW.md)／[PDF](FirmwareUpgradeTool_Shared_Console_Bridge_Guide_zh-TW.pdf) |
| CAN／I2C 模式與目前支援界線 | [多介面操作](MULTIBUS_TOOL_GUIDE_zh-TW.md)／[PDF](FirmwareUpgradeTool_MultiBus_Guide_zh-TW.pdf) |
| UI 各欄資訊來源、live 與離線 BIN | [資訊卡指南](UI_IDENTITY_GUIDE_zh-TW.md)／[PDF](FirmwareUpgradeTool_UI_Identity_Guide_zh-TW.pdf) |
| PC↔Host 高速／接收延遲、F13速度比較與 Device UART 固定 baud | [Baud 與效能分工](HOST_LINK_BAUD_zh-TW.md) |
| CAN更新加速、v5區塊／重送／配套安裝 | [64-byte區塊指南](CAN_BLOCK_TRANSFER_V5_zh-TW.md) |
| Log／錯誤排查 | [Diagnostics](DIAGNOSTICS.md) |

## Firmware／Tool 整合規格

- [共用 Metadata／更新介面防呆](COMMON_IMAGE_METADATA.md)
- [PC↔Host Link v2](HOST_LINK_PROTOCOL_V2.md)
- [Host UART 自更新協定](BRIDGE_SELF_UPDATE_PROTOCOL.md)
- [F13 CAN 更新 Profile](F13_BOOT_PROFILE.md)
- [Custom Transfer 通訊測試協定](F13_TRANSFER_PROTOCOL.md)
- [F13 映像／CRC post-build](F13_CRC_POST_BUILD.md)
- [F24 硬體／Smart Configurator 注意事項](F24_BRINGUP.md)
- [架構與責任分工](ARCHITECTURE_V2.md)
- [多介面架構](MULTIBUS_BRIDGE_DESIGN.md)
- [新增 Bridge backend](HOST_BACKEND_GUIDE.md)
- [新增 Target profile](TARGET_PROFILE_GUIDE.md)

歷史實驗結果不能當成最新版規格。軟體測試與實板驗證狀態仍須分開閱讀；
指南不代表已完成硬體驗收，也不代表網站已發布。
