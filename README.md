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
- Dự kiến tích hợp SX1262 để truyền dữ liệu tầm xa qua LoRa/LoRaWAN.

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
| SX1262 *(planned)* | LoRa transceiver cho kết nối LoRa/LoRaWAN; driver và stack chưa được tích hợp |

> **Trạng thái tích hợp:** NEH7100 đã có source tại `Application/neh7100.cpp` và `Application/neh7100.h`. ST25DV04K đã được tích hợp đầy đủ driver và logic xử lý (config, logger, FTM mailbox). SX1262 hiện mới nằm trong kế hoạch phần cứng, driver SX1262 và LoRaWAN stack sẽ được bổ sung sau.

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
├── LoRaWAN/                     # Planned: SX1262 driver và LoRaWAN stack
└── BLE_p2pServer_GATT.ioc       # Cấu hình STM32CubeMX
```

Thư mục `LoRaWAN/` trong sơ đồ thể hiện kiến trúc dự kiến và chưa tồn tại trong source tree hiện tại.

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
| 14 | 2 | — | Reserved | Hiện bằng `0` |

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
