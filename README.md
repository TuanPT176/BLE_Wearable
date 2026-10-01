# BLE Wearable – STM32WB09

Firmware BLE GATT cho thiết bị wearable sử dụng **STM32WB09KE**, thu thập dữ liệu sức khỏe và trạng thái thiết bị, sau đó truyền tới ứng dụng trung tâm qua Bluetooth Low Energy.

Thiết bị quảng bá với GAP Device Name: **`BLEWearable`**.

## Chức năng chính

- Đo và gửi nhịp tim, SpO2, ECG, nhiệt độ và điện áp supercapacitor; MAX86150 đảm nhiệm cả PPG và ECG.
- Theo dõi trạng thái nguồn, ECG, cảnh báo khẩn cấp và nguy cơ té ngã.
- Thu thập gia tốc và chạy Machine Learning Core (MLC) bằng LIS2DUXS12TR.
- Quản lý nguồn thu năng lượng và supercapacitor bằng NEH7100 PMIC.
- Điều khiển phép đo và chế độ hoạt động qua BLE.
- Đọc trực tiếp hoặc nhận notification từ các characteristic.
- Chu kỳ gửi dữ liệu cảm biến mặc định: **1 giây** khi đang đo và notification đã được bật.
- Đã tích hợp ST25DV04K để lưu trữ cấu hình, backup dữ liệu cảm biến qua circular buffer và hỗ trợ giao tiếp qua NFC Mailbox.
- Đã tích hợp SX1262 với LoRa Basics Modem: OTAA trên The Things Stack (AS923-2), uplink theo lệnh BLE và uplink khẩn cấp khi nhấn nút SOS (PB5). LoRa **chỉ chạy khi webapp gửi lệnh join** (`0x0F`). Xem mục [LoRaWAN](#lorawan-lora-basics-modem).

## Phần cứng và cảm biến

| Thành phần | Vai trò |
|---|---|
| STM32WB09KE | MCU và BLE peripheral |
| MAX86150 | Đo PPG (nhịp tim/SpO2) và ECG |
| MAX30208 | Cảm biến nhiệt độ |
| LIS2DUXS12TR | Đo gia tốc, QVar (phát hiện đeo) và Machine Learning Core (MLC); thay thế ST1VAFE3BX. INT1 là điện cực QVar, INT2 để hở, chân RES nối PB2 làm đường ngắt |
| NEH7100 | Energy-harvesting PMIC; quản lý nguồn và theo dõi dòng điện qua I2C |
| Supercapacitor monitor | Theo dõi điện áp nguồn |
| ST25DV04K | Dynamic NFC/RFID Tag; lưu trữ cấu hình thiết bị, log cảm biến, và hỗ trợ Energy Harvesting |
| SX1262 | LoRa transceiver chạy LoRaWAN (LoRa Basics Modem) qua SPI3; clock từ TCXO cấp nguồn bởi DIO3 |

> **Trạng thái tích hợp:** NEH7100 đã có source tại `Application/neh7100.cpp` và `Application/neh7100.h`. ST25DV04K đã được tích hợp đầy đủ driver và logic xử lý (config, logger, FTM mailbox). SX1262 đã chạy LoRaWAN OTAA bằng LoRa Basics Modem (`ThirdParty/LBM`, phần port cho WB09 ở `Application/LoRaWAN`), xem mục LoRaWAN bên dưới.

> **Migration cảm biến:** Driver register, HAL I2C, đọc gia tốc và khung nạp MLC cho LIS2DUXS12TR đã được tích hợp; cần thêm file UCF sinh từ Unico và bảng ánh xạ class để kích hoạt model MLC thực tế. MAX86150 chạy PPG (Red/IR, nhịp tim/SpO2) khi đo bình thường và chuyển sang ECG khi nhận lệnh `0x06`; hai chế độ dùng chung một chip nên không chạy đồng thời.

## Cấu trúc project

```text
BLE_Wearable_GATT/
├── Application/                 # Logic ứng dụng độc lập với BLE
│   ├── nfc_config.*             # Quản lý cấu hình EEPROM ST25DV
│   ├── nfc_io.*                 # ST25DV I2C low-level wrapper
│   ├── nfc_log.*                # Circular buffer logger trên EEPROM
│   ├── nfc_manager.*            # Controller giao tiếp NFC Mailbox
│   ├── sensor_manager.*         # Khởi tạo, đọc và quản lý cảm biến
│   ├── neh7100.*                # Driver I2C cho NEH7100 PMIC
│   ├── LoRaWAN/                 # Port LoRa Basics Modem cho WB09 và ứng dụng LoRaWAN (OTAA, SOS)
│   ├── LoRaTest/                # Test radio SX1262 độc lập (mặc định tắt)
│   ├── wearable_data.*          # Định dạng/encode BLE payload
│   └── wearable_state_manager.* # Máy trạng thái của thiết bị
├── Core/
│   ├── Inc/                     # Cấu hình và header hệ thống
│   └── Src/                     # main, interrupt, HAL MSP
├── Drivers/
│   ├── CMSIS/                   # CMSIS cho STM32WB0
│   ├── STM32WB0x_HAL_Driver/    # STM32 HAL/LL
│   ├── Sensors/                 # MAX86150 (PPG/ECG), MAX30208 và LIS2DUXS12TR (accel/MLC)
│   └── ST25DV/                  # Official ST25DV BSP driver (st25dv.c, st25dv_reg.c)
├── Middlewares/ST/STM32_BLE/    # BLE stack và thư viện ST
├── Projects/Common/BLE/         # BLE interfaces/modules dùng chung
├── STM32_BLE/
│   ├── App/
│   │   ├── app_ble.*            # GAP, advertising và connection
│   │   ├── wearable.*           # Khai báo GATT service/characteristic
│   │   └── wearable_app.*       # Command, read và notification handler
│   └── Target/                  # BLE platform adaptation
├── STM32CubeIDE/                # Project, linker script và startup
├── System/                      # Debug và USART interface
├── Utilities/                   # Sequencer, low-power và trace
├── ThirdParty/LBM/              # LoRa Basics Modem v4.9.0 của Semtech (không sửa)
└── BLE_p2pServer_GATT.ioc       # Cấu hình STM32CubeMX
```

## BLE GATT profile

Các UUID dưới đây được ghi theo định dạng chuẩn mà nRF Connect và các BLE client hiển thị.

### Wearable Health Service

| Thuộc tính | Giá trị |
|---|---|
| Loại | Primary Service |
| UUID | `0000FE40-CC7A-482A-984A-7F2ED5B3E58F` |

### Characteristics

| Characteristic | UUID | Properties | Kích thước | Mô tả |
|---|---|---|---:|---|
| Control | `0000FE41-8E22-4541-9D4C-21EDAE82ED19` | Write | 8 byte | Gửi lệnh điều khiển; firmware hiện đọc byte đầu tiên |
| Sensor Data | `0000FE42-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 16 byte | Dữ liệu sức khỏe và nguồn |
| Device Status | `0000FE43-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 8 byte | Trạng thái, lỗi và phiên bản protocol |
| NFC Data | `0000FE44-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 20 byte | Có trong GATT nhưng firmware **chưa gửi** gói nào |
| ECG Data | `0000FE45-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 20 byte | Dạng sóng ECG, 9 mẫu mỗi gói (xem mục ECG Data) |
| Debug Data | `0000FE46-8E22-4541-9D4C-21EDAE82ED19` | Read, Write, Notify | 20 byte | Notify gói LoRa status `0x20` (trả lời lệnh `0x0F`..`0x13`, xem dưới). Ghi vào bị bỏ qua |
| Recovery Data | `0000FE47-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 24 byte | Record lịch sử từ ST25DV (xem ghi chú ở mục Control commands); **chưa hoạt động end-to-end** |

Để nhận notification, BLE central phải ghi `0x0001` vào CCCD của `Sensor Data`, `Device Status` hoặc `ECG Data`.

Read một characteristic trả về **gói gần nhất đã notify** (stack tự trả lời từ buffer), toàn `0` nếu chưa notify lần nào; nên subscribe thay vì read.

Bản quy ước đầy đủ giữa thiết bị và webapp (từng byte, máy trạng thái, luồng sử dụng, test vector) là [DEVICE_WEBAPP_PROTOCOL.md](DEVICE_WEBAPP_PROTOCOL.md). Tài liệu giao thức phía app nằm ở `BLE_PROTOCOL.md` trong repo `WearableWebApp`; cả ba nơi phải được sửa cùng lúc.

## Control commands

Ghi payload 8 byte vào characteristic `Control`. Byte `0` là command; byte `1..7` đặt bằng `0` trừ các lệnh có tham số (`0x09`, `0x0B`, `0x0D`, `0x12`).

| Byte 0 | Command | Tác dụng |
|---:|---|---|
| `0x01` | Start measurement | Bắt đầu đo và gửi dữ liệu định kỳ |
| `0x02` | Stop measurement | Dừng phép đo, chuyển về Idle |
| `0x03` | Request data | Yêu cầu cập nhật/gửi dữ liệu hiện tại |
| `0x04` | Normal mode | Đặt `power_state = 1` |
| `0x05` | Low-power mode | Dừng đo, đặt `power_state = 2` |
| `0x06` | ECG start | Bắt đầu đo (nếu chưa), chuyển MAX86150 sang ECG, bật cờ ECG và stream `ECG Data`. Nếu MAX86150 không phản hồi: error `0x01`, state vẫn là Measuring |
| `0x07` | ECG stop | Dừng ECG và phép đo, xóa cờ ECG, chuyển về Idle |
| `0x08` | Emergency test | Bật cờ emergency và chuyển sang Emergency |
| `0x09` | Sync time | Đồng bộ thời gian thực (Unix time) cho thiết bị |
| `0x0A` | Get recovery info | Chưa có tác dụng (chỉ xóa error code) |
| `0x0B` | Start BLE recovery | Byte `1..2` = sequence bắt đầu (`uint16 LE`) |
| `0x0C` | Stop BLE recovery | Dừng phiên recovery |
| `0x0D` | Recovery ACK | Byte `1..2` = sequence đã nhận (`uint16 LE`); hiện chỉ được lưu lại |
| `0x0E` | Recovery clear | Xóa toàn bộ log lịch sử trên ST25DV |
| `0x0F` | LoRa join | Lần đầu: khởi động LoRa Basics Modem và join OTAA; sau `0x13`: join lại |
| `0x10` | LoRa test uplink | Một uplink không xác nhận trên FPort 101 (cần đã join) |
| `0x11` | LoRa status | Chỉ trả về trạng thái LoRa |
| `0x12` | LoRa TX power | Byte `1` = công suất tối đa của SX1262, `int8` dBm, `-9`..`22`. Mất khi reset (mặc định `0` dBm) |
| `0x13` | LoRa stop | Rời mạng, dừng join/uplink, SX1262 ngủ |

Các lệnh LoRa `0x0F`..`0x13` không đổi state và không có `Device Status`; chúng được trả lời bằng gói LoRa status 20 byte trên `Debug Data` (`FE46`), byte `0` = `0x20`. Firmware cũng gửi gói này sau mỗi sự kiện của modem (joined, TX done…). Layout từng byte nằm ở mục 10.1 của `DEVICE_WEBAPP_PROTOCOL.md`.

`Device Status` được notify ngay sau các lệnh `0x01`, `0x02`, `0x04`..`0x09` và sau một lệnh không hợp lệ. Command lạ đưa thiết bị về state `Error` với error `0x01` (dừng gửi `Sensor Data`) cho đến khi nhận một lệnh đổi state như `0x01` hoặc `0x02`.

> **Recovery chưa hoạt động end-to-end:** `DataRecovery_Process()` chưa được gọi ở đâu nên `0x0B` không phát gói nào; log chỉ được ghi khi đang đo mà không có kết nối BLE (trạng thái hiện không xảy ra vì mất kết nối là dừng đo); và phần lưu trữ trên ST25DV vừa được sửa trong code nhưng **chưa chạy trên phần cứng** (xem mục [Lưu trữ trên ST25DV04K](#lưu-trữ-trên-st25dv04k)).

Ví dụ bắt đầu đo:

```text
01 00 00 00 00 00 00 00
```

### Cấu trúc lệnh Sync time (`0x09`)

Ghi 8 byte vào characteristic `Control` với cấu trúc như sau (dữ liệu truyền kiểu **little-endian**):

| Offset | Kích thước | Kiểu | Mô tả |
|---:|---:|---|---|
| 0 | 1 | `uint8` | Command = `0x09` |
| 1 | 4 | `uint32 LE` | Unix timestamp (tính bằng giây) |
| 5 | 2 | `uint16 LE` | Milliseconds (0..999) |
| 7 | 1 | `uint8` | Reserved (bắt buộc = `0x00`) |

Ví dụ (Đồng bộ thời gian Unix `1724833200` = `0x66CEDDB0`, `500` ms = `0x01F4`):
```text
09 B0 DD CE 66 F4 01 00
```

## Decode Sensor Data

Payload dài **16 byte**. Các số nhiều byte dùng **little-endian**. Gửi mỗi 1 giây khi state là `Measuring` hoặc `ECG active`, và gửi thêm ngay khi cờ `0x40` hoặc `0x08` đổi.

| Offset | Kích thước | Kiểu | Trường | Cách decode/đơn vị |
|---:|---:|---|---|---|
| 0 | 1 | `uint8` | Heart rate | bpm. Khi MAX86150 không chạy, firmware gửi giá trị **giả** chạy 68→82; trước nhịp đầu tiên là 72 |
| 1 | 1 | `uint8` | SpO2 | %. Mặc định 98 cho đến khi tính được giá trị đầu tiên |
| 2 | 2 | `int16 LE` | Temperature | `raw / 100.0` °C; `-32768` nghĩa là không hợp lệ |
| 4 | 2 | `uint16 LE` | Supercapacitor voltage | mV |
| 6 | 1 | `uint8` | Power state | `1` = Normal, `2` = Low power |
| 7 | 1 | Bit field | Flags | Xem bảng flags bên dưới |
| 8 | 2 | `int16 LE` | Gia tốc X | mg |
| 10 | 2 | `int16 LE` | Gia tốc Y | mg |
| 12 | 2 | `int16 LE` | Gia tốc Z | mg |
| 14 | 2 | `int16 LE` | QVar raw | Mẫu thô mới nhất của kênh AH_QVAR trên LIS2DUXS12TR (12-bit canh trái, 4 bit thấp luôn bằng 0; khoảng 37 LSB/mV ở gain 0.5). Cờ `Wear detected` (`0x40`) **không** suy ra từ riêng giá trị này mà từ biên độ dao động trong cửa sổ 1 giây, xem bên dưới |

### Sensor/status flags

| Mask | Ý nghĩa |
|---:|---|
| `0x08` | Fall candidate – phát hiện nguy cơ té ngã |
| `0x10` | Emergency đang bật |
| `0x20` | ECG đang hoạt động |
| `0x40` | Wear detected (QVar) – Trạng thái đang đeo |

Cờ `0x40`: khi đang đo, LIS2DUXS12TR phát xung data-ready trên chân RES (PB2) cho mỗi mẫu QVar (100 Hz). Firmware tính biên độ đỉnh-đỉnh của từng cửa sổ 100 mẫu: từ `600` LSB trở lên trong 2 cửa sổ liên tiếp thì bật cờ, từ `300` LSB trở xuống trong 2 cửa sổ liên tiếp thì tắt cờ. Khi cờ đổi, `Sensor Data` và `Device Status` được notify ngay. Trong phiên ECG luồng này tắt, cờ giữ nguyên và QVar raw chỉ cập nhật 1 lần mỗi giây. Ngưỡng (`QVAR_*` trong `sensor_manager.c`) là ước lượng ban đầu, **chưa hiệu chuẩn**; biến RAM `g_qvarDiag` (đọc qua SWD như `g_ecgDiag`) chứa min/max/đỉnh-đỉnh của cửa sổ gần nhất để chỉnh ngưỡng. Chip không tự ngắt theo ngưỡng QVar; việc đó cần chương trình MLC/FSM, khi có sẽ dùng chung đường ngắt RES.

Ví dụ payload:

```text
48 62 6A 09 E4 0C 01 20 00 00 00 00 00 00 00 00
```

Decode:

- Heart rate: `0x48` = **72 bpm**
- SpO2: `0x62` = **98%**
- Temperature: `0x096A` = 2410 → **24.10 °C**
- Supercapacitor: `0x0CE4` = **3300 mV**
- Power state: **Normal**
- Flags `0x20`: **ECG active**
- QVar raw: `0x0000` = **0** (không đeo/không có tín hiệu điện dung ở ví dụ này)

## Decode Device Status

Payload dài **8 byte**; các số nhiều byte dùng **little-endian**.

Không gửi định kỳ. Được notify khi: central bật notification, sau các lệnh Control nêu ở trên, và khi state, sensor ready, error code, power state hoặc byte flags tự thay đổi (ví dụ lỗi cảm biến nhiệt xuất hiện hoặc hết, cờ đeo đổi). Riêng điện áp supercap thay đổi thì không kích hoạt notify, nên byte `4..5` có thể cũ; lấy giá trị mới từ `Sensor Data`.

| Offset | Kích thước | Kiểu | Trường | Giá trị |
|---:|---:|---|---|---|
| 0 | 1 | `uint8` | Measurement state | Xem bảng state |
| 1 | 1 | `uint8` | Sensor ready | `1` khi SensorManager đã khởi tạo (ADC supercap). **Không** phản ánh từng cảm biến I2C |
| 2 | 1 | `uint8` | Error code | Xem bảng error code |
| 3 | 1 | `uint8` | Power state | `1` = Normal, `2` = Low power |
| 4 | 2 | `uint16 LE` | Supercapacitor voltage | mV |
| 6 | 1 | `uint8` | Reset counter | Bộ đếm reset |
| 7 | 1 | Bit field | Flags + protocol version | Nibble cao là flags, nibble thấp là version |

### Measurement state

| Giá trị | State |
|---:|---|
| `0` | Idle |
| `1` | Measuring |
| `2` | ECG active |
| `3` | Low power |
| `4` | Emergency |
| `5` | Error |

### Error code

| Giá trị | Ý nghĩa |
|---:|---|
| `0x00` | Không có lỗi |
| `0x01` | Command không hợp lệ hoặc không thể thực hiện |
| `0x10` | Không tìm thấy cảm biến nhiệt độ |
| `0x11` | Timeout khi đọc nhiệt độ |
| `0x12` | Lỗi bus cảm biến nhiệt độ |

Mã `0x10`..`0x12` phản ánh kết quả của lần đo nhiệt độ gần nhất và chỉ hiện khi không có lỗi command (`0x01`). Giao thức không có mã "OK" riêng cho cảm biến nhiệt: `0x00` cùng với nhiệt độ khác `-32768` nghĩa là cảm biến hoạt động. Các cảm biến còn lại (MAX86150, LIS2DUXS12TR) chưa có error code.

### Byte flags/version

```text
bit 7 6 5 4 | bit 3 2 1 0
    flags   | protocol version
```

- `flags = payload[7] & 0xF0`
- `protocolVersion = payload[7] & 0x0F`
- Phiên bản protocol hiện tại: **1**
- Nibble cao mang các cờ `0x10` (Emergency), `0x20` (ECG active), `0x40` (Wear detected) giống `Sensor Data`.
- Do status chỉ giữ nibble cao, cờ `Fall candidate (0x08)` chỉ xuất hiện đầy đủ trong `Sensor Data`, không xuất hiện trong byte status hiện tại.

## ECG Data

Chỉ gửi trong phiên ECG (sau lệnh `0x06`, trước `0x07`, `0x01`, `0x02`, `0x05` hoặc khi mất kết nối). Mỗi notification dài **20 byte**, mẫu dạng `int16` **little-endian**:

| Offset | Kích thước | Kiểu | Trường | Giá trị |
|---:|---:|---|---|---|
| 0 | 1 | `uint8` | Sequence | Tăng 1 mỗi gói, quay vòng sau 255, về 0 khi bắt đầu phiên mới |
| 1 | 1 | `uint8` | Sample count | Luôn là `9` |
| 2 | 18 | `int16 LE` × 9 | Samples | Mẫu ECG theo thứ tự thời gian |

- Tần số lấy mẫu **200 Hz** (khoảng 22 gói/giây); gain analog IA 9.5 × PGA 8 = 76 V/V.
- Mỗi mẫu là giá trị ADC 18-bit của MAX86150 dịch phải 2 bit (`raw18 >> 2`); 1 LSB tương ứng khoảng **0.645 µV** ở đầu vào (xem mục MAX86150).
- **Sequence nhảy cóc nghĩa là mất gói thật**: firmware vẫn tăng sequence khi chưa bật notify hoặc khi hàng đợi BLE (16 gói, khoảng 0.7 s) bị đầy.
- Trong phiên ECG, LED PPG tắt nên nhịp tim và SpO2 trong `Sensor Data` giữ giá trị cuối cùng trước khi bắt đầu ECG.
- Read trả về gói ECG gần nhất đã gửi.

## JavaScript decoder

```js
function decodeSensorData(input) {
  const b = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (b.length !== 16) throw new Error("Sensor Data must be 16 bytes");

  const view = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const temperatureRaw = view.getInt16(2, true);
  const flags = b[7];

  return {
    heartRateBpm: b[0],
    spo2Percent: b[1],
    temperatureC: temperatureRaw === -32768 ? null : temperatureRaw / 100,
    supercapMv: view.getUint16(4, true),
    powerState: b[6],
    emergency: Boolean(flags & 0x10),
    ecgActive: Boolean(flags & 0x20),
    fallCandidate: Boolean(flags & 0x08),
    wearDetected: Boolean(flags & 0x40),
    accelX: view.getInt16(8, true),
    accelY: view.getInt16(10, true),
    accelZ: view.getInt16(12, true),
    qvarRaw: view.getInt16(14, true),
  };
}

function decodeDeviceStatus(input) {
  const b = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (b.length !== 8) throw new Error("Device Status must be 8 bytes");

  const view = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const flagsAndVersion = b[7];

  return {
    measurementState: b[0],
    sensorReady: b[1] !== 0,
    errorCode: b[2],
    powerState: b[3],
    supercapMv: view.getUint16(4, true),
    resetCounter: b[6],
    flags: flagsAndVersion & 0xf0,
    protocolVersion: flagsAndVersion & 0x0f,
    emergency: Boolean(flagsAndVersion & 0x10),
    ecgActive: Boolean(flagsAndVersion & 0x20),
    wearDetected: Boolean(flagsAndVersion & 0x40),
  };
}

function decodeEcgData(input) {
  const b = input instanceof Uint8Array ? input : new Uint8Array(input);
  if (b.length !== 20) throw new Error("ECG Data must be 20 bytes");

  const view = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const samples = [];
  for (let i = 0; i < b[1]; i++) {
    samples.push(view.getInt16(2 + i * 2, true));
  }
  return { sequence: b[0], samples }; // 200 Hz
}
```

## MAX86150 (PPG/ECG): các `#define` cấu hình

MAX86150 chạy một trong hai chế độ, mỗi lần chuyển đều soft reset chip: **PPG** Red/IR (nhịp tim, SpO2) khi đo bình thường và **ECG** một đạo trình sau lệnh `0x06`. Ý nghĩa các bit thanh ghi bên dưới lấy từ datasheet MAX86150 (19-8402 Rev 2, 12/18). Sửa `#define` xong phải build lại và nạp.

### Vị trí code

| File | Nội dung |
|---|---|
| `Drivers/Sensors/MAX86150/max86150_optical.c/.h` | Driver I2C: giá trị thanh ghi của hai chế độ, đọc FIFO |
| `Application/SensorManager/sensor_manager.c` | Thuật toán nhịp tim/SpO2, chuyển PPG/ECG |
| `Application/wearable_config.h` | Tham số PPG (dòng LED, ngưỡng thuật toán) và các switch chẩn đoán ECG |
| `STM32_BLE/App/wearable_app.c` | Task đọc FIFO ECG, đóng gói và notify `ECG Data` |
| `Application/wearable_data.h` | Định dạng gói `ECG Data` (giao thức đã đóng băng) |
| `Application/SensorManager/ecg_diag.h` | Biến quan sát `g_ecgDiag` và kiểm tra phạm vi các switch chẩn đoán |

### Giá trị thanh ghi (`max86150_optical.c`)

| Macro | Giá trị | Ý nghĩa |
|---|---|---|
| `MAX86150_OPTICAL_I2C_ADDRESS` | `0x5E` | Địa chỉ I2C 7-bit (write `0xBC`, read `0xBD`) |
| `MAX86150_OPTICAL_EXPECTED_PART_ID` | `0x1E` | Giá trị thanh ghi `0xFF`, dùng để nhận diện chip |
| `MAX86150_DEFAULT_TIMEOUT_MS` | `20` | Timeout của mỗi giao dịch I2C |
| `MAX86150_RESET_TIMEOUT_MS` | `100` | Thời gian chờ tối đa bit RESET tự xóa sau soft reset |
| `MAX86150_FIFO_ROLLOVER` | `0x1F` | Thanh ghi `0x08`: `FIFO_ROLLS_ON_FULL = 1` (FIFO đầy thì ghi đè mẫu cũ), `FIFO_A_FULL = 15` |
| `MAX86150_FIFO_IR_RED_SLOTS` | `0x21` | Thanh ghi `0x09` ở chế độ PPG: FD1 = LED1 (IR), FD2 = LED2 (Red) |
| `MAX86150_PPG_CONFIG_100HZ_400US` | `0xD3` | Thanh ghi `0x0E`: `PPG_ADC_RGE = 11` (thang 32768 nA), `PPG_SR = 0100` (100 sps), `PPG_LED_PW = 11` (xung 400 µs) |
| `MAX86150_PPG_INTEGRATION_DELAY` | `0x18` | Thanh ghi `0x0F`. Datasheet Rev 2 chỉ mô tả `SMP_AVE[2:0]` (ở đây = 0, không lấy trung bình); bit 3 và 4 mà giá trị này bật **không có trong datasheet**, giữ theo driver tham chiếu |
| `MAX86150_FIFO_ECG_SLOT` | `0x09` | Thanh ghi `0x09` ở chế độ ECG: FD1 = `1001` (ECG), FD2 trống. Không slot nào dùng LED nên LED tắt |
| `MAX86150_ECG_CONFIG_200SPS` | `0x03` | Thanh ghi `0x3C`: `ECG_ADC_CLK = 0`, `ECG_ADC_OSR = 11`, xem bảng tốc độ bên dưới |
| `MAX86150_ECG_GAIN_IA9_5_PGA8` | `0x0D` | Thanh ghi `0x3E`: `PGA_ECG_GAIN` (bit 3:2) = `11` là 8, `IA_GAIN` (bit 1:0) = `01` là 9.5, tổng 76 V/V |
| `MAX86150_ECG_FIFO_DEPTH` | `32` | Độ sâu FIFO của chip (mẫu). Cố định theo phần cứng, không đổi được |

Tốc độ lấy mẫu ECG theo `0x3C` (giá trị typical trong datasheet):

| `0x3C` | Tốc độ | Băng thông lọc 70% / 90% |
|---|---|---|
| `0x00` | 1600 sps | 420 / 232 Hz |
| `0x01` | 800 sps | 210 / 116 Hz |
| `0x02` | 400 sps | 105 / 58 Hz |
| `0x03` (đang dùng) | 200 sps | 52 / 29 Hz |

Gain ECG theo `0x3E`: `IA_GAIN` `00`/`01`/`10`/`11` là 5 / 9.5 / 20 / 50 V/V; `PGA_ECG_GAIN` `00`/`01`/`10`/`11` là 1 / 2 / 4 / 8 V/V. Datasheet chỉ trim chính xác tại nhà máy cho cặp 9.5 × 8 đang dùng.

Quy đổi điện áp: `V_in = raw18 × 12.247 µV / 76`, tức **0.161 µV** mỗi LSB 18-bit và **0.645 µV** mỗi LSB `int16` trong gói `ECG Data` (vì firmware gửi `raw18 >> 2`).

> Đổi tốc độ hoặc gain ECG là đổi ý nghĩa dữ liệu mà app đang decode (app giả định 200 Hz). Tăng tốc độ còn làm FIFO 32 mẫu đầy nhanh hơn (160 ms ở 200 sps, 80 ms ở 400 sps), phải giảm chu kỳ đọc tương ứng.

### Luồng ECG (`wearable_app.c`, `wearable_data.h`)

| Macro | Mặc định | Ý nghĩa |
|---|---|---|
| `WEARABLE_ECG_DRAIN_PERIOD_MS` | `ECG_DIAG_DRAIN_PERIOD_MS` (45) | Chu kỳ timer của task đọc FIFO. Timer được nạp lại **sau khi** task chạy xong nên chu kỳ thật dài hơn vài ms (thời gian đọc I2C). Đổi giá trị trong `wearable_config.h` (chỉ có tác dụng khi `ENABLE_TEST = 1`) |
| `WEARABLE_ECG_READ_MAX_SAMPLES` | `32` | Số mẫu tối đa lấy trong một lần đọc, bằng độ sâu FIFO |
| `WEARABLE_ECG_QUEUE_LEN` | `16` | Số gói `ECG Data` giữ lại khi bộ đệm TX của BLE đầy (khoảng 0.7 s). Đầy thì bỏ gói cũ nhất, app thấy sequence nhảy cóc |
| `WEARABLE_ECG_SAMPLES_PER_PACKET` | `9` | Số mẫu mỗi gói. **Thuộc giao thức BLE, không đổi** |
| `WEARABLE_ECG_PAYLOAD_LENGTH` | `20` | Độ dài gói `ECG Data`. **Thuộc giao thức BLE, không đổi** |

Một lần đọc có thể trả về nhiều hơn 9 mẫu: firmware đóng một gói mỗi khi đủ 9 mẫu và giữ mẫu lẻ cho lần sau, không mất mẫu.

### PPG: nhịp tim và SpO2 (`wearable_config.h`, mục 5)

| Macro | Mặc định | Ý nghĩa |
|---|---|---|
| `OPTICAL_DEFAULT_LED_CURRENT_CODE` | `0x24` | Dòng LED Red và IR: 0.2 mA mỗi LSB ở thang 50 mA, tức 7.2 mA. Tăng nếu tín hiệu yếu, giảm nếu ADC bão hòa |
| `OPTICAL_DRAIN_INTERVAL_MS` | `200` | Chu kỳ đọc FIFO PPG. Ở 100 sps FIFO 32 mẫu đầy sau 320 ms |
| `OPTICAL_SAMPLE_PERIOD_MS` | `10` | Khoảng cách giữa hai mẫu, dùng để tính thời gian giữa các nhịp. Phải khớp với 100 sps của `MAX86150_PPG_CONFIG_100HZ_400US` |
| `OPTICAL_PULSE_SIGN` | `-1.0` | Chiều của xung mạch trên kênh IR. Đổi thành `1.0` nếu không bắt được nhịp |
| `OPTICAL_DC_ALPHA` | `0.05` | Hệ số lọc EMA của thành phần DC (đường nền) |
| `OPTICAL_ENVELOPE_ALPHA` | `0.03` | Hệ số lọc EMA của biên độ xung |
| `OPTICAL_THRESHOLD_HIGH_FRAC`, `OPTICAL_THRESHOLD_LOW_FRAC` | `0.5`, `0.25` | Ngưỡng phát hiện nhịp (vượt lên) và ngưỡng nhả (tụt xuống), tính theo tỉ lệ biên độ |
| `OPTICAL_MIN_ENVELOPE` | `50` | Biên độ tối thiểu (đơn vị ADC) để coi là có mạch |
| `OPTICAL_MIN_DC_FOR_VALID` | `2000` | Mức DC tối thiểu để coi là đang đeo. Thấp hơn thì SpO2 giữ giá trị cũ |
| `OPTICAL_MIN_BPM`, `OPTICAL_MAX_BPM` | `30`, `220` | Khoảng nhịp tim chấp nhận; nhịp ngoài khoảng bị bỏ |
| `OPTICAL_BEAT_HISTORY_LEN` | `4` | Số khoảng nhịp gần nhất dùng để lấy trung bình nhịp tim |
| `OPTICAL_SPO2_WINDOW_DRAINS` | `5` | Số lần đọc FIFO giữa hai lần tính SpO2 (khoảng 1 s) |
| `OPTICAL_REPROBE_INTERVAL_CALLS` | `2` | Khi MAX86150 không phản hồi, dò lại sau mỗi bao nhiêu lần chạy task sensor (mỗi lần 1 s) |

Thuật toán nhịp tim và SpO2 là heuristic, chưa hiệu chuẩn với máy đo chuẩn.

### Chẩn đoán nhiễu ECG (`wearable_config.h`, mục 1)

Dùng để tìm nguồn của nhiễu tuần hoàn khoảng 20 Hz trên tín hiệu ECG. Các switch chỉ có tác dụng khi `ENABLE_TEST = 1`; với `ENABLE_TEST = 0` firmware luôn dùng giá trị production trong bảng. Mỗi switch chỉ đổi **một** yếu tố; mỗi lần đo chỉ đổi một switch rồi so sánh log.

| Macro | Production | Ý nghĩa và cách dùng |
|---|---|---|
| `ECG_DIAG_PPG_OFF` | `0` | `1`: ghi 0 vào dòng LED1, LED2 và pilot (`0x11`, `0x12`, `0x15`) ngay sau khi cấu hình ECG. Chỉ để kiểm tra chéo, vì sau soft reset ba thanh ghi này đã là 0 |
| `ECG_DIAG_DRAIN_PERIOD_MS` | `45` | Chu kỳ đọc FIFO ECG, cho phép 10 đến 120. Ví dụ `30` hoặc `90`: nếu tần số nhiễu đổi theo thì nguồn nhiễu là lần đọc FIFO. Ở `90` FIFO dùng khoảng 61%, task trễ thêm quá khoảng 63 ms là mất mẫu |
| `ECG_DIAG_NOTIFY_EVERY_N_DRAINS` | `1` | `N` (2 đến 8): vẫn đọc FIFO theo chu kỳ cũ nhưng chỉ notify mỗi N lần đọc. Tách ảnh hưởng của notify khỏi ảnh hưởng của I2C |
| `ECG_DIAG_CONN_INTERVAL_MS` | `0` | Khác 0 (8 đến 500): khi bắt đầu ECG, xin central đổi connection interval sang giá trị này. Central có thể từ chối; kết quả nằm trong `g_ecgDiag` |
| `ECG_DIAG_DUMP_REGS` | `0` | `1`: đọc lại 13 thanh ghi MAX86150 sau khi cấu hình ECG, lưu vào `g_ecgDiag.regs`. Chỉ đọc, không ghi, nên có thể bật cùng bất kỳ switch nào khác |

Board không có UART (`printf` bị bỏ qua), nên kết quả quan sát nằm trong biến RAM `g_ecgDiag` (kiểu `ecg_diag_info_t` trong `ecg_diag.h`), luôn được build và không đổi hành vi firmware:

| Trường | Ý nghĩa |
|---|---|
| `conn_interval_1p25`, `conn_updates` | Connection interval hiện tại (đơn vị 1.25 ms) và số lần central đổi interval |
| `conn_req_status` | Kết quả lệnh xin đổi interval (`0xFF` = chưa xin) |
| `regs_valid`, `regs[13][2]` | Cặp địa chỉ và giá trị thanh ghi đọc lại (`0xEE, 0xEE` = đọc lỗi) |
| `ecg_sessions`, `ecg_overflows`, `ecg_dropped_packets` | Số phiên ECG, số lần FIFO tràn (mất mẫu mà app **không** thấy qua sequence), số gói bị bỏ khỏi hàng đợi |
| `drains`, `period_us_*` | Số lần đọc theo timer và chu kỳ thật giữa hai lần timer kích (min, max, tổng, số lần) |
| `fire_to_read_done_us_*`, `read_us_*` | Thời gian từ lúc timer kích đến lúc đọc I2C xong, và riêng thời gian đọc I2C |
| `samples_min`, `samples_max`, `samples_sum`, `queue_max` | Số mẫu mỗi lần đọc và độ sâu lớn nhất của hàng đợi gói |
| `tx_retry_drains` | Số lần đọc ngoài lịch do sự kiện TX-pool của BLE |

Thời gian đo bằng SysTick (micro giây), vì Cortex-M0+ không có DWT cycle counter. Các trường thời gian được xóa mỗi khi bắt đầu phiên ECG.

Đọc `g_ecgDiag` qua ST-LINK khi firmware đang chạy, không dừng và không reset chip. Địa chỉ và kích thước lấy ở dòng `.bss.g_ecgDiag` trong `STM32CubeIDE/Debug/BLE_p2pServer_GATT.map`:

```bash
STM32_Programmer_CLI -c port=SWD mode=HOTPLUG -r8 <địa chỉ> <kích thước>
```

Script `STM32CubeIDE/diag_builds/read_ecg_diag.py` làm việc này tự động (tra địa chỉ trong map, đọc qua SWD, giải mã từng trường): `python read_ecg_diag.py [đường dẫn file .map]`, mặc định dùng map trong `Debug/`.

Phải đọc **trước khi** nạp lại hoặc tắt nguồn, vì reset xóa RAM.

## Lưu trữ trên ST25DV04K

Cấu hình và log nằm trong bộ nhớ người dùng của tag (512 byte, địa chỉ I2C `0x0000`–`0x01FF`), truy cập qua `NFC_IO_ReadUserMemory/WriteUserMemory` (`Application/NFC/nfc_io.c`). Mọi cấu trúc được encode từng byte, little-endian; CRC là CRC-16/MODBUS của các byte đứng trước nó. Phần này đã biên dịch và chạy thử trên PC với tag giả lập, **chưa chạy trên phần cứng**.

| Địa chỉ | Kích thước | Nội dung |
|---|---:|---|
| `0x0000`–`0x001F` | 32 | Cấu hình: các trường ở byte `0–14`, CRC ở byte `30–31` |
| `0x0020`–`0x002F` | 16 | Header log: CRC ở byte `14–15` |
| `0x0030`–`0x003F` | 16 | Không dùng |
| `0x0040`–`0x01EF` | 432 | 18 record × 24 byte, bộ đệm vòng |
| `0x01F0`–`0x01FF` | 16 | Không dùng |

Mỗi record 24 byte trùng từng byte với gói `Recovery Data`: sequence (`uint16`, byte `0–1`, do `NFC_Log_Add` gán từ `1`), timestamp (`uint32`, `2–5`), sensor payload 16 byte (`6–21`), CRC của byte `0–21` (`22–23`).

Layout chi tiết, lệnh mailbox và các hạn chế còn lại (mailbox cần `MB_MODE = 1` trên tag, `Get log` chỉ trả 10 record, cấu hình chiếm chỗ của CC file NDEF) nằm ở mục 16.1 của [DEVICE_WEBAPP_PROTOCOL.md](DEVICE_WEBAPP_PROTOCOL.md).

## LoRaWAN (LoRa Basics Modem)

Thiết bị chạy LoRaWAN **OTAA, Class A** trên SX1262 bằng thư viện [LoRa Basics Modem](https://github.com/Lora-net/SWL2001) (LBM) v4.9.0 của Semtech, tương ứng LoRaWAN L2 1.0.4 và Regional Parameters RP002-1.0.3. Vùng tần số đang dùng: **AS923-2**.

> **LoRaWAN không tự chạy.** SX1262 bị giữ ở reset cho tới khi webapp gửi lệnh BLE LoRa join (`0x0F`, màn hình *LoRa* của webapp). Sau khi join, mặc định **không** có uplink định kỳ (`LBM_UPLINK_PERIOD_S = 0`): uplink chỉ đi theo lệnh `0x10` hoặc khi nhấn nút SOS. Công suất phát bị giới hạn ở `0` dBm (`LBM_TX_POWER_MAX_DBM`, đổi lúc chạy bằng `0x12`). Build với `LBM_APP_ENABLE = 0` (trong `Application/wearable_config.h`) thì bỏ hẳn LoRa: các lệnh LoRa trả "not built".

### Vị trí code

| Đường dẫn | Nội dung |
|---|---|
| `Application/wearable_config.h` (mục 6) | **Toàn bộ thông số cấu hình** (xem bảng bên dưới) |
| `Application/LoRaWAN/lbm_config.h` | Gom cấu hình từ `wearable_config.h` và key từ `lbm_credentials.h` cho port LBM |
| `Application/LoRaWAN/lbm_credentials.h` | DevEUI, JoinEUI, AppKey. Bản trong Git là mẫu toàn số 0, không commit key thật |
| `Application/LoRaWAN/lbm_app.c` | Luồng ứng dụng: join, uplink định kỳ, nút SOS |
| `Application/LoRaWAN/*_wb09.c` | Port cho WB09: timer, SPI3 với SX1262, cấu hình TCXO/PA |
| `ThirdParty/LBM/lbm_lib/` | Thư viện LBM của Semtech, không sửa |
| `Application/LoRaTest/` | Chương trình test radio độc lập, bật bằng `ENABLE_TEST = 1` và `LORA_TEST_ENABLE = 1` trong `wearable_config.h` (mặc định tắt, cần `LBM_APP_ENABLE = 0`) |

Chân kết nối SX1262: SPI3 (SCK PB3, MISO PA8, MOSI PA11), NSS PA9, NRESET PB15, BUSY PB14, DIO1 PA1.

### Chuẩn bị network server (The Things Stack)

1. Tạo device với LoRaWAN Specification **1.0.4**, Regional Parameters **RP002 1.0.3**, frequency plan **AS923-2**, kích hoạt OTAA.
2. **DevEUI** và **AppKey**: bấm *Generate* trên console. **JoinEUI** có thể để toàn số 0.
3. Điền vào `lbm_credentials.h` theo đúng thứ tự byte console hiển thị (MSB trước). Nếu DevEUI hoặc AppKey còn toàn 0, firmware không join (`g_lbmState = LBM_STATE_NO_CREDENTIALS`).
4. Khi phát triển, bật **Resets join nonces** cho device. Context của LBM đang lưu trong RAM `.noinit` nên mất khi cắt nguồn hoặc khi build lại (DevNonce về 0, server báo *DevNonce too small*). Tùy chọn này tắt cơ chế chống phát lại, **chỉ dùng khi test**.
5. Gateway phải chạy plan AS923-2 (kênh join mặc định 921.4 và 921.6 MHz).

### Cấu hình thông số

Sửa trong `Application/wearable_config.h` (mục 6), sau đó build lại và nạp.

| Macro | Mặc định | Ý nghĩa |
|---|---|---|
| `LBM_APP_ENABLE` | `1` | `0`: không có LoRa, SX1262 giữ ở reset. `1`: có LoRa, nhưng chỉ khởi động khi nhận lệnh BLE `0x0F` |
| `LBM_TX_POWER_MAX_DBM` | `0` | Giới hạn công suất phát của SX1262 (dBm, `-9`..`22`), áp sau yêu cầu của vùng (AS923 xin 14 dBm). Dòng phát xấp xỉ: 41 mA ở 0 dBm, 54 mA ở 5 dBm, 89 mA ở 14 dBm. Đổi lúc chạy bằng lệnh `0x12` |
| `LBM_REGION` | `SMTC_MODEM_REGION_AS_923_GRP2` | Nhóm tần số. AS923-1 là `..._GRP1`, AS923-3 là `..._GRP3` (đồng thời gateway phải khớp) |
| `LBM_UPLINK_PORT` | `101` | Cổng của uplink định kỳ |
| `LBM_UPLINK_PERIOD_S` | `0` | Chu kỳ uplink định kỳ (giây). `0` = không có uplink định kỳ, kể cả gói ngay sau khi join |
| `LBM_FIRST_UPLINK_DELAY_S` | `10` | Trễ của gói định kỳ đầu tiên sau khi join (gói ngay khi join xong gửi riêng). Chỉ dùng khi `LBM_UPLINK_PERIOD_S` > 0 |
| `LBM_ADR_MODE` | `LBM_ADR_NETWORK_CONTROLLED` | Cách chọn data rate, xem bên dưới |
| `LBM_FIXED_DR` | `2` | DR cố định, chỉ dùng khi `LBM_ADR_MODE = LBM_ADR_FIXED_DR` |
| `LBM_NB_TRANS` | `1` | Số lần phát mỗi uplink (1 đến 15). Bị bỏ qua khi ADR do server điều khiển |
| `LBM_JOIN_DR` | `-1` | Data rate của các gói **join**. `-1` giữ mặc định của LBM (trộn DR2 đến DR5), `0` đến `7` ép một DR |
| `LBM_TX_POWER_OFFSET_DB` | `0` | Cộng thêm vào công suất trước khi tra bảng PA (bù suy hao antenna). Chip giới hạn -9 đến +22 dBm |
| `LBM_RADIO_USE_TCXO`, `LBM_RADIO_TCXO_VOLTAGE_REG`, `LBM_RADIO_TCXO_STARTUP_MS` | `1`, `0x02` (1.8 V), `5` | TCXO cấp nguồn từ DIO3. Đặt `LBM_RADIO_USE_TCXO = 0` nếu dùng thạch anh thường |
| `LBM_RADIO_USE_DCDC`, `LBM_RADIO_USE_DIO2_RF_SWITCH` | `1`, `1` | Kế thừa từ module E22, **chưa xác nhận** trên board custom: DC-DC cần cuộn cảm đã hàn, DIO2 phải thật sự điều khiển switch antenna |
| `LBM_SOS_PORT`, `LBM_SOS_CONFIRMED`, `LBM_SOS_DEBOUNCE_MS` | `102`, `true`, `300` | Cổng, kiểu gói và thời gian chống rung của nút SOS |

#### Data rate và spreading factor (AS923)

| DR | Điều chế | Payload ứng dụng tối đa* |
|---|---|---|
| DR0 | SF12, 125 kHz | không dùng để phát (dwell time) |
| DR1 | SF11, 125 kHz | không dùng để phát (dwell time) |
| DR2 | SF10, 125 kHz | 11 byte |
| DR3 | SF9, 125 kHz | 53 byte |
| DR4 | SF8, 125 kHz | 125 byte |
| DR5 | SF7, 125 kHz | 242 byte |
| DR6 | SF7, 250 kHz | 242 byte |
| DR7 | FSK 50 kbit/s | 242 byte |

\* Khi dwell time uplink bật (mặc định của LBM cho AS923), lấy từ bảng MACPayload tối đa trong code LBM trừ 8 byte header (FHDR 7 và FPort 1); nếu có thêm MAC command thì còn ít hơn. Payload hiện tại chỉ 4 đến 5 byte nên đủ ở mọi DR được phép.

DR thấp (SF cao) đi xa hơn nhưng chiếm kênh lâu hơn, mỗi bậc SF làm thời gian phát tăng khoảng gấp đôi.

| `LBM_ADR_MODE` | Hành vi |
|---|---|
| `LBM_ADR_NETWORK_CONTROLLED` | Server điều khiển DR và công suất. Phù hợp thiết bị đứng yên |
| `LBM_ADR_MOBILE_LONG_RANGE` | Profile có sẵn của LBM, ưu tiên tầm xa, cho thiết bị di chuyển |
| `LBM_ADR_MOBILE_LOW_POWER` | Profile có sẵn của LBM, ưu tiên thời gian phát ngắn, cho thiết bị di chuyển |
| `LBM_ADR_FIXED_DR` | Không ADR, luôn dùng `LBM_FIXED_DR` |

Ví dụ:

```c
/* Luôn SF10 (DR2), phát 1 lần mỗi gói */
#define LBM_ADR_MODE   LBM_ADR_FIXED_DR
#define LBM_FIXED_DR   2
#define LBM_NB_TRANS   1

/* Luôn SF7 (DR5), phát lặp 2 lần mỗi gói để tăng độ tin cậy */
#define LBM_ADR_MODE   LBM_ADR_FIXED_DR
#define LBM_FIXED_DR   5
#define LBM_NB_TRANS   2

/* Chỉ join bằng SF10 */
#define LBM_JOIN_DR    2
```

ADR profile và NbTrans chỉ áp dụng được sau khi join, firmware tự gọi khi nhận sự kiện JOINED. Công suất tối đa của vùng AS923 là 16 dBm EIRP. Cố định công suất bên ngoài cơ chế ADR chưa được hỗ trợ.

### Nút SOS (PB5)

Nối một nút nhấn giữa PB5 và GND (chân đã cấu hình pull-up nội và ngắt cạnh xuống). Nhấn nút sẽ gửi **một uplink khẩn cấp** (`smtc_modem_request_emergency_uplink`): ưu tiên cao hơn mọi dịch vụ khác và không bị giới hạn duty cycle.

- Chống rung 300 ms (`LBM_SOS_DEBOUNCE_MS`), nhấn đôi trong khoảng đó chỉ tính một lần.
- Nếu nhấn khi chưa join xong, yêu cầu được giữ lại và gửi ngay sau khi join.
- Gói mặc định là *confirmed*: server phải trả ACK, nếu không LBM tự phát lại theo cơ chế của nó (số lần cụ thể chưa được kiểm tra).
- Nút này **chưa** kích hoạt trạng thái EMERGENCY hay notification qua BLE, hiện chỉ gửi qua LoRa.

### Định dạng payload

| Cổng | Khi nào | Nội dung |
|---|---|---|
| 101 | Lệnh BLE `0x10`, hoặc định kỳ nếu `LBM_UPLINK_PERIOD_S` > 0; không xác nhận | 4 byte: bộ đếm uplink, big-endian, gói đầu tiên là 0 |
| 102 | Nhấn nút SOS, confirmed | 5 byte: `0x01` (SOS) rồi 4 byte số lần nhấn từ khi khởi động, big-endian |

Payload formatter cho The Things Stack (*Payload formatters* → *Uplink* → *Custom Javascript formatter*):

```javascript
function decodeUplink(input) {
  var b = input.bytes;
  var port = input.fPort;

  function u32(i) {
    return ((b[i] << 24) | (b[i + 1] << 16) | (b[i + 2] << 8) | b[i + 3]) >>> 0;
  }

  if (port === 101) {
    if (b.length !== 4) {
      return { data: {}, warnings: [], errors: ["port 101 expects 4 bytes, got " + b.length] };
    }
    return { data: { counter: u32(0) }, warnings: [], errors: [] };
  }

  if (port === 102) {
    if (b.length !== 5 || b[0] !== 0x01) {
      return { data: {}, warnings: [], errors: ["port 102 expects 5 bytes starting with 0x01"] };
    }
    return { data: { sos: true, presses: u32(1) }, warnings: [], errors: [] };
  }

  return { data: {}, warnings: ["unknown fPort " + port], errors: [] };
}
```

### Theo dõi khi debug

Firmware không có kênh log (PA1 là DIO1, không phải UART), nên đọc trạng thái bằng **Live Expressions** trong STM32CubeIDE:

| Biến | Ý nghĩa |
|---|---|
| `g_lbmState` | 0 chưa chạy, 1 thiếu credentials, 2 đang join, 3 đã join, 4 join lỗi, 5 lỗi API (xem `g_lbmLastRc`) |
| `g_lbmLastEvent`, `g_lbmEventCount` | Sự kiện LBM gần nhất (0 RESET, 1 ALARM, 2 JOINED, 3 TXDONE, 4 DOWNDATA, 5 JOINFAIL) và tổng số |
| `g_lbmUplinkCount`, `g_lbmTxDoneStatus` | Số uplink định kỳ đã yêu cầu, kết quả TxDone gần nhất (0 chưa gửi, 1 đã gửi, 2 có ACK) |
| `g_lbmSosPressCount`, `g_lbmSosSentCount`, `g_lbmSosPending`, `g_lbmSosLastRc` | Số lần nhấn, số gói SOS đã gửi, còn chờ gửi, mã lỗi gần nhất |
| `g_lbmRadioIrqCount`, `g_lbmDio1PollEdges` | Số ngắt DIO1 nhận được qua EXTI và qua poll 1 ms |
| `g_lbmDiag` | Nằm trong RAM `.noinit` nên **sống sót qua reset**: số lần khởi động, số lần LBM panic và nội dung panic, lỗi SPI/BUSY của radio, 9 byte chip trả về ở lần đọc đầu tiên. Dùng để phát hiện vòng reset |

Khi debug đừng đặt breakpoint trên đường chạy của LBM: CPU dừng thì các cửa sổ RX của LoRaWAN cũng lỡ. Nếu cần, chỉ đặt ở `case SMTC_MODEM_EVENT_JOINED` hoặc `JOINFAIL` trong `lbm_app.c`.

### Giới hạn đã biết

- Context LBM (trong đó có DevNonce) chưa lưu vào flash, chỉ nằm trong RAM `.noinit`. Cần bật *Resets join nonces* trên server khi test, xem mục chuẩn bị ở trên.
- Timer của LBM chạy trên SysTick 1 ms nên CPU không vào Stop/Off mode (`CFG_LPM_LBM`, và `PWR_EnterSleepMode` không còn dừng SysTick). `main()` giữ nguyên giới hạn này cả khi LoRaWAN tắt, vì `DeviceTime` cũng đếm bằng SysTick. Đây là cấu hình bring-up, tốn điện hơn thiết kế cuối.
- BLE và LoRaWAN chưa được kiểm chứng chạy đồng thời lâu dài; ngắt radio BLE có thể làm lệch cửa sổ RX của LoRaWAN. Task LBM chạy cùng mức ưu tiên với BLE stack (sequencer luân phiên) và chờ BUSY của SX1262 tối đa 100 ms, để LoRa không chặn BLE host.
- Ở LDO 2.4 V của NEH7100, khi LoRa join ở 14 dBm thì BLE mất kết nối; ở điện áp cao hơn thì không. Nguyên nhân phần cứng chưa xác định, vì vậy công suất mặc định được hạ xuống 0 dBm.
- Board custom: clock SX1262 dùng TCXO gắn thêm. Với thạch anh thiết kế ban đầu, chip không hoàn tất khởi động trên board này (nguyên nhân trong mạch XTAL chưa xác định).

## Kết nối nhanh bằng nRF Connect

1. Flash firmware và reset board.
2. Quét, tìm thiết bị `BLEWearable` và kết nối.
3. Mở service `0000FE40-CC7A-482A-984A-7F2ED5B3E58F`.
4. Bật notification cho `Sensor Data` và `Device Status`.
5. Ghi `01 00 00 00 00 00 00 00` vào `Control` để bắt đầu đo.
6. Decode notification theo các bảng byte layout ở trên.

## Build và flash

1. Mở STM32CubeIDE.
2. Import project từ thư mục `STM32CubeIDE`.
3. Build cấu hình `Debug` hoặc `Release`.
4. Flash qua ST-LINK và theo dõi log debug nếu cần.

> Lưu ý: các thư mục output build được loại khỏi Git bằng `.gitignore`.

### Cấu hình build (`Application/wearable_config.h`)

Mọi tham số build mà người dùng có thể chỉnh nằm trong **một file duy nhất**, mỗi macro có comment giải thích:

| Mục | Nội dung |
|---|---|
| 1. Tests and diagnostics | `ENABLE_TEST` và mọi switch test/chẩn đoán (`LORA_TEST_ENABLE`, `ECG_DIAG_*`), thông số radio của LoRaTest |
| 2. Features | `LBM_APP_ENABLE` |
| 3. Power | Giá trị thanh ghi NEH7100, cầu phân áp supercap, ngưỡng power policy |
| 4. BLE application | `WEARABLE_SENSOR_PERIOD_MS` |
| 5. Sensors | Thời gian đo nhiệt độ, chu kỳ dò lại cảm biến, ngưỡng phát hiện đeo QVar, tham số PPG |
| 6. LoRaWAN | Vùng tần số, uplink, data rate/ADR, SOS, front-end radio |

`ENABLE_TEST` là công tắc tổng: `0` (mặc định, bản production) thì mọi test bị bỏ khỏi bản build và mọi switch chẩn đoán bị ép về giá trị production, bất kể khối test ghi gì. Chỉ khi `ENABLE_TEST = 1` thì các giá trị trong khối `#if ENABLE_TEST` mới có tác dụng.

Không nằm trong file này (có chủ đích): địa chỉ/giá trị thanh ghi của chip, định dạng gói BLE/NFC (app đang decode), cấu hình STM32CubeMX (`Core/Inc/app_conf.h`) và key LoRaWAN (`lbm_credentials.h`).

## Cập nhật gần đây
- Gom toàn bộ tham số build vào `Application/wearable_config.h`, thêm công tắc tổng `ENABLE_TEST` cho mọi test/chẩn đoán. Mã máy của bản production không đổi.
- LoRaWAN: thêm lệnh BLE `0x0F`..`0x13` (join, test uplink, status, TX power, stop) và gói LoRa status `0x20` trên `FE46`; LoRa chỉ chạy khi có lệnh join; bỏ uplink định kỳ mặc định; giới hạn công suất phát ở 0 dBm; task LBM hạ xuống cùng mức ưu tiên với BLE, timeout BUSY từ 1 s xuống 100 ms. Webapp có màn hình *LoRa* để test.
- Thứ tự khởi động mới: (1) cấu hình PMIC NEH7100 qua I2C trước mọi thứ khác, kể cả radio; (2) khởi tạo BLE và advertising; (3) cảm biến (MAX30208, MAX86150, LIS2DUXS12TR, ADC supercap) chỉ được dò và cấu hình **sau lần kết nối BLE đầu tiên kể từ khi reset**, rồi chờ lệnh `0x01`/`0x06` mới đo; kết nối lại không khởi tạo lại. LoRaWAN mặc định tắt (`LBM_APP_ENABLE`).
- Tối ưu bộ nhớ: Tăng Stack size lên 6KB chuẩn bị cho các thuật toán xử lý dữ liệu phức tạp (PPG, ECG).
- Khắc phục lỗi sinh code của STM32CubeMX: Xử lý triệt để các lỗi ghi đè cấu hình GATT, lỗi thiếu biến ADC, và lỗi khai báo của thư viện BLE stack (BLEPLAT_CNTR_IsEnabledTimer1).
- Tích hợp LoRa Basics Modem v4.9.0 trên SX1262: OTAA AS923-2 với The Things Stack, uplink định kỳ, cấu hình data rate/SF/ADR trong `lbm_config.h`.
- Thêm nút SOS ở PB5: nhấn nút gửi uplink LoRaWAN khẩn cấp (cổng 102, confirmed).
- MAX86150: đối chiếu giá trị thanh ghi ECG với datasheet, thêm các switch chẩn đoán nhiễu ECG trong `ecg_diag.h` và biến `g_ecgDiag` đọc qua SWD (board không có UART).
- ST25DV04K: sửa phần lưu cấu hình và log (ghi nhầm vào vùng cấu hình hệ thống thay vì bộ nhớ người dùng, layout chồng lấn, CRC sai phạm vi, tràn bộ đệm `Get log`, vượt 512 byte); chưa kiểm chứng trên phần cứng.
