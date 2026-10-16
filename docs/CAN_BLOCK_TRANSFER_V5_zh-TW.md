# CAN Upgrade v5：64-byte 區塊更新

2026-10-15｜Tool 0.1.0.0082／F24 App 1.5.0／F13 Boot、App 1.5.0

目標是減少逐筆 PC 往返，不是只把 CAN bitrate 調高。本版不相容舊 v4 DATA；
F13 初次需重燒 matching Boot/App，之後相容 App 可正常更新。F24 Boot 1.2.0
不變，可經 Misc／115200 更新 Bridge App 1.5.0。不要拿 F13 BIN 更新 host。

## 1. 基準與改善範圍

| 項目 | 舊 v4 | 新 v5 |
| --- | --- | --- |
| 一次 PC 交易的有效 image 資料 | 4 bytes | 最多64 bytes |
| DATA CAN frame payload | 4-byte header＋4-byte image | 8-byte image；索引放 CAN ID |
| 確認點 | 每4 bytes | 每個區塊寫入完成 |
| 19224-byte image | 4806次 DATA 交易 | 301次區塊交易 |
| image format／metadata／App start | 6／256 bytes／0x2000 | 不變 |
| IRQ ABI／Boot 分區 | F1V1／8 KiB | 不變 |

使用者171150 log：0080／Host115200／CAN500K，19224B、CRC1DE10DCD，
DATA約77秒、完整更新約79秒；先前0079約142／145秒。這是舊 v4 的實板證據，
不是新版 v5 的速度。171150 trace行數少於4806，UI佇列可能遺失log，不能用行數
推算實際收發量。0082會顯示佇列溢位警告；不完整log不可當完整封包紀錄。

301是交易數，不是預測加速16倍。UART、USB driver、Flash及UI仍影響實際耗時。

第一版實測：使用者182253 log，0082／F24 App1.5.0／F13 Boot1.5.0，COM6／
Host115200／CAN500K；19244B、CRC CA0DF55C，301個連續COMMIT ACK，完整10963ms／
1755B/s，END CRC相符。無UART timeout/retry、UI overflow警告或CAN lost/error。
較舊v4約79秒快約7.2倍，但映像19224／19244B不同，且只有單次結果，不是嚴格A/B。
reset後新版App執行仍待UART／重新INFO確認；[交付清單與限制](FIRST_DELIVERY_20261015_zh-TW.md)。

## 2. 泛用 Bridge 指令

Host Link framing仍v2、payload128、frame142；新增 feature bit12（0x1000）與
CAN_BURST_WAIT_RX（0x22）。Tool在UI及worker要求此能力，缺少就封鎖更新。
F24不解析F13 command、CRC、位址或Flash geometry。

12-byte header：version1、frame count1..10、LE16 timeout1..30000ms、LE16
reply ID、LE16非零reply mask、reply DLC0..8、reserved0、LE16 gap0..10ms。
後接 count 筆 `[ID16, DLC8, data[DLC]]`；只接受標準Classical data frames。
先驗證整包、所有ID／DLC／長度及結尾，失敗時零TX；10筆滿長frame佔122 bytes。

F24逐筆等待實際TX完成，再送下一筆；gap為非阻塞main-loop排程，預設0。
只在最後一筆開始傳送後接收匹配回覆，包含早於最後TX-complete的快速ACK。
成功回覆既有CAN record。TX／RX等待與abort皆有界，abort停滯最多100ms後stop。
每筆TX與最後RX各有timeout；PC總期限包含所有frame。Bridge不自動重送整個burst。
相同UART sequence／內容沿用既有active去重與完成回覆快取，避免重複TX。

## 3. F13 區塊 wire contract

控制／回覆仍為標準ID700／701。INFO page1A回傳LE32
`(data_base_id << 16) | block_bytes`，此板為0x07200040。
Tool只接受64-byte、對齊8個標準ID且不重疊control／response的live宣告。

