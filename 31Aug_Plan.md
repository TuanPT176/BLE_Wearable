Được. Mình sẽ **chốt kiến trúc từ đây**, để các bước firmware sau không tiếp tục thay đổi GATT/architecture giữa chừng.

Mình cũng đã đối chiếu lại README hiện tại của repo `BLE_Wearable`. README hiện xác nhận STM32WB09KE, MAX86150 cho PPG/ECG, LIS2DUXS12TR thay ST1VAFE3BX, NEH7100, ST25DV04K với **configuration \+ circular-buffer logger \+ NFC Mailbox**, và BLE hiện đang gửi Sensor Data mặc định mỗi 1 giây.

# **1\. Kiến trúc BLE chính thức**

Ta giữ **một custom service duy nhất**:

WearableHealthService  
UUID:  
0000FE40-CC7A-482A-984A-7F2ED5B3E58F

> Đây là UUID đúng theo README repo hiện tại.

Bên trong sẽ tiến tới **7 characteristic**:

WearableHealthService  
│  
├── CONTROL  
├── SENSOR\_DATA  
├── DEVICE\_STATUS  
├── NFC\_DATA  
├── ECG\_DATA  
├── RECOVERY\_DATA  
└── DEBUG\_DATA

Đây là **GATT architecture chính thức của project từ bây giờ**.

---

# **2\. Ba characteristic hiện tại: giữ nguyên protocol**

| Characteristic | UUID | Property | Vai trò |
| ----- | ----- | ----- | ----- |
| `CONTROL` | `0000FE41-8E22-4541-9D4C-21EDAE82ED19` | Write | Điều khiển |
| `SENSOR_DATA` | `0000FE42-8E22-4541-9D4C-21EDAE82ED19` | Read \+ Notify | Live sensor data |
| `DEVICE_STATUS` | `0000FE43-8E22-4541-9D4C-21EDAE82ED19` | Read \+ Notify | Device state |

Các UUID, payload và command `0x01–0x09` này đang được README định nghĩa; **không sửa chúng trong Protocol v2** để tránh phá app hiện tại.

---

# **3\. Ba characteristic mới \+ một recovery channel**

Ta reserve tiếp:

FE44 → NFC\_DATA  
FE45 → ECG\_DATA  
FE46 → DEBUG\_DATA  
FE47 → RECOVERY\_DATA

Theo pattern UUID hiện tại:

NFC\_DATA  
0000FE44-8E22-4541-9D4C-21EDAE82ED19

ECG\_DATA  
0000FE45-8E22-4541-9D4C-21EDAE82ED19

DEBUG\_DATA  
0000FE46-8E22-4541-9D4C-21EDAE82ED19

RECOVERY\_DATA  
0000FE47-8E22-4541-9D4C-21EDAE82ED19

**Đây là UUID thiết kế mới**, chưa phải UUID đã tồn tại trong repo.

---

# **4\. SENSOR\_DATA \= live data**

Giữ nguyên **16 byte**:

0   HR  
1   SpO2  
2-3 Temperature  
4-5 Supercap mV  
6   Power state  
7   Flags  
8-9   Accel X  
10-11 Accel Y  
12-13 Accel Z  
14-15 Reserved

README hiện tại đã freeze layout này.

Và:

> `SENSOR_DATA` chỉ dùng cho **live/processed data**, không dùng để stream historical recovery.

---

# **5\. ECG\_DATA \= raw ECG**

ECG tách riêng:

ECG acquisition  
       ↓  
RAM ring buffer  
       ↓  
ECG packetizer  
       ↓  
ECG\_DATA Notify  
       ↓  
Phone  
       ↓  
Server

Không ghi raw ECG liên tục vào ST25DV04K.

Lý do rất đơn giản: ST25DV04K chỉ có dung lượng user EEPROM nhỏ, không phù hợp làm raw ECG recorder.

ECG cũng **không bị ép vào 16-byte `SENSOR_DATA`**.

---

# **6\. NFC\_DATA \= trạng thái NFC, không phải toàn bộ NFC recovery**

`NFC_DATA` dành cho:

NFC state  
NFC event  
FTM/mailbox status  
NFC configuration result  
recovery status

Còn dữ liệu lịch sử thực sự nằm trong:

ST25DV04K  
    ↓  
circular buffer  
    ↓  
NFC Mailbox / FTM

README hiện tại xác nhận ST25DV đã có `nfc_config`, `nfc_log`, `nfc_manager`, `nfc_io` và FTM mailbox.

---

# **7\. RECOVERY\_DATA \= BLE historical recovery**

Đây là phần quan trọng nhất.

