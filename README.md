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
- Đã tích hợp SX1262 với LoRa Basics Modem: OTAA trên The Things Stack (AS923-2), uplink định kỳ và uplink khẩn cấp khi nhấn nút SOS (PB5). Xem mục [LoRaWAN](#lorawan-lora-basics-modem).

## Phần cứng và cảm biến

| Thành phần | Vai trò |
|---|---|
| STM32WB09KE | MCU và BLE peripheral |
| MAX86150 | Đo PPG (nhịp tim/SpO2) và ECG |
| MAX30208 | Cảm biến nhiệt độ |
| LIS2DUXS12TR | Đo gia tốc và xử lý Machine Learning Core (MLC); thay thế ST1VAFE3BX |
| NEH7100 | Energy-harvesting PMIC; quản lý nguồn và theo dõi dòng điện qua I2C |
| Supercapacitor monitor | Theo dõi điện áp nguồn |
| ST25DV04K | Dynamic NFC/RFID Tag; lưu trữ cấu hình thiết bị, log cảm biến, và hỗ trợ Energy Harvesting |
| SX1262 | LoRa transceiver chạy LoRaWAN (LoRa Basics Modem) qua SPI3; clock từ TCXO cấp nguồn bởi DIO3 |

> **Trạng thái tích hợp:** NEH7100 đã có source tại `Application/neh7100.cpp` và `Application/neh7100.h`. ST25DV04K đã được tích hợp đầy đủ driver và logic xử lý (config, logger, FTM mailbox). SX1262 đã chạy LoRaWAN OTAA bằng LoRa Basics Modem (`ThirdParty/LBM`, phần port cho WB09 ở `Application/LoRaWAN`), xem mục LoRaWAN bên dưới.

> **Migration cảm biến:** Driver register, HAL I2C, đọc gia tốc và khung nạp MLC cho LIS2DUXS12TR đã được tích hợp; cần thêm file UCF sinh từ Unico và bảng ánh xạ class để kích hoạt model MLC thực tế. MAX86150 hiện vẫn ở chế độ optical-only, vì vậy phần thu nhận ECG cần được bổ sung tiếp.

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

Để nhận notification, BLE central phải ghi `0x0001` vào CCCD của `Sensor Data` hoặc `Device Status`.

## Control commands

Ghi payload 8 byte vào characteristic `Control`. Byte `0` là command; byte `1..7` hiện chưa sử dụng và nên đặt bằng `0`.

| Byte 0 | Command | Tác dụng |
|---:|---|---|
| `0x01` | Start measurement | Bắt đầu đo và gửi dữ liệu định kỳ |
| `0x02` | Stop measurement | Dừng phép đo, chuyển về Idle |
| `0x03` | Request data | Yêu cầu cập nhật/gửi dữ liệu hiện tại |
| `0x04` | Normal mode | Đặt `power_state = 1` |
| `0x05` | Low-power mode | Dừng đo, đặt `power_state = 2` |
| `0x06` | ECG start | Bắt đầu ECG và bật cờ ECG |
| `0x07` | ECG stop | Dừng ECG và xóa cờ ECG |
| `0x08` | Emergency test | Bật cờ emergency và chuyển sang Emergency |
| `0x09` | Sync time | Đồng bộ thời gian thực (Unix time) cho thiết bị |

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

Ví dụ (Đồng bộ thời gian Unix `1724833200` = `0x66CE9DB0`, `500` ms = `0x01F4`):
```text
09 B0 9D CE 66 F4 01 00
```

## Decode Sensor Data

Payload dài **16 byte**. Các số nhiều byte dùng **little-endian**.

| Offset | Kích thước | Kiểu | Trường | Cách decode/đơn vị |
|---:|---:|---|---|---|
| 0 | 1 | `uint8` | Heart rate | bpm |
| 1 | 1 | `uint8` | SpO2 | % |
| 2 | 2 | `int16 LE` | Temperature | `raw / 100.0` °C; `-32768` nghĩa là không hợp lệ |
| 4 | 2 | `uint16 LE` | Supercapacitor voltage | mV |
| 6 | 1 | `uint8` | Power state | `1` = Normal, `2` = Low power |
| 7 | 1 | Bit field | Flags | Xem bảng flags bên dưới |
| 8 | 2 | `int16 LE` | Gia tốc X | mg |
| 10 | 2 | `int16 LE` | Gia tốc Y | mg |
| 12 | 2 | `int16 LE` | Gia tốc Z | mg |
| 14 | 2 | `int16 LE` | QVar raw | Giá trị thô kênh electrometer (AH_QVAR) của LIS2DUXS12TR; cùng đơn vị/thang đo dùng để so ngưỡng cờ `Wear detected` (`0x40`) bên dưới. Không phải điện áp, là LSB thô từ cảm biến |

### Sensor/status flags

| Mask | Ý nghĩa |
|---:|---|
| `0x08` | Fall candidate – phát hiện nguy cơ té ngã |
| `0x10` | Emergency đang bật |
| `0x20` | ECG đang hoạt động |
| `0x40` | Wear detected (QVar) – Trạng thái đang đeo |

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

| Offset | Kích thước | Kiểu | Trường | Giá trị |
|---:|---:|---|---|---|
| 0 | 1 | `uint8` | Measurement state | Xem bảng state |
| 1 | 1 | `uint8` | Sensor ready | `0` = chưa sẵn sàng, `1` = sẵn sàng |
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

### Byte flags/version

```text
bit 7 6 5 4 | bit 3 2 1 0
    flags   | protocol version
```

- `flags = payload[7] & 0xF0`
- `protocolVersion = payload[7] & 0x0F`
- Phiên bản protocol hiện tại: **1**
- Do status chỉ giữ nibble cao, cờ `Fall candidate (0x08)` chỉ xuất hiện đầy đủ trong `Sensor Data`, không xuất hiện trong byte status hiện tại.

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
  };
}
```

## LoRaWAN (LoRa Basics Modem)

Thiết bị chạy LoRaWAN **OTAA, Class A** trên SX1262 bằng thư viện [LoRa Basics Modem](https://github.com/Lora-net/SWL2001) (LBM) v4.9.0 của Semtech, tương ứng LoRaWAN L2 1.0.4 và Regional Parameters RP002-1.0.3. Vùng tần số đang dùng: **AS923-2**.

### Vị trí code

| Đường dẫn | Nội dung |
|---|---|
| `Application/LoRaWAN/lbm_config.h` | **Toàn bộ thông số cấu hình** (xem bảng bên dưới) |
| `Application/LoRaWAN/lbm_credentials.h` | DevEUI, JoinEUI, AppKey. Bản trong Git là mẫu toàn số 0, không commit key thật |
| `Application/LoRaWAN/lbm_app.c` | Luồng ứng dụng: join, uplink định kỳ, nút SOS |
| `Application/LoRaWAN/*_wb09.c` | Port cho WB09: timer, SPI3 với SX1262, cấu hình TCXO/PA |
| `ThirdParty/LBM/lbm_lib/` | Thư viện LBM của Semtech, không sửa |
| `Application/LoRaTest/` | Chương trình test radio độc lập, bật bằng `LORA_TEST_ENABLE` trong `lora_test.h` (mặc định tắt, không chạy cùng LBM) |

Chân kết nối SX1262: SPI3 (SCK PB3, MISO PA8, MOSI PA11), NSS PA9, NRESET PB15, BUSY PB14, DIO1 PA1.

### Chuẩn bị network server (The Things Stack)

1. Tạo device với LoRaWAN Specification **1.0.4**, Regional Parameters **RP002 1.0.3**, frequency plan **AS923-2**, kích hoạt OTAA.
2. **DevEUI** và **AppKey**: bấm *Generate* trên console. **JoinEUI** có thể để toàn số 0.
3. Điền vào `lbm_credentials.h` theo đúng thứ tự byte console hiển thị (MSB trước). Nếu DevEUI hoặc AppKey còn toàn 0, firmware không join (`g_lbmState = LBM_STATE_NO_CREDENTIALS`).
4. Khi phát triển, bật **Resets join nonces** cho device. Context của LBM đang lưu trong RAM `.noinit` nên mất khi cắt nguồn hoặc khi build lại (DevNonce về 0, server báo *DevNonce too small*). Tùy chọn này tắt cơ chế chống phát lại, **chỉ dùng khi test**.
5. Gateway phải chạy plan AS923-2 (kênh join mặc định 921.4 và 921.6 MHz).

### Cấu hình thông số

Sửa trong `Application/LoRaWAN/lbm_config.h`, sau đó build lại và nạp.

| Macro | Mặc định | Ý nghĩa |
|---|---|---|
| `LBM_REGION` | `SMTC_MODEM_REGION_AS_923_GRP2` | Nhóm tần số. AS923-1 là `..._GRP1`, AS923-3 là `..._GRP3` (đồng thời gateway phải khớp) |
| `LBM_UPLINK_PORT` | `101` | Cổng của uplink định kỳ |
| `LBM_UPLINK_PERIOD_S` | `60` | Chu kỳ gửi uplink định kỳ (giây) |
| `LBM_FIRST_UPLINK_DELAY_S` | `10` | Trễ của gói định kỳ đầu tiên sau khi join (gói ngay khi join xong gửi riêng) |
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
| 101 | Định kỳ, không xác nhận | 4 byte: bộ đếm uplink, big-endian, gói đầu tiên là 0 |
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
- Timer của LBM chạy trên SysTick 1 ms nên CPU không vào Stop/Off mode khi LBM chạy (`CFG_LPM_LBM`, và `PWR_EnterSleepMode` không còn dừng SysTick). Đây là cấu hình bring-up, tốn điện hơn thiết kế cuối.
- BLE và LoRaWAN chưa được kiểm chứng chạy đồng thời lâu dài; ngắt radio BLE có thể làm lệch cửa sổ RX của LoRaWAN.
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

## Cập nhật gần đây
- Tối ưu bộ nhớ: Tăng Stack size lên 6KB chuẩn bị cho các thuật toán xử lý dữ liệu phức tạp (PPG, ECG).
- Khắc phục lỗi sinh code của STM32CubeMX: Xử lý triệt để các lỗi ghi đè cấu hình GATT, lỗi thiếu biến ADC, và lỗi khai báo của thư viện BLE stack (BLEPLAT_CNTR_IsEnabledTimer1).
- Tích hợp LoRa Basics Modem v4.9.0 trên SX1262: OTAA AS923-2 với The Things Stack, uplink định kỳ, cấu hình data rate/SF/ADR trong `lbm_config.h`.
- Thêm nút SOS ở PB5: nhấn nút gửi uplink LoRaWAN khẩn cấp (cổng 102, confirmed).