| frame | ID／DLC | Data |
| --- | --- | --- |
| BLOCK_BEGIN 0B | 700／8 | cmd、offset24、block CRC32 |
| 第0..7段 | 720..727／8 | 完整8-byte image；最後不足8時DLC4 |
| BLOCK_COMMIT 0C | 700／8 | 同一offset24與block CRC32 |
| COMMIT ACK | 701／8 | 0C、status、LE16 offset/64、LE32下一個絕對位址 |

BEGIN與raw段不逐筆回覆；COMMIT才回一筆ACK。raw ID的低3 bits是段索引，
可偵測缺段，支援相同段重送及段內重排；衝突重送會使該區塊失效。
區塊起點須64-byte aligned，長度為min(64, START_size-offset)，最後仍4-byte aligned。
CRC32涵蓋本區塊實際長度，沒有將未使用尾段padding寫入Flash。

先收齊RAM、核對BEGIN／COMMIT header及block CRC，metadata重疊區還須逐字
符合先前封存的metadata，之後才允許erase／write。FSL仍按4-byte word操作；
64-byte傳輸單位不是FSL寫入單位，也不是metadata wire chunk大小。

只允許下一區塊，或即時前一個已提交區塊的重播。重播先核對已寫Flash CRC，
ACK不再erase／write。Tool對Host回報TIMEOUT或target BAD_SEQUENCE／CRC_ERROR
最多整區塊重送一次；其他NACK、錯ACK位址／序號、串口斷線或Flash錯誤停止。
Flash可能部分寫入時不嘗試猜測續傳，必須重新建立完整session。

## 4. 不變的保護與安裝順序

1. 以debugger安裝F13 v5 matching Boot/App（或先安裝Boot並確保App區無舊有效映像）。
2. 用既有F24 Boot，經Misc安裝App1.5.0、明確RUN並重新Probe；確認bit12。
3. 選CAN、Classical500K，載入F13 v5 `_update.bin`，Discover／必要時ENTER。
4. UI及worker比對本地CRC、policy、MCU、layout、protocol及live INFO；不修改BIN。
5. RESET／64筆METADATA／SEAL仍在START前完成，256-byte整區先驗證、不擦寫。
6. START後依序提交64-byte區塊；END仍檢查完整長度、metadata及whole-image CRC。
7. END成功後reset，再跑boot decision。取消／Flash錯誤保留Boot；不完整映像不跳App。

530-byte ROM餘裕通過512門檻；為維持8 KiB，移除RAM dump與逐次entry/size等冗餘
文字。UART仍有clock／CAN結果、fail正值錯誤碼、updated／RST／leave。
例如fail05=Flash、07=CRC、09=image、0A=session timeout；完整版本、長度由INFO／Tool取得。
TJA1050仍須5V；X1=4MHz、CPU32MHz、CAN500K與所有向量／RAM契約不變。

## 5. 驗證與尚待板測

自動測試執行實際PC parser／client／runner、F24 burst codec與非阻塞CAN progression、
F13 block staging／CRC；涵蓋16種尾段長度、缺段、壞CRC、衝突重送、ACK遺失、
錯ACK、Flash錯誤、斷線、取消、同區塊不重寫與模擬逐word斷電。HAL及Flash是模型，
不是實際FSL／CAN bus／電源測量；既有metadata／IRQ／layout artifact guards保留。

新版實板正常更新與END CRC已有上方單次log；bus-off、拔線、真實斷電、
reset後新版App啟動與E2 Lite／IRQ runtime仍待驗證。
同一v5 BIN、CAN500K，先Host115200／相同trace，再trace-off與460800／921600各至少3次。
保存summary bytes／blocks／ms／B/s、CRC、UART overflow、CAN lost/error與reset後App log。
CAN 傳送結果以 wire trace 及 device 回覆核對；PC TX log 記錄的是傳送要求。