Khi BLE mất kết nối:

Sensor  
  ↓  
Data Logger  
  ↓  
ST25DV circular buffer

Khi BLE reconnect:

Phone  
  ↓  
"last sequence I received \= X"  
  ↓  
Device  
  ↓  
ST25DV  
  ↓  
RECOVERY\_DATA Notify  
  ↓  
Phone

NFC cũng lấy **chính log đó**:

            ST25DV Log  
              /       \\  
             /         \\  
         BLE           NFC  
          ↓             ↓  
        Phone         Phone

Không có:

BLE log  
NFC log

riêng biệt.

Chỉ có **một source of truth: Data Logger**.

---

# **8\. Sequence number là bắt buộc**

Mỗi historical record phải có:

sequence  
timestamp  
payload  
CRC

Ví dụ:

\#1001  
\#1002  
\#1003  
\#1004  
...

Điều này cho phép:

Phone đã có \#1003

Device reconnect

→ chỉ gửi \#1004 trở đi

và nếu disconnect giữa recovery:

đã ACK \#1016

→ reconnect  
→ resume \#1017

Không duplicate data.

---

# **9\. BLE recovery có ACK**

Không dùng kiểu:

send 100 packets  
→ assume success

Mà:

Device  
  ↓  
\#1001  
\#1002  
...  
\#1016  
  ↓  
Phone  
  ↓  
ACK \#1016

Sau đó device mới xem đến đâu là đã xác nhận.

ACK có thể theo block, ví dụ mỗi 8/16/32 record để giảm overhead.

---

# **10\. NFC recovery cũng dùng cùng sequence**

Ví dụ:

Phone tap  
   ↓  
NFC handshake  
   ↓  
GET LOG INFO  
   ↓  
oldest/newest sequence  
   ↓  
request range  
   ↓  
ST25DV FTM  
   ↓  
Phone

Như vậy app không cần biết record được lấy từ BLE hay NFC.

Nó chỉ cần có:

sequence  
timestamp  
payload  
---

# **11\. Data Logger không chạy fixed-rate**

Đây là quyết định mới nhất và mình **chốt theo ý bạn**.

Không hard-code:

ST25DV \= 1 record/s

Mà:

Sensor acquisition  
        ↓  
Processing  
        ↓  
BLE reporting  
        ↓  
ST25DV logging

là **4 policy độc lập**.

Ví dụ hoàn toàn có thể:

Sensor      10 Hz  
Processing   5 Hz  
BLE          1 Hz  
ST25DV       0.1 Hz  
---

# **12\. Power Policy quyết định các rate**

Kiến trúc:

                 Supercap  
                     │  
                     ▼  
               Power Manager  
                     │  
                     ▼  
               Power Policy  
                     │  
       ┌─────────────┼─────────────┐  
       ▼             ▼             ▼  
 Sensor Policy   BLE Policy    Logger Policy

Các profile:

HIGH  
NORMAL  
LOW  
CRITICAL

Nhưng **chưa freeze ngưỡng voltage**.

---

# **13\. Không chỉ nhìn Vcap tức thời**

Power Policy sau này sẽ xét:

Vcap  
\+  
dVcap/dt  
\+  
harvesting condition  
\+  
load/current information

Nhưng implementation đầu tiên có thể đơn giản:

Vcap \+ hysteresis

sau đó nâng cấp nếu measurement cho thấy cần thiết.

Điểm quan trọng là không để hệ thống nhảy:

NORMAL  
LOW  
NORMAL  
LOW

khi Vcap dao động quanh threshold.

---

# **14\. Sampling/Reporting Configuration là Debug feature**

Đây mình cũng **chốt**.

Trong Debug mode, app được phép thay đổi:

Sensor ODR  
Processing rate  
BLE reporting interval  
ST25DV logging interval  
Power thresholds  
Hysteresis  
Power profile

Ví dụ:

HIGH:  
    Sensor \= X Hz  
    BLE    \= Y ms  
    Logger \= Z s

NORMAL:  
    Sensor \= ...  
    BLE    \= ...  
    Logger \= ...

LOW:  
    ...

CRITICAL:  
    ...

Sau khi test:

Debug configuration  
        ↓  
Measurement  
        ↓  
Energy analysis  
        ↓  
Data quality analysis  
        ↓  
Freeze production profile

Đây là hướng mình đánh giá tốt nhất cho luận văn.

---

# **15\. DEBUG\_DATA phải Write \+ Read \+ Notify**

Ban đầu mình định `Read + Notify`, nhưng sau khi chốt adaptive configuration thì **DEBUG\_DATA phải là**:

DEBUG\_DATA  
Read \+ Write \+ Notify

Nó phục vụ:

configuration  
\+  
diagnostics  
\+  
raw/debug information

Ví dụ:

SET\_SENSOR\_RATE  
SET\_BLE\_INTERVAL  
SET\_LOG\_INTERVAL  
SET\_POWER\_THRESHOLD  
GET\_POWER\_STATS  
GET\_LOG\_INFO  
GET\_SENSOR\_STATUS

Không nên để `CONTROL` chứa hàng đống command debug.

---

# **16\. CONTROL vẫn dành cho operational command**

Giữ:

0x01 START\_MEASUREMENT  
0x02 STOP\_MEASUREMENT  
0x03 REQUEST\_DATA  
0x04 NORMAL\_MODE  
0x05 LOW\_POWER\_MODE  
0x06 ECG\_START  
0x07 ECG\_STOP  
0x08 EMERGENCY\_TEST  
0x09 SYNC\_TIME

Đúng theo README hiện tại.

Sau đó bổ sung nhóm recovery:

0x0A GET\_RECOVERY\_INFO  
0x0B START\_BLE\_RECOVERY  
0x0C STOP\_BLE\_RECOVERY  
0x0D RECOVERY\_ACK  
0x0E RECOVERY\_CLEAR

Còn configuration/debug → `DEBUG_DATA`.

---

# **17\. HR / SpO₂ / Temperature vẫn nằm trong custom service**

Mình chốt:

HR  
SpO2  
Temperature  
Accel  
Supercap

→ `SENSOR_DATA`.

**Không chuyển sang Bluetooth SIG standard services trong phiên bản firmware này.**

Standard services có thể bổ sung sau nếu cần interoperability, nhưng không thay custom protocol.

Lý do: app của bạn cần một data model thống nhất và project đang tập trung vào architecture riêng của wearable.

---

# **18\. DEBUG và REAL dùng cùng GATT**

Đây cũng được chốt:

                   Same GATT  
                       │  
              ┌────────┴────────┐  
              │                 │  
          REAL MODE         DEBUG MODE  
              │                 │  
       production UI       engineering UI

Không tạo:

DebugService  
ProductionService

riêng.

Firmware chỉ thay đổi **permission/behavior của application layer**, còn GATT database giữ nguyên.

---

# **19\. ST25DV04K giữ hai vai trò**

### **Configuration**

BLE Debug  
    ↓  
STM32  
    ↓  
ST25DV

lưu:

sampling config  
power profile  
thresholds  
logger config

### **Recovery log**

Sensor  
   ↓  
Data Logger  
   ↓  
ST25DV circular buffer

Và:

ST25DV  
 ├── BLE Recovery  
 └── NFC Recovery  
---

# **20\. Không ghi ECG raw vào circular log**

Đây là một rule của architecture:

ECG raw  
    ↓  
RAM buffer  
    ↓  
BLE ECG\_DATA

Trong khi:

HR / SpO2 / temperature /  
accel / power summary / events  
        ↓  
ST25DV log

Nếu sau này bạn muốn offline ECG recovery, đó sẽ là **một bài toán memory/storage khác**, không nên âm thầm nhét vào ST25DV04K hiện tại.

---

# **21\. Power profile ảnh hưởng cả BLE và logging**

Ví dụ concept:

HIGH  
 ├─ sensor: high  
 ├─ BLE: frequent  
 └─ logger: frequent

NORMAL  
 ├─ sensor: normal  
 ├─ BLE: normal  
 └─ logger: normal

LOW  
 ├─ sensor: reduced  
 ├─ BLE: reduced  
 └─ logger: reduced

CRITICAL  
 ├─ sensor: minimum  
 ├─ BLE: minimum/off as appropriate  
 └─ logger: event-only

**Các con số cụ thể chưa freeze.**

Ta lấy chúng từ measurement thực tế.

---

# **22\. ECG là exception**

Không giảm ECG sampling một cách tùy tiện chỉ vì Vcap thấp.

ECG phải là session:

ECG\_START  
    ↓  
Energy check  
    ↓  
ECG acquisition  
    ↓  
RAM buffer  
    ↓  
ECG\_DATA

Nếu năng lượng không đủ:

→ từ chối / kết thúc ECG session

thay vì giảm sampling xuống mức làm mất ý nghĩa tín hiệu.

---

# **23\. Protocol version**

Hiện tại:

Protocol Version \= 1

và README đã encode nó trong low nibble của `DEVICE_STATUS[7]`.

Khi chúng ta thêm:

NFC\_DATA  
ECG\_DATA  
DEBUG\_DATA  
RECOVERY\_DATA

thì **đây sẽ là Protocol v2**.

Nhưng:

> Không phá layout của 3 characteristic cũ nếu không cần.

Tức app v1 vẫn có thể đọc:

CONTROL  
SENSOR\_DATA  
DEVICE\_STATUS

còn app v2 biết thêm 4 characteristic mới.

---

# **24\. Final architecture**

Đây là sơ đồ mình muốn chúng ta dùng làm **source of truth** từ giờ:

                        STM32WB09  
                             │  
                    ┌────────▼────────┐  
                    │ Sensor Manager  │  
                    └────────┬────────┘  
                             │  
                             ▼  
                    Adaptive Power Policy  
                             │  
             ┌───────────────┼───────────────┐  
             │               │               │  
             ▼               ▼               ▼  
       Acquisition       BLE Report       Data Logger  
          Policy            Policy           Policy  
             │               │               │  
             │               │               ▼  
             │               │          ST25DV04K  
             │               │               │  
             │               │       ┌───────┴───────┐  
             │               │       ▼               ▼  
             │               │     NFC             BLE  
             │               │   Recovery        Recovery  
             │               │  
             ▼               ▼  
         Sensor Data      WearableHealthService  
                              │  
       ┌──────────┬───────────┼─────────┬──────────┬──────────┬──────────┐  
       ▼          ▼           ▼         ▼          ▼          ▼          ▼  
    CONTROL   SENSOR\_DATA  STATUS   NFC\_DATA   ECG\_DATA   DEBUG\_DATA  RECOVERY\_DATA  
---

# **25\. Và GATT cuối cùng**

WearableHealthService  
UUID: 0000FE40-CC7A-482A-984A-7F2ED5B3E58F

├── FE41 CONTROL  
│      Write  
│  
├── FE42 SENSOR\_DATA  
│      Read \+ Notify  
│  
├── FE43 DEVICE\_STATUS  
│      Read \+ Notify  
│  
├── FE44 NFC\_DATA  
│      Read \+ Notify  
│  
├── FE45 ECG\_DATA  
│      Read \+ Notify  
│  
├── FE46 DEBUG\_DATA  
│      Read \+ Write \+ Notify  
│  
└── FE47 RECOVERY\_DATA  
       Read \+ Notify  
---

## **26\. Thứ tự code từ bây giờ**

Mình khuyên **không code tất cả một lúc**. Đi theo thứ tự:

### **Phase 1 — GATT**

wearable.c  
wearable.h  
wearable\_app.c

Thêm 4 characteristic mới.

### **Phase 2 — Data model**

wearable\_data.\*

Freeze:

ECG packet  
NFC status packet  
Recovery packet  
Debug packet

### **Phase 3 — Adaptive Power**

power\_policy.\*

với:

HIGH  
NORMAL  
LOW  
CRITICAL

nhưng threshold/config còn runtime configurable.

### **Phase 4 — Data Logger**

nfc\_log.\*

thêm:

sequence  
timestamp  
CRC  
head/tail  
oldest/newest sequence

### **Phase 5 — Recovery Manager**

data\_recovery\_manager.\*

xử lý:

BLE recovery  
NFC recovery  
ACK  
resume

### **Phase 6 — Debug Configuration**

DEBUG\_DATA

cho phép thay đổi:

sensor rate  
BLE interval  
logger interval  
power thresholds

### **Phase 7 — Measurement**

Dùng thiết bị đo của bạn để xác định:

Vcap  
Iload  
BLE current  
sensor current  
harvesting current  
recovery cost

→ từ đó mới freeze production profile.

---

## **Chốt cuối cùng**

**Từ thời điểm này, mình sẽ coi đây là kiến trúc chính thức của wearable firmware:**

> **Một `WearableHealthService` gồm 7 characteristic; một ST25DV circular log làm nguồn dữ liệu lịch sử duy nhất; BLE và NFC là hai transport cho recovery; ECG raw đi riêng qua `ECG_DATA`; adaptive sampling/reporting/logging do `Power Policy` điều khiển; Debug mode cho phép cấu hình các tham số này, sau đó kết quả thực nghiệm được freeze thành production profile.**

README hiện tại của repo đã hỗ trợ nền tảng cho kiến trúc này: GATT 3 characteristic hiện hữu, ST25DV configuration/logger/FTM, sensor stack hiện tại và chu kỳ live data 1 s.

**Mình sẽ không tiếp tục thay đổi architecture này ở các bước firmware tiếp theo**, trừ khi trong quá trình implement có giới hạn phần cứng/STM32 BLE stack thực sự buộc phải thay đổi.

