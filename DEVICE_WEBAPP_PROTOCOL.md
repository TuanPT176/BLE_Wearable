# Giao thức thiết bị ↔ WebApp — BLE Wearable (protocol v1)

Tài liệu này quy ước toàn bộ dữ liệu trao đổi giữa thiết bị wearable (firmware `BLE_Wearable_GATT`, STM32WB09KE) và webapp (`WearableWebApp`, Web Bluetooth). Đây là **mốc chung** cho cả hai phía: khi code và tài liệu lệch nhau thì phải sửa một trong hai ngay trong cùng thay đổi đó.

| Mục | Giá trị |
|---|---|
| Phiên bản giao thức | `1` (`WEARABLE_PROTOCOL_VERSION`, nằm ở nibble thấp byte 7 của `DEVICE_STATUS`) |
| Ngày đối chiếu mã nguồn | 2026-10-01 (firmware) |
| Firmware | `BLE_Wearable_GATT`, nhánh `main`, commit `fbac3a2` cộng working tree (bộ phát hiện đeo bằng QVar chưa commit) |
| WebApp | `WearableWebApp`, nhánh `main`, commit `d97e660` cộng working tree |
| Tài liệu liên quan | `README.md` (firmware, mục *BLE GATT profile*), `BLE_PROTOCOL.md` (webapp). Ba tài liệu phải khớp nhau; tài liệu này là bản đầy đủ nhất |

Nhãn trạng thái dùng trong tài liệu:

| Nhãn | Ý nghĩa |
|---|---|
| **[CHỐT]** | Đã triển khai ở firmware, webapp đang dùng. Không đổi UUID, offset, mã lệnh |
| **[DỞ]** | Có code ở firmware nhưng chưa chạy end-to-end. Layout có thể còn đổi |
| **[STUB]** | Chỉ có khai báo trong GATT hoặc struct, chưa có logic. Layout chưa chốt |

## Mục lục

1. [Các kênh giao tiếp](#1-các-kênh-giao-tiếp)
2. [Quy ước chung](#2-quy-ước-chung)
3. [Tầng kết nối BLE (GAP)](#3-tầng-kết-nối-ble-gap)
4. [GATT: service và characteristic](#4-gatt-service-và-characteristic)
5. [CONTROL `FE41`: lệnh từ app xuống thiết bị](#5-control-fe41-lệnh-từ-app-xuống-thiết-bị)
6. [SENSOR_DATA `FE42`](#6-sensor_data-fe42)
7. [DEVICE_STATUS `FE43`](#7-device_status-fe43)
8. [ECG_DATA `FE45`](#8-ecg_data-fe45)
9. [NFC_DATA `FE44`](#9-nfc_data-fe44)
10. [DEBUG_DATA `FE46`](#10-debug_data-fe46)
11. [RECOVERY_DATA `FE47`](#11-recovery_data-fe47)
12. [Máy trạng thái thiết bị](#12-máy-trạng-thái-thiết-bị)
13. [Luồng sử dụng chuẩn](#13-luồng-sử-dụng-chuẩn)
14. [Quy tắc xử lý phía webapp](#14-quy-tắc-xử-lý-phía-webapp)
15. [Test vector](#15-test-vector)
16. [Kênh phụ: NFC mailbox và LoRaWAN](#16-kênh-phụ-nfc-mailbox-và-lorawan)
17. [Hạn chế và điểm lệch đã biết](#17-hạn-chế-và-điểm-lệch-đã-biết)
18. [Quy tắc thay đổi giao thức](#18-quy-tắc-thay-đổi-giao-thức)
19. [Bảng đối chiếu mã nguồn](#19-bảng-đối-chiếu-mã-nguồn)

---

## 1. Các kênh giao tiếp

```text
                 BLE GATT (kênh chính, hai chiều)
  WebApp  <------------------------------------------>  Wearable
 (Chrome,          lệnh: CONTROL (write)                (STM32WB09,
  Web Bluetooth)   dữ liệu: notify 1 Hz / 200 Hz ECG     peripheral)
                                                            |
  App native / đầu đọc NFC  <--- ST25DV mailbox (FTM) ------+   [DỞ]
  The Things Stack          <--- LoRaWAN uplink (SX1262) ---+   độc lập với BLE
```

| Kênh | Hướng | Dùng cho | Trạng thái |
|---|---|---|---|
| BLE GATT, service `WearableHealthService` | Hai chiều | Điều khiển, dữ liệu sức khỏe, trạng thái, ECG | **[CHỐT]** cho `FE41`, `FE42`, `FE43`, `FE45` |
| NFC (ST25DV04K, Fast Transfer Mode mailbox) | Hai chiều | Cấu hình và đọc log khi không có BLE | **[DỞ]**, xem [16.1](#161-nfc-mailbox-st25dv04k) |
| LoRaWAN (SX1262, OTAA AS923-2) | Thiết bị → network server | Uplink theo lệnh BLE và SOS | **[DỞ]** Chỉ chạy khi app gửi lệnh `0x0F`; trạng thái báo qua `FE46`, dữ liệu uplink webapp **chưa** nhận, xem [16.2](#162-lorawan-uplink) |

Webapp hiện chỉ dùng kênh BLE.

## 2. Quy ước chung

| Quy ước | Nội dung |
|---|---|
| Thứ tự byte | Mọi số nhiều byte trên BLE là **little-endian** (byte thấp trước). Riêng payload LoRaWAN là big-endian |
| Đánh số | Byte đánh số từ `0`. Bit `0` là bit thấp nhất (LSB) |
| Kiểu dữ liệu | `uint8`, `uint16`, `uint32` không dấu; `int16` có dấu, bù 2 |
| Độ dài | Mỗi characteristic có độ dài **cố định**. Gói sai độ dài là gói lỗi và phải bị bỏ |
| Bit/byte reserved | Bên gửi ghi `0`, bên nhận bỏ qua (không được báo lỗi khi khác `0`) |
| Giá trị enum lạ | Bên nhận hiển thị `Unknown (0x..)`, không tự quy đổi |
| Thời gian | Unix time, giây, UTC. Thiết bị **không** có RTC giữ giờ qua reset |
| Ký hiệu | `FE41`…`FE47` là viết tắt của UUID characteristic tương ứng; mã lệnh và mask viết dạng hex `0x..` |

## 3. Tầng kết nối BLE (GAP)

### 3.1 Thông số phía thiết bị **[CHỐT]**

| Thông số | Giá trị | Nguồn |
|---|---|---|
| Vai trò | Peripheral, GATT server | `app_ble.c` |
| GAP Device Name | `BLEWearable` | `a_GapDeviceName` |
| Loại advertising | Legacy, connectable, scannable, general discoverable | `ADV_TYPE` |
| Chu kỳ advertising | 80 đến 100 ms (`0x0080`…`0x00A0` × 0.625 ms), không có timeout | `ADV_INTERVAL_MIN/MAX` |
| Bắt đầu advertising | Ngay sau khi khởi động và ngay sau mỗi lần mất kết nối | `APP_BLE_Init`, sự kiện disconnect |
| Địa chỉ | Static random, cố định cho từng thiết bị | `CFG_BD_ADDRESS_TYPE` |
| Công suất phát | Khoảng 0 dBm: PA level `0x18`; advertising cấu hình 0 dBm, bảng PA của ST (`RADIO_utils.c`) ghi level `0x18` là −1 dBm | `CFG_TX_POWER` |
| ATT MTU tối đa | 247. Firmware **không** tự khởi tạo MTU exchange | `CFG_BLE_ATT_MTU_MAX` |
| Data Length Extension | Tắt | `CFG_BLE_CONTROLLER_DATA_LENGTH_EXTENSION_ENABLED` |
| Tham số kết nối | Do central quyết định; firmware không xin đổi (trừ khi bật switch chẩn đoán `ECG_DIAG_CONN_INTERVAL_MS`) | `wearable_app.c` |
| Số central | Firmware chỉ theo dõi **một** connection handle; giao thức giả định một central | `WEARABLE_APP_Context` |
| Bảo mật | Không characteristic nào yêu cầu mã hóa hay xác thực. Không cần pairing/bonding | `BLE_GATT_SRV_PERM_NONE` |

Nếu central tự khởi tạo pairing, thiết bị trả lời với IO capability *Display Yes/No*, MITM bắt buộc, bonding bật, Secure Connections tùy chọn, passkey cố định `111111`, và tự xác nhận numeric comparison. Giao thức v1 không dựa vào pairing.

### 3.2 Nội dung gói advertising (31 byte)

| Byte | Giá trị | Ý nghĩa |
|---|---|---|
| 0–2 | `02 01 06` | Flags: LE General Discoverable, không hỗ trợ BR/EDR |
| 3–15 | `0C 09` + `"BLEWearable"` | Complete Local Name, 11 ký tự ASCII |
| 16–30 | `0E FF 30 00` + 11 byte `00` | Manufacturer Specific Data, company ID `0x0030` (STMicroelectronics), phần dữ liệu chưa dùng |

UUID của service **không** nằm trong gói advertising và không có scan response. Vì vậy app phải lọc theo **tên**, không lọc được theo service UUID.

### 3.3 Yêu cầu phía webapp

- Web Bluetooth cần secure context (HTTPS hoặc `localhost`), trình duyệt nhân Chromium (Chrome, Edge trên desktop và Android), và `requestDevice()` phải được gọi từ thao tác của người dùng.
- Phải khai báo service trong `optionalServices`, nếu không trình duyệt chặn truy cập GATT:

```ts
const device = await navigator.bluetooth.requestDevice({
  filters: [{ name: 'BLEWearable' }],
  optionalServices: ['0000fe40-cc7a-482a-984a-7f2ed5b3e58f'],
})
```

  Webapp hiện dùng `acceptAllDevices: true` cùng `optionalServices`; cả hai cách đều hợp lệ.
- Web Bluetooth không cho app chọn MTU hay connection interval; trình duyệt và hệ điều hành tự thương lượng.

### 3.4 Hành vi khi mất kết nối **[CHỐT]**

Khi kết nối bị ngắt (bất kể bên nào ngắt), firmware:

1. tắt toàn bộ trạng thái notify (CCCD không được nhớ qua lần kết nối sau);
2. dừng timer 1 giây, dừng luồng ECG, dừng cảm biến;
3. đưa state về `Idle`;
4. advertising lại ngay.

`power state`, các cờ (`0x08`, `0x10`, `0x40`) và error code **không** bị xóa khi mất kết nối. Thời gian đã đồng bộ vẫn giữ cho đến khi thiết bị reset.

Sau khi kết nối lại, app phải làm lại từ đầu: subscribe, đồng bộ thời gian, gửi lệnh bắt đầu đo.

## 4. GATT: service và characteristic

### 4.1 Service

| Thuộc tính | Giá trị |
|---|---|
| Tên | `WearableHealthService` |
| Loại | Primary service |
| UUID | `0000FE40-CC7A-482A-984A-7F2ED5B3E58F` |

### 4.2 Characteristic

UUID đầy đủ của characteristic có dạng `0000FE4x-8E22-4541-9D4C-21EDAE82ED19`. Lưu ý phần đuôi **khác** phần đuôi của service.

| Tên | UUID | Properties | Độ dài | Hướng | Trạng thái |
|---|---|---|---:|---|---|
| `CONTROL` | `0000FE41-8E22-4541-9D4C-21EDAE82ED19` | Write (có response) | 8 | App → thiết bị | **[CHỐT]** |
| `SENSOR_DATA` | `0000FE42-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 16 | Thiết bị → app | **[CHỐT]** |
| `DEVICE_STATUS` | `0000FE43-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 8 | Thiết bị → app | **[CHỐT]** |
| `NFC_DATA` | `0000FE44-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 20 | Thiết bị → app | **[STUB]** |
| `ECG_DATA` | `0000FE45-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 20 | Thiết bị → app | **[CHỐT]** |
| `DEBUG_DATA` | `0000FE46-8E22-4541-9D4C-21EDAE82ED19` | Read, Write, Notify | 20 | Hai chiều | **[STUB]** |
| `RECOVERY_DATA` | `0000FE47-8E22-4541-9D4C-21EDAE82ED19` | Read, Notify | 24 | Thiết bị → app | **[DỞ]** |

Webapp gọi `FE44` là `NFC_EVENT`; đó là cùng một characteristic với `NFC_DATA` của firmware.

Trong `wearable.c` UUID được khai báo theo thứ tự byte **đảo ngược** (byte thấp trước), ví dụ `CONTROL_UUID` bắt đầu bằng `0x19,0xed,0x82,...` và kết thúc bằng `...,0x41,0xfe,0x00,0x00`.

### 4.3 Notification (CCCD)

- Mỗi characteristic có Notify đều có CCCD riêng. App ghi `01 00` để bật, `00 00` để tắt; Web Bluetooth làm việc này qua `startNotifications()` / `stopNotifications()`.
- Không dùng Indication.
- Bật notify cho `FE43` làm firmware gửi ngay một gói `DEVICE_STATUS`. Vì vậy app phải gắn listener `characteristicvaluechanged` **trước** khi gọi `startNotifications()`.
- Bật notify cho `FE42` khi đang ở state `Measuring` hoặc `ECG active` khởi động lại timer 1 giây.

### 4.4 Read

Read trả về **gói gần nhất đã được notify** trên characteristic đó, và trả về toàn `0` nếu chưa notify lần nào. Read không tạo dữ liệu mới. App dùng notification, không poll bằng read.

### 4.5 Giới hạn độ dài và MTU

Với ATT MTU mặc định 23, một notification chở tối đa 20 byte. `FE42`, `FE43`, `FE44`, `FE45`, `FE46` đều vừa. Riêng `FE47` dài 24 byte nên cần ATT MTU từ 27 trở lên; xem [mục 11](#11-recovery_data-fe47).

---

## 5. CONTROL `FE41`: lệnh từ app xuống thiết bị

**[CHỐT]** cho các lệnh `0x01`…`0x09`.

### 5.1 Khung lệnh

App **luôn ghi đúng 8 byte** bằng Write Request (có response).

| Byte | Kiểu | Ý nghĩa |
|---:|---|---|
| 0 | `uint8` | Mã lệnh |
| 1–7 | tùy lệnh | Tham số, little-endian. Lệnh không có tham số thì cả 7 byte bằng `0` |

Quy tắc xử lý ở firmware:

- Firmware chọn lệnh theo byte `0`. Lệnh không có tham số thì byte `1–7` bị bỏ qua.
- Ghi rỗng (0 byte) bị bỏ qua hoàn toàn, không đổi state, không có phản hồi.
- Không có gói ACK riêng. Phản hồi của một lệnh là notification `FE43` (nếu lệnh đó có phản hồi, xem bảng dưới) cùng với response ở tầng ATT của thao tác write.
- Một lệnh hợp lệ thuộc nhóm `0x01`…`0x0A` xóa error code lệnh (`0x01`) của lần trước.

### 5.2 Bảng lệnh

| Byte 0 | Tên | Tham số | State sau lệnh | Notify `FE43` ngay | Trạng thái |
|---:|---|---|---|:---:|---|
| `0x01` | Start measurement | — | `Measuring` | Có | **[CHỐT]** |
| `0x02` | Stop measurement | — | `Idle` | Có | **[CHỐT]** |
| `0x03` | Request data | — | Không đổi | Không | **[CHỐT]** |
| `0x04` | Normal mode | — | `Low power` → `Idle`; state khác giữ nguyên | Có | **[CHỐT]** |
| `0x05` | Low-power mode | — | `Low power` | Có | **[CHỐT]** |
| `0x06` | ECG start | — | `ECG active` | Có | **[CHỐT]** |
| `0x07` | ECG stop | — | `Idle` | Có | **[CHỐT]** |
| `0x08` | Emergency test | — | `Emergency` | Có | **[CHỐT]** |
| `0x09` | Sync time | Unix time, xem 5.4 | Không đổi | Có | **[CHỐT]** |
| `0x0A` | Get recovery info | — | Không đổi | Không | **[STUB]** |
| `0x0B` | Start BLE recovery | Byte `1–2`: sequence bắt đầu, `uint16` | Không đổi | Không | **[DỞ]** |
| `0x0C` | Stop BLE recovery | — | Không đổi | Không | **[DỞ]** |
| `0x0D` | Recovery ACK | Byte `1–2`: sequence đã nhận, `uint16` | Không đổi | Không | **[DỞ]** |
| `0x0E` | Recovery clear | — | Không đổi | Không | **[DỞ]** |
| `0x0F` | LoRa join (OTAA) | — | Không đổi | Không, trả lời trên `FE46` | **[DỞ]** |
| `0x10` | LoRa test uplink | — | Không đổi | Không, trả lời trên `FE46` | **[DỞ]** |
| `0x11` | LoRa status | — | Không đổi | Không, trả lời trên `FE46` | **[DỞ]** |
| `0x12` | LoRa TX power | Byte `1`: công suất tối đa của SX1262, `int8` dBm, `-9`…`22` | Không đổi | Không, trả lời trên `FE46` | **[DỞ]** |
| `0x13` | LoRa stop | — | Không đổi | Không, trả lời trên `FE46` | **[DỞ]** |
| khác | Không hợp lệ | — | `Error`, error code `0x01` | Có | **[CHỐT]** |

Mã `0x00` và `0x14`…`0xFF` hiện đều là lệnh không hợp lệ. Mã mới được cấp tiếp từ `0x14`.

### 5.3 Chi tiết từng lệnh

**`0x01` Start measurement** — `01 00 00 00 00 00 00 00`

- Kết thúc phiên ECG nếu đang có, xóa cờ `0x20`.
- Bật cảm biến: MAX86150 ở chế độ PPG (nhịp tim, SpO2), MAX30208 (nhiệt độ), luồng QVar 100 Hz của LIS2DUXS12TR (phát hiện đeo).
- State `Measuring`, timer 1 giây chạy, `SENSOR_DATA` được notify mỗi giây nếu app đã subscribe `FE42`.
- Nếu khối cảm biến chưa khởi tạo được (ADC supercap lỗi; việc khởi tạo chạy ở lần kết nối BLE đầu tiên sau reset): error code `0x01`, state `Error`.
- `power state` **không** đổi. Nếu trước đó đã gửi `0x05` thì vẫn là `2`; muốn về `1` phải gửi `0x04`.

**`0x02` Stop measurement** — `02 00 00 00 00 00 00 00`

- Dừng luồng ECG, tắt cảm biến, dừng timer. State `Idle`.
- Giá trị cuối cùng của `SENSOR_DATA` được giữ lại (đọc được qua `0x03` hoặc Read).

**`0x03` Request data** — `03 00 00 00 00 00 00 00`

- Yêu cầu firmware gửi ngay một gói `SENSOR_DATA` (chỉ gửi nếu app đã subscribe `FE42`).
- Đang đo thì gói chứa số liệu vừa đọc. Không đo thì gói là **ảnh chụp cũ** (giá trị lúc dừng đo, hoặc giá trị khởi tạo nếu chưa đo lần nào).
- Không có `DEVICE_STATUS` ngay; `FE43` chỉ được gửi nếu nội dung status thay đổi.

**`0x04` Normal mode** — `04 00 00 00 00 00 00 00`

- Đặt `power state = 1`. Nếu đang `Low power` thì về `Idle`; state khác giữ nguyên.
- Không tự bật lại phép đo.

**`0x05` Low-power mode** — `05 00 00 00 00 00 00 00`

- Dừng luồng ECG, tắt cảm biến, dừng timer. `power state = 2`, state `Low power`.

**`0x06` ECG start** — `06 00 00 00 00 00 00 00`

- Bắt đầu đo nếu chưa đo, rồi chuyển MAX86150 sang chế độ ECG 200 sps. Bật cờ `0x20`, state `ECG active`, bắt đầu stream `ECG_DATA`, sequence về `0`.
- LED PPG tắt: nhịp tim và SpO2 trong `SENSOR_DATA` **đứng yên** ở giá trị cuối. Luồng QVar tắt: cờ đeo `0x40` giữ nguyên.
- `SENSOR_DATA` vẫn được gửi mỗi giây trong phiên ECG.
- Nếu MAX86150 không phản hồi: error code `0x01`, state `Measuring` (không có ECG).
- Nếu khối cảm biến chưa khởi tạo: error code `0x01`, state `Error`.
- Gửi lại `0x06` khi đang ECG sẽ khởi động lại phiên (sequence về `0`).
- App phải subscribe `FE45` **trước** khi gửi lệnh này.

**`0x07` ECG stop** — `07 00 00 00 00 00 00 00`

- Giống `0x02`: dừng ECG **và** dừng đo, state `Idle`. Muốn đo tiếp nhịp tim/SpO2 phải gửi lại `0x01`.

**`0x08` Emergency test** — `08 00 00 00 00 00 00 00`

- Bật cờ `0x10`, state `Emergency`, dừng timer 1 giây (hết `SENSOR_DATA` định kỳ).
- Cảm biến **không** bị tắt. Nếu đang có phiên ECG thì `ECG_DATA` vẫn tiếp tục.
- Cờ `0x10` không có lệnh xóa; nó chỉ mất khi thiết bị reset. Thoát state `Emergency` bằng `0x01`, `0x02`, `0x05`, `0x06` hoặc `0x07`.

**`0x0E` Recovery clear** — `0E 00 00 00 00 00 00 00`

- Xóa header log trên ST25DV và dừng phiên recovery. Không hoàn tác được. Không có phản hồi.

Các lệnh `0x0A`…`0x0D` được mô tả ở [mục 11](#11-recovery_data-fe47).

**`0x0F`…`0x13` Lệnh test LoRaWAN** **[DỞ]**

Dùng để thử LoRaWAN từ webapp. LoRa **không bao giờ tự chạy**: SX1262 không phát gì cho tới khi app gửi `0x0F`. Các lệnh này không đổi measurement state, không đụng error code của `FE43` và không làm thiết bị vào state `Error`. Mỗi lệnh được trả lời bằng gói LoRa status `0x20` trên `FE46` ([mục 10.1](#101-gói-0x20-lora-status-dở)); app phải subscribe `FE46` để nhận.

- `0x0F` LoRa join — `0F 00 00 00 00 00 00 00`. Lần đầu sau reset: khởi động LoRa Basics Modem rồi join OTAA. Sau `0x13`: join lại. Đang join, đã join, hoặc join lỗi (state `4`, LBM tự thử lại) thì không làm gì.
- `0x10` LoRa test uplink — `10 00 00 00 00 00 00 00`. Một uplink không xác nhận trên FPort `101`, payload là bộ đếm uplink 4 byte big-endian ([mục 16.2](#162-lorawan-uplink)). Chưa join thì kết quả `0x02`.
- `0x11` LoRa status — `11 00 00 00 00 00 00 00`. Chỉ trả về trạng thái.
- `0x12` LoRa TX power — `12 <dBm> 00 00 00 00 00 00`. Byte `1` là `int8`, ví dụ `0x00` = 0 dBm, `0xFB` = −5 dBm, `0x0E` = 14 dBm. Áp dụng từ lần phát kế tiếp; về mặc định `0` dBm khi thiết bị reset. Ngoài `-9`…`22` thì kết quả `0x04`, giá trị cũ giữ nguyên.
- `0x13` LoRa stop — `13 00 00 00 00 00 00 00`. Rời mạng (`smtc_modem_leave_network`), dừng join và uplink; SX1262 ngủ cho tới `0x0F`.

Ngoài trả lời lệnh, firmware còn gửi gói `0x20` sau mỗi sự kiện của modem (joined, join fail, TX done, downlink…), với byte `6` = `0xFF`.

### 5.4 Lệnh Sync time (`0x09`)

| Byte | Kiểu | Ý nghĩa | Ràng buộc |
|---:|---|---|---|
| 0 | `uint8` | `0x09` | |
| 1–4 | `uint32` LE | Unix time, giây, UTC | |
| 5–6 | `uint16` LE | Mili giây | `0`…`999` |
| 7 | `uint8` | Reserved | Bắt buộc `0x00` |

Ví dụ: Unix time `1724833200` (`0x66CEDDB0`) và `500` ms (`0x01F4`):

```text
09 B0 DD CE 66 F4 01 00
```

- Thành công: error code `0x00` trong `FE43` gửi ngay sau đó. State không đổi.
- Thất bại (gói ngắn hơn 8 byte, mili giây lớn hơn `999`, byte 7 khác `0`): error code `0x01`, state **không** đổi, thời gian cũ giữ nguyên.
- Thiết bị tính thời gian hiện tại từ mốc này cộng bộ đếm mili giây nội bộ. Mốc mất khi thiết bị reset, và chỉ đúng trong khoảng 49.7 ngày kể từ lần đồng bộ cuối.
- Khi chưa đồng bộ, mọi timestamp do thiết bị sinh ra bằng `0`.
- Thời gian này hiện chỉ dùng để đóng dấu record log trên ST25DV; `SENSOR_DATA` và `ECG_DATA` không mang timestamp.

> Không được ghi thẳng Unix time dạng `uint64` vào `FE41`: byte thấp nhất sẽ bị hiểu là một mã lệnh bất kỳ.

### 5.5 Xử lý lỗi lệnh

| Tình huống | Error code | State | Ghi chú |
|---|---|---|---|
| Mã lệnh lạ | `0x01` | `Error` | Cảm biến và luồng ECG đang chạy **không** bị tắt; có thể còn một gói `SENSOR_DATA` cuối |
| `0x09` sai định dạng | `0x01` | Không đổi | |
| `0x06` khi MAX86150 lỗi | `0x01` | `Measuring` | |
| `0x01` / `0x06` khi cảm biến chưa khởi tạo | `0x01` | `Error` | |
| `0x0B` / `0x0D` ngắn hơn 3 byte | — | Không đổi | Bị bỏ qua, không báo lỗi |

Để đưa thiết bị về trạng thái sạch sau lỗi, app gửi `0x02` (về `Idle`, xóa error code, tắt cảm biến).

---

## 6. SENSOR_DATA `FE42`

**[CHỐT]** Payload cố định **16 byte**.

### 6.1 Byte layout

| Byte | Kiểu | Trường | Đơn vị / quy đổi | Giá trị đặc biệt |
|---:|---|---|---|---|
| 0 | `uint8` | Heart rate | bpm | Khởi tạo `72` |
| 1 | `uint8` | SpO2 | % | Khởi tạo `98`; giá trị tính được nằm trong `70`…`100` |
| 2–3 | `int16` LE | Temperature | `raw / 100` °C | `-32768` (`00 80`) = chưa có giá trị hợp lệ |
| 4–5 | `uint16` LE | Supercap voltage | mV | |
| 6 | `uint8` | Power state | `1` = Normal, `2` = Low power | |
| 7 | bit field | Flags | Xem 6.2 | |
| 8–9 | `int16` LE | Accel X | mg (thang ±4 g) | `0` nếu chưa đọc được lần nào |
| 10–11 | `int16` LE | Accel Y | mg | |
| 12–13 | `int16` LE | Accel Z | mg | |
| 14–15 | `int16` LE | QVar raw | LSB thô, có dấu | 12 bit canh trái: 4 bit thấp luôn `0` |

### 6.2 Flags (byte 7)

| Bit | Mask | Tên | Ý nghĩa |
|---:|---:|---|---|
| 0 | `0x01` | Reserved | Luôn `0` |
| 1 | `0x02` | Reserved | Luôn `0` |
| 2 | `0x04` | Reserved | Luôn `0` |
| 3 | `0x08` | `FALL_CANDIDATE` | Có ứng viên té ngã. Cần chương trình MLC trên LIS2DUXS12TR; hiện chưa nạp nên luôn `0`. Khi đã bật thì không có đường xóa ngoài reset |
| 4 | `0x10` | `EMERGENCY` | Bật bởi lệnh `0x08`. Chỉ mất khi thiết bị reset |
| 5 | `0x20` | `ECG_ACTIVE` | Đang trong phiên ECG |
| 6 | `0x40` | `WEAR_DETECTED` | Thiết bị đang được đeo (`1`) hay không (`0`) |
| 7 | `0x80` | Reserved | Luôn `0` |

### 6.3 Ý nghĩa và độ tin cậy từng trường

**Heart rate, SpO2 (byte 0, 1)**

- Tính từ PPG Red/IR của MAX86150 (100 sps). Nhịp tim là trung bình 4 khoảng nhịp gần nhất, chấp nhận trong `30`…`220` bpm. SpO2 được tính lại khoảng mỗi giây.
- **Không có cờ hợp lệ.** Khi MAX86150 không chạy (không có chip hoặc cấu hình lỗi), firmware gửi nhịp tim **giả** tăng dần 68 → 82 rồi lặp lại; SpO2 giữ giá trị cuối (mặc định `98`).
- Khi không có tiếp xúc da, SpO2 giữ giá trị cũ.
- Trong phiên ECG cả hai giá trị đứng yên.
- Thuật toán là heuristic, chưa hiệu chuẩn với máy đo chuẩn. Không dùng cho chẩn đoán.

**Temperature (byte 2–3)**

- MAX30208, đo một lần mỗi giây khi đang đo. Ví dụ `44 0E` = `0x0E44` = 3652 → 36.52 °C.
- `-32768` cho đến khi có lần đo hợp lệ đầu tiên. Khi một lần đo lỗi, trường này **giữ giá trị hợp lệ gần nhất**; lỗi được báo qua error code `0x10`…`0x12` của `FE43`.

**Supercap voltage (byte 4–5)**

- Đọc ADC một lần khi khởi tạo cảm biến (ngay sau lần kết nối BLE đầu tiên kể từ khi reset) và mỗi giây khi đang đo. Ví dụ `E4 0C` = 3300 mV.

**Power state (byte 6)**

- Chỉ nhận `1` hoặc `2`, và chỉ đổi theo lệnh `0x04` / `0x05`. Firmware chưa tự đổi theo điện áp supercap.

**Accel X/Y/Z (byte 8–13)**

- LIS2DUXS12TR, thang ±4 g, ODR 100 Hz. Mỗi gói chứa **một** mẫu tức thời lấy lúc tạo gói (1 Hz), không phải trung bình.
- Khi không đọc được chip, giữ giá trị cuối.
- App tính `magnitude = sqrt(X² + Y² + Z²)`; đây là giá trị dẫn xuất, không nằm trong payload. Thiết bị đứng yên cho magnitude khoảng 1000 mg.

**QVar raw (byte 14–15)**

- Mẫu mới nhất của kênh AH_QVAR, gain 0.5 (khoảng 37 LSB/mV), trở kháng vào 520 MΩ. Chỉ là giá trị thô để quan sát.
- Khi đang đo (ngoài phiên ECG) giá trị này là mẫu gần nhất của luồng 100 Hz; trong phiên ECG nó chỉ được cập nhật 1 lần mỗi giây.
- App **không** tự áp ngưỡng lên giá trị này để suy ra trạng thái đeo; dùng cờ `0x40`.

**Cờ `WEAR_DETECTED` (`0x40`)**

- Firmware phân loại: tính biên độ đỉnh-đỉnh của QVar trong từng cửa sổ 100 mẫu (khoảng 1 giây). Từ `600` LSB trở lên trong 2 cửa sổ liên tiếp → bật; từ `300` LSB trở xuống trong 2 cửa sổ liên tiếp → tắt.
- Chỉ được cập nhật khi đang đo và ngoài phiên ECG. Khi dừng đo hoặc trong phiên ECG, cờ giữ nguyên trạng thái cuối.
- Ngưỡng là ước lượng ban đầu, **chưa hiệu chuẩn**.

### 6.4 Thời điểm gửi

Firmware notify `SENSOR_DATA` (khi `FE42` đã được subscribe):

| Khi nào | Ghi chú |
|---|---|
| Mỗi 1 giây khi state là `Measuring` hoặc `ECG active` | Timer được nạp lại sau khi xử lý xong nên chu kỳ thực dài hơn 1 giây một chút |
| Ngay khi cờ `0x40` hoặc `0x08` đổi | Gói ngoài lịch, không làm lệch timer 1 giây |
| Một lần khi nhận lệnh `0x03` | Xem mục 5.3 |

Khi khối cảm biến chưa khởi tạo (`sensor ready = 0`), payload là 16 byte `0`.

---

## 7. DEVICE_STATUS `FE43`

**[CHỐT]** Payload cố định **8 byte**.

### 7.1 Byte layout

| Byte | Kiểu | Trường | Giá trị |
|---:|---|---|---|
| 0 | `uint8` | Measurement state | Xem 7.2 |
| 1 | `uint8` | Sensor ready | `1` khi khối cảm biến đã khởi tạo (ADC supercap). **Không** phản ánh từng cảm biến I2C |
| 2 | `uint8` | Error code | Xem 7.3 |
| 3 | `uint8` | Power state | `1` = Normal, `2` = Low power |
| 4–5 | `uint16` LE | Supercap voltage | mV, giá trị tại thời điểm tạo gói |
| 6 | `uint8` | Reset counter | Chưa triển khai, luôn `0` |
| 7 | bit field | Flags + protocol version | Xem 7.4 |

Khi `sensor ready = 0` thì byte `3`, `4–5` và nibble cao của byte `7` đều bằng `0`.

### 7.2 Measurement state (byte 0)

| Giá trị | Tên | `SENSOR_DATA` định kỳ | Ý nghĩa |
|---:|---|:---:|---|
| `0` | Idle | Không | Không đo. State sau khi khởi động, sau `0x02`/`0x07`, sau mất kết nối |
| `1` | Measuring | Có | Đang đo PPG, nhiệt độ, gia tốc, QVar |
| `2` | ECG active | Có | Đang stream ECG |
| `3` | Low power | Không | Cảm biến tắt, `power state = 2` |
| `4` | Emergency | Không | Sau lệnh `0x08` |
| `5` | Error | Không | Lệnh không hợp lệ hoặc khởi tạo cảm biến lỗi |
| khác | — | — | App hiển thị `Unknown` |

### 7.3 Error code (byte 2)

| Giá trị | Ý nghĩa | Xuất hiện khi | Hết khi |
|---:|---|---|---|
| `0x00` | Không lỗi | | |
| `0x01` | Lệnh không hợp lệ hoặc không thực hiện được | Xem mục 5.5 | Lệnh hợp lệ kế tiếp thuộc `0x01`…`0x0A` |
| `0x10` | Không tìm thấy cảm biến nhiệt độ | MAX30208 không trả lời | Chip trả lời lại (firmware dò lại mỗi 5 giây khi đang đo) |
| `0x11` | Timeout khi đọc nhiệt độ | Lần đo gần nhất quá 60 ms | Lần đo sau thành công, hoặc dừng đo |
| `0x12` | Lỗi bus cảm biến nhiệt độ | Lần đo gần nhất lỗi I2C | Lần đo sau thành công, hoặc dừng đo |
| khác | — | | App hiển thị `Unknown` |

- Byte này chỉ chứa **một** mã. `0x01` được ưu tiên: khi đang có `0x01` thì mã `0x10`…`0x12` bị che.
- Không có mã "OK" riêng cho cảm biến nhiệt: `0x00` cùng với Temperature khác `-32768` nghĩa là cảm biến hoạt động.
- MAX86150 và LIS2DUXS12TR chưa có error code.
- Quy ước cấp mã mới (đề xuất, chưa có trong code): `0x02`…`0x0F` cho lỗi lệnh/hệ thống, `0x13`…`0x1F` cho cảm biến nhiệt, `0x20` trở lên cho các cảm biến khác.

### 7.4 Flags + protocol version (byte 7)

```text
bit   7   6   5   4 | 3   2   1   0
      R  WEAR ECG EMG | protocol version
```

| Bit | Mask | Ý nghĩa |
|---:|---:|---|
| 0–3 | `0x0F` | Protocol version, hiện là `1` |
| 4 | `0x10` | `EMERGENCY` |
| 5 | `0x20` | `ECG_ACTIVE` |
| 6 | `0x40` | `WEAR_DETECTED` |
| 7 | `0x80` | Reserved |

Nibble cao là bản sao nibble cao của flags trong `SENSOR_DATA`. Cờ `FALL_CANDIDATE` (`0x08`) **không** có trong `FE43` vì nibble thấp mang protocol version.

### 7.5 Thời điểm gửi

`DEVICE_STATUS` **không** gửi định kỳ. Firmware notify khi:

| Khi nào | Ghi chú |
|---|---|
| App bật notify cho `FE43` | Gói đầu tiên sau khi kết nối |
| Sau các lệnh `0x01`, `0x02`, `0x04`…`0x09` và sau lệnh không hợp lệ | Kể cả khi nội dung không đổi |
| Byte `0`, `1`, `2`, `3` hoặc `7` tự thay đổi | Ví dụ lỗi nhiệt độ xuất hiện/hết, cờ đeo đổi. Kiểm tra mỗi giây khi đang đo và khi có ngắt chuyển động |

Điện áp supercap (byte `4–5`) thay đổi thì **không** kích hoạt notify, nên giá trị này có thể cũ. Giá trị mới nhất nằm ở `SENSOR_DATA`.

---

## 8. ECG_DATA `FE45`

**[CHỐT]** Payload cố định **20 byte**.

### 8.1 Byte layout

| Byte | Kiểu | Trường | Giá trị |
|---:|---|---|---|
| 0 | `uint8` | Sequence | Tăng 1 mỗi gói, quay vòng `255` → `0`. Về `0` khi bắt đầu phiên mới |
| 1 | `uint8` | Sample count | Số mẫu hợp lệ. Firmware luôn gửi `9` |
| 2–3 | `int16` LE | Sample 0 | Mẫu cũ nhất trong gói |
| 4–5 | `int16` LE | Sample 1 | |
| … | … | … | |
| 18–19 | `int16` LE | Sample 8 | Mẫu mới nhất trong gói |

Mẫu thứ `i` nằm ở byte `2 + 2i` và `3 + 2i`.

### 8.2 Thông số tín hiệu

| Thông số | Giá trị |
|---|---|
| Nguồn | MAX86150, một đạo trình |
| Tần số lấy mẫu | **200 Hz** |
| Tốc độ gói | Khoảng 22.2 gói/giây (200 / 9), tức 444 byte/giây |
| Gain analog | IA 9.5 × PGA 8 = 76 V/V |
| Giá trị mẫu | ADC 18 bit của chip dịch phải 2 bit: `sample = raw18 >> 2` |
| Quy đổi điện áp | `µV = sample × 0.645` (điện áp ở đầu vào, trước gain) |
| Dải biểu diễn | `int16` đầy thang tương ứng khoảng ±21.1 mV |
| Băng thông lọc của chip | 52 Hz (70%) / 29 Hz (90%) ở 200 sps |

Gói không mang timestamp. App dựng trục thời gian bằng cách đếm mẫu: mẫu thứ `n` của phiên ở thời điểm `n / 200` giây.

### 8.3 Phiên ECG

- Bắt đầu: lệnh `0x06`. Kết thúc: `0x07`, `0x01`, `0x02`, `0x05` hoặc mất kết nối.
- Firmware đọc FIFO của chip mỗi khoảng 45 ms, đóng một gói mỗi khi đủ 9 mẫu và giữ mẫu lẻ cho lần sau. Các gói vì vậy đến theo cụm, không cách đều.
- Read trả về gói ECG gần nhất đã gửi.

### 8.4 Mất gói

- **Sequence nhảy cóc nghĩa là mất gói thật.** Firmware vẫn tăng sequence khi app chưa subscribe `FE45` và khi hàng đợi gửi (16 gói, khoảng 0.7 giây) bị đầy và phải bỏ gói cũ nhất.
- Số gói mất: `gap = (seq − seq_trước + 256) mod 256`, số gói mất là `gap − 1`. App chèn `9 × (gap − 1)` mẫu trống để giữ đúng trục thời gian.
- Mất liên tiếp từ 256 gói trở lên (khoảng 11.5 giây) thì không phát hiện được bằng sequence.
- Tràn FIFO **trong chip** (task đọc bị trễ quá 160 ms) làm mất mẫu mà sequence không phản ánh. Firmware đếm sự kiện này trong biến chẩn đoán `g_ecgDiag`, không gửi qua BLE.
- Nếu `sample count < 9` (hiện không xảy ra), các mẫu còn lại là padding `0` và phải bị bỏ.

---

## 9. NFC_DATA `FE44`

**[STUB]** Payload 20 byte. Characteristic có trong GATT, struct và hàm encode đã có (`wearable_nfc_status_t`, `WearableData_EncodeNFCStatus`), nhưng firmware **chưa gửi gói nào**.

| Byte | Kiểu | Trường | Ghi chú |
|---:|---|---|---|
| 0 | `uint8` | NFC state | Enum chưa định nghĩa |
| 1 | `uint8` | Last event | Enum chưa định nghĩa |
| 2 | `uint8` | FTM status | Enum chưa định nghĩa |
| 3 | `uint8` | Config result | Enum chưa định nghĩa |
| 4 | `uint8` | Recovery status | Enum chưa định nghĩa |
| 5–19 | — | Reserved | `0` |

App giữ 5 trường dưới dạng số thô. Phải định nghĩa enum ở cả hai phía trước khi dùng.

## 10. DEBUG_DATA `FE46`

**[STUB]** Payload 20 byte, dùng chung cho lệnh (app ghi) và phản hồi (thiết bị notify).

| Byte | Kiểu | Trường |
|---:|---|---|
| 0 | `uint8` | Mã lệnh debug |
| 1–19 | — | Tham số hoặc dữ liệu phản hồi, chưa định nghĩa |

Firmware mới đặt tên cho các mã lệnh, **chưa xử lý lệnh nào**: ghi vào `FE46` bị bỏ qua và không có phản hồi.

| Mã | Tên dự kiến |
|---:|---|
| `0x01` | Set sensor rate |
| `0x02` | Set BLE interval |
| `0x03` | Set log interval |
| `0x04` | Set power threshold |
| `0x05` | Get power stats |
| `0x06` | Get log info |
| `0x07` | Get sensor status |

Layout tham số và phản hồi của từng lệnh phải được thêm vào mục này trước khi triển khai.

### 10.1 Gói `0x20`: LoRa status **[DỞ]**

Gói duy nhất firmware hiện gửi trên `FE46` (notify). Trả lời các lệnh `0x0F`…`0x13` của `FE41` và báo mỗi sự kiện modem. Ghi vào `FE46` vẫn bị bỏ qua.

| Byte | Kiểu | Trường | Giá trị |
|---:|---|---|---|
| 0 | `uint8` | Mã gói | `0x20` |
| 1 | `uint8` | LoRa state | `0` chưa chạy hoặc đã stop, `1` thiếu credentials, `2` đang join, `3` đã join, `4` join lỗi (LBM tự thử lại), `5` lỗi API của LBM, `0xFF` firmware build với `LBM_APP_ENABLE = 0` |
| 2 | `int8` | TX power cap | dBm, giới hạn công suất phát của SX1262 |
| 3 | `uint8` | Sự kiện modem gần nhất | `0` reset, `1` alarm, `2` joined, `3` TX done, `4` downlink, `5` join fail. Chỉ có nghĩa khi byte `10–11` > 0 |
| 4 | `uint8` | Kết quả TX gần nhất | `0` chưa gửi, `1` đã gửi, `2` có ACK |
| 5 | `int8` | Mã lỗi LBM gần nhất | `smtc_modem_return_code_t`: `0` OK, `1` not init, `2` invalid, `3` busy, `4` fail, `5` no time |
| 6 | `uint8` | Kết quả lệnh | `0x00` OK, `0x01` firmware không có LoRa, `0x02` chưa join (cho `0x10`), `0x03` LBM từ chối (xem byte 5), `0x04` TX power ngoài `-9`…`22`, `0xFF` không phải trả lời lệnh mà là một sự kiện modem |
| 7 | `uint8` | Modem | `0` chưa khởi động, `1` đang trong `smtc_modem_init()`, `2` đã khởi động |
| 8–9 | `uint16` LE | Số uplink đã yêu cầu | |
| 10–11 | `uint16` LE | Số sự kiện modem | |
| 12–13 | `uint16` LE | Số downlink | |
| 14–15 | `uint16` LE | Số lần LBM panic | Mỗi lần panic là một lần reset MCU; đếm từ khi cấp nguồn |
| 16–17 | `uint16` LE | Số lần BUSY của SX1262 kẹt quá 100 ms | |
| 18–19 | `uint16` LE | Số lỗi SPI tới SX1262 | |

## 11. RECOVERY_DATA `FE47`

**[DỞ]** Payload 24 byte, mỗi gói là một record lịch sử đọc từ ST25DV.

### 11.1 Byte layout

| Byte | Kiểu | Trường | Ghi chú |
|---:|---|---|---|
| 0–1 | `uint16` LE | Sequence | Số thứ tự record |
| 2–5 | `uint32` LE | Timestamp | Unix time, giây; `0` nếu thiết bị chưa đồng bộ giờ lúc ghi |
| 6–21 | 16 byte | Sensor payload | Cùng layout với `SENSOR_DATA` ([mục 6.1](#61-byte-layout)) |
| 22–23 | `uint16` LE | CRC | CRC-16/MODBUS của byte `0–21` của chính gói này |

CRC dùng thuật toán CRC-16/MODBUS (đa thức phản chiếu `0xA001`, giá trị đầu `0xFFFF`). Record được lưu trên ST25DV đúng theo 24 byte này ([mục 16.1](#161-nfc-mailbox-st25dv04k)), nên app kiểm tra được CRC trực tiếp trên gói nhận về.

### 11.2 Luồng dự kiến

```text
App                                   Thiết bị
 | subscribe FE47                        |
 | 0B <seq_lo> <seq_hi> 00 00 00 00 00 ->|  bắt đầu từ sequence yêu cầu
 |                                       |  (nhỏ hơn record cũ nhất thì bắt đầu từ record cũ nhất)
 |<---------- RECOVERY_DATA (seq n) -----|  mỗi record cách nhau ít nhất 50 ms
 |<---------- RECOVERY_DATA (seq n+1) ---|
 | 0D <seq_lo> <seq_hi> 00 ... --------->|  ACK record đã nhận
 | 0C 00 ... --------------------------->|  dừng (tùy chọn)
```

### 11.3 Hiện trạng và các điểm phải chốt trước khi dùng

App **không** được phụ thuộc vào tính năng này ở protocol v1:

- `DataRecovery_Process()` chưa được gọi ở đâu, nên `0x0B` không phát gói nào.
- Log chỉ được ghi khi đang đo mà không có kết nối BLE; trạng thái đó hiện không xảy ra vì mất kết nối là dừng đo.
- Phần lưu trữ trên ST25DV (sequence, CRC, bản đồ EEPROM) đã được sửa trong code nhưng **chưa chạy trên phần cứng**; xem [mục 16.1](#161-nfc-mailbox-st25dv04k).
- Sequence là `uint16`, bắt đầu từ `1` sau mỗi lần xóa log. Khi tràn qua `65535` thì phép so sánh sequence trong recovery sai; chưa xử lý.
- `0x0A` (Get recovery info) không trả về gì, nên app không biết sequence cũ nhất/mới nhất.
- `0x0D` (ACK) chỉ được lưu lại, không điều khiển việc gửi tiếp hay gửi lại.
- Không có gói báo kết thúc phiên recovery.
- Gói 24 byte cần ATT MTU ≥ 27. Với MTU mặc định 23 gói không gửi được nguyên vẹn. Phải chọn một trong hai: bắt buộc MTU lớn, hoặc rút payload xuống 20 byte.
- Bộ nhớ chỉ chứa tối đa 18 record.

---

## 12. Máy trạng thái thiết bị

```text
                 0x01                      0x06
        +---------------------+   +---------------------+
        |                     v   |                     v
     [Idle] <--0x02/0x07-- [Measuring] <--0x01-- [ECG active]
        ^                                              |
        +----------------- 0x02 / 0x07 ----------------+

   0x05 từ bất kỳ state  -> [Low power]  --0x04--> [Idle]
   0x08 từ bất kỳ state  -> [Emergency]
   lệnh lạ từ bất kỳ state -> [Error]
   mất kết nối từ bất kỳ state -> [Idle]
```

### 12.1 Bảng chuyển trạng thái

| Sự kiện | State mới | `power state` | Cảm biến | Timer 1 giây |
|---|---|---|---|---|
| Khởi động | `Idle` | `1` | Chưa khởi tạo | Dừng |
| Kết nối BLE đầu tiên sau reset | Giữ `Idle` (`Error` + `0x01` nếu khởi tạo cảm biến lỗi) | Giữ nguyên | Được dò và cấu hình, chưa đo | Dừng |
| `0x01` | `Measuring` | Giữ nguyên | PPG, nhiệt độ, QVar bật | Chạy |
| `0x02`, `0x07` | `Idle` | Giữ nguyên | Tắt | Dừng |
| `0x03` | Giữ nguyên | Giữ nguyên | Giữ nguyên | Giữ nguyên |
| `0x04` | `Low power` → `Idle`; khác giữ nguyên | `1` | Giữ nguyên | Giữ nguyên |
| `0x05` | `Low power` | `2` | Tắt | Dừng |
| `0x06` | `ECG active` (`Measuring` + `0x01` nếu MAX86150 lỗi) | Giữ nguyên | ECG bật, PPG và luồng QVar tắt, nhiệt độ vẫn đo | Chạy |
| `0x08` | `Emergency` | Giữ nguyên | **Giữ nguyên** | Dừng |
| `0x09`…`0x0E` | Giữ nguyên | Giữ nguyên | Giữ nguyên | Giữ nguyên |
| Lệnh lạ | `Error` | Giữ nguyên | **Giữ nguyên** | Tự dừng sau lần chạy kế tiếp |
| Mất kết nối | `Idle` | Giữ nguyên | Tắt | Dừng |

Hai điểm dễ nhầm:

- `Emergency` và `Error` không tắt cảm biến hay luồng ECG đang chạy. App gửi `0x02` để dọn.
- `power state` độc lập với measurement state: có thể đang `Measuring` với `power state = 2` nếu gửi `0x05` rồi `0x01`.

---

## 13. Luồng sử dụng chuẩn

### 13.1 Kết nối và bắt đầu đo

```text
App                                          Thiết bị (đang advertising "BLEWearable")
 | requestDevice + gatt.connect() ------------->|
 | getPrimaryService(FE40), getCharacteristics  |
 | gắn listener, startNotifications(FE42)       |
 | gắn listener, startNotifications(FE43) ----->|
 |<------------- DEVICE_STATUS (state Idle) ----|  gửi ngay khi bật notify FE43
 | startNotifications(FE45)                     |
 | write FE41: 09 <time>  (Sync time) --------->|
 |<------------- DEVICE_STATUS (error 0x00) ----|
 | write FE41: 01 00 00 00 00 00 00 00 -------->|
 |<------------- DEVICE_STATUS (Measuring) -----|
 |<------------- SENSOR_DATA ------------------ |  mỗi giây
 |<------------- SENSOR_DATA + DEVICE_STATUS ---|  ngay khi cờ đeo đổi
```

Webapp hiện tự đồng bộ thời gian sau khi kết nối, còn lệnh `0x01` do người dùng bấm.

### 13.2 Phiên ECG

```text
 | (FE45 đã subscribe)                          |
 | write FE41: 06 00 ... ---------------------->|
 |<------------- DEVICE_STATUS (ECG active, cờ 0x20)
 |<------------- ECG_DATA seq 0, 1, 2, ... -----|  khoảng 22 gói/giây
 |<------------- SENSOR_DATA (HR/SpO2 đứng yên) |  mỗi giây
 | write FE41: 07 00 ... ---------------------->|
 |<------------- DEVICE_STATUS (Idle) ----------|
 | write FE41: 01 00 ... (nếu muốn đo tiếp) --->|
```

Nếu sau `0x06` mà `FE43` báo state `Measuring` với error `0x01` thì MAX86150 không vào được chế độ ECG; app báo lỗi cho người dùng và không chờ `ECG_DATA`.

### 13.3 Mất kết nối và kết nối lại

1. App nhận sự kiện `gattserverdisconnected`, đánh dấu mọi số liệu là cũ.
2. Thiết bị tự về `Idle` và advertising lại.
3. App kết nối lại và chạy lại toàn bộ luồng 13.1. Không giả định notify, state hay phiên ECG còn giữ.

### 13.4 Dừng sạch

Trước khi chủ động ngắt kết nối, app nên gửi `0x02` để thiết bị tắt cảm biến ngay (dù thiết bị cũng tự làm việc này khi mất kết nối).

---

## 14. Quy tắc xử lý phía webapp

1. **Kiểm tra độ dài** trước khi decode: `FE42` = 16, `FE43` = 8, `FE45` = 20, `FE44` = 20, `FE46` = 20, `FE47` = 24. Sai độ dài thì bỏ gói và ghi log.
2. **Kiểm tra protocol version**: `status[7] & 0x0F`. Khác `1` thì cảnh báo người dùng và không tin các trường chưa biết.
3. **Subscribe trước, lệnh sau.** Gắn listener trước `startNotifications()`.
4. **Luôn ghi đủ 8 byte** vào `FE41`, byte không dùng bằng `0`.
5. **Xác nhận lệnh bằng `FE43`**, không giả định lệnh thành công: sau khi ghi lệnh, đọc measurement state và error code trong notification kế tiếp.
6. **Trạng thái hiển thị lấy từ thiết bị**, không từ nút người dùng vừa bấm. Ví dụ trạng thái "đang ECG" là cờ `0x20`, không phải việc đã gửi `0x06`.
7. **Timestamp**: `FE42` và `FE45` không có timestamp, app đóng dấu thời gian lúc nhận. Với ECG, trục thời gian dựng theo số mẫu và sequence ([mục 8.4](#84-mất-gói)).
8. **Supercap** lấy từ `FE42`; giá trị trong `FE43` có thể cũ.
9. **Nhịp tim/SpO2** không có cờ hợp lệ: khi `ECG_ACTIVE` bật thì hiển thị là tạm dừng; khi cờ đeo tắt thì không nên coi là số đo thật.
10. **Trạng thái đeo** lấy từ cờ `0x40`, không tự áp ngưỡng lên QVar raw.
11. **Lỗi nhiệt độ** lấy từ error code `0x10`…`0x12` của `FE43`; Temperature `-32768` hiển thị là "chưa có".
12. **Bit reserved và enum lạ**: bỏ qua bit reserved; enum lạ hiển thị `Unknown (0x..)`.
13. **Không dùng** `FE44`, `FE46`, `FE47` cho tính năng người dùng ở protocol v1. Ngoại lệ: gói LoRa status `0x20` của `FE46`, chỉ dành cho màn hình test LoRa. Gói `FE46` có byte `0` khác `0x20` thì giữ dạng raw.

Bản cài đặt tham chiếu của các decoder là `lib/protocol/wearableProtocol.ts` trong repo webapp.

---

## 15. Test vector

Dùng chung cho unit test của decoder phía webapp và để so khi debug firmware.

### 15.1 CONTROL (app → thiết bị)

| Lệnh | Byte |
|---|---|
| Start measurement | `01 00 00 00 00 00 00 00` |
| Stop measurement | `02 00 00 00 00 00 00 00` |
| ECG start | `06 00 00 00 00 00 00 00` |
| Sync time, `1724833200` giây, `500` ms | `09 B0 DD CE 66 F4 01 00` |
| Sync time, `1790760000` giây (2026-09-30 09:20:00 UTC), `250` ms | `09 40 D4 BC 6A FA 00 00` |
| Start BLE recovery từ sequence `1` | `0B 01 00 00 00 00 00 00` |
| Recovery ACK sequence `300` | `0D 2C 01 00 00 00 00 00` |
| LoRa join | `0F 00 00 00 00 00 00 00` |
| LoRa test uplink | `10 00 00 00 00 00 00 00` |
| LoRa TX power −5 dBm | `12 FB 00 00 00 00 00 00` |
| LoRa TX power 14 dBm | `12 0E 00 00 00 00 00 00` |

### 15.2 SENSOR_DATA

```text
4B 61 44 0E 30 0C 01 40 F4 FF 23 00 E6 03 00 FB
```

| Trường | Byte | Giá trị |
|---|---|---|
| Heart rate | `4B` | 75 bpm |
| SpO2 | `61` | 97 % |
| Temperature | `44 0E` | 3652 → 36.52 °C |
| Supercap | `30 0C` | 3120 mV |
| Power state | `01` | Normal |
| Flags | `40` | `WEAR_DETECTED` |
| Accel X | `F4 FF` | −12 mg |
| Accel Y | `23 00` | 35 mg |
| Accel Z | `E6 03` | 998 mg |
| QVar raw | `00 FB` | −1280 |

Gói ngay sau khi kết nối lần đầu, chưa đo lần nào (nhiệt độ chưa hợp lệ):

```text
48 62 00 80 E4 0C 01 00 00 00 00 00 00 00 00 00
```

Nhịp tim 72, SpO2 98, Temperature `-32768` (không hợp lệ), supercap 3300 mV, Normal, không cờ, gia tốc và QVar bằng 0.

### 15.3 DEVICE_STATUS

| Byte | Decode |
|---|---|
| `01 01 00 01 E4 0C 00 41` | Measuring, sensor ready, không lỗi, Normal, 3300 mV, reset 0, cờ `WEAR_DETECTED`, protocol v1 |
| `05 01 01 01 E4 0C 00 01` | Error, sensor ready, lỗi `0x01` (lệnh không hợp lệ), Normal, 3300 mV, không cờ, v1 |
| `02 01 11 01 30 0C 00 61` | ECG active, sensor ready, lỗi `0x11` (timeout nhiệt độ), Normal, 3120 mV, cờ `WEAR_DETECTED` + `ECG_ACTIVE`, v1 |

### 15.4 ECG_DATA

```text
2A 09 0C 00 FB FF 82 00 20 03 C0 FE 28 00 00 00 FF FF 07 00
```

Sequence `42`, 9 mẫu: `12, −5, 130, 800, −320, 40, 0, −1, 7`. Mẫu `800` tương ứng khoảng 516 µV ở đầu vào.

### 15.4b DEBUG_DATA, gói LoRa status `0x20`

```text
20 03 00 03 01 00 FF 02 01 00 04 00 00 00 00 00 00 00 00 00
```

State `3` (đã join), TX power cap `0` dBm, sự kiện gần nhất `3` (TX done), TX `1` (đã gửi), mã lỗi `0`, byte 6 `0xFF` (sự kiện modem, không phải trả lời lệnh), modem đã khởi động, 1 uplink, 4 sự kiện, 0 downlink, không có lỗi radio.

```text
20 FF 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00
```

Firmware build với `LBM_APP_ENABLE = 0`: state `0xFF`, kết quả `0x01`.

### 15.5 RECOVERY_DATA (layout, chưa có trên thiết bị thật)

```text
2C 01 40 D4 BC 6A 4B 61 44 0E 30 0C 01 40 F4 FF 23 00 E6 03 00 FB 53 A3
```

Sequence `300`, timestamp `1790760000`, sensor payload giống mục 15.2, CRC `0xA353` (CRC-16/MODBUS của 22 byte đầu).

---

## 16. Kênh phụ: NFC mailbox và LoRaWAN

### 16.1 NFC mailbox (ST25DV04K)

**[DỞ]** Layout dưới đây mô tả code hiện tại, **chưa chốt**, và chưa được thử trên phần cứng.

Thiết bị dùng Fast Transfer Mode (mailbox 256 byte) của ST25DV04K. Web NFC của trình duyệt chỉ đọc/ghi NDEF, không gửi được lệnh mailbox, nên **webapp không dùng được kênh này**; nó dành cho app native hoặc đầu đọc ST25.

Khung lệnh và phản hồi:

| Hướng | Byte 0 | Byte 1 | Byte 2… |
|---|---|---|---|
| Đầu đọc → thiết bị | Mã lệnh | Tham số… | |
| Thiết bị → đầu đọc | Lặp lại mã lệnh | `0x00` = ACK, `0xFF` = lỗi | Dữ liệu |

| Mã | Lệnh | Tham số | Dữ liệu phản hồi |
|---:|---|---|---|
| `0x01` | Get config | — | Cấu hình, 32 byte |
| `0x02` | Set config | Cấu hình, 32 byte | — |
| `0x03` | Get status | — | Chưa xử lý, trả `0xFF` |
| `0x10` | Get log info | — | Header log, 16 byte |
| `0x11` | Get log | — | `uint16` tổng số record trong log, rồi tối đa 10 record cũ nhất, mỗi record 24 byte |
| `0x12` | Clear log | — | — |

Mọi cấu trúc dưới đây được firmware encode/decode từng byte theo offset cố định (`NFC_Config_Encode/Decode` trong `nfc_config.c`, các hàm `Log_*` trong `nfc_log.c`), little-endian, và dùng chung một layout cho cả EEPROM lẫn mailbox. Các struct C (`NFC_Config_t`, `NFC_LogHeader_t`, `NFC_SensorRecord_t`) chỉ là bản trong RAM, không được chép thẳng ra tag hay mailbox.

Cấu hình (32 byte):

| Byte | Kiểu | Trường | Mặc định | Firmware có dùng |
|---:|---|---|---|:---:|
| 0 | `uint8` | Version | `1` | Có (kiểm tra lúc boot) |
| 1 | — | Reserved | `0` | |
| 2–3 | `uint16` | HR interval, giây | `60` | Có: chu kỳ ghi log khi không có BLE |
| 4–5 | `uint16` | SpO2 interval, giây | `60` | Không |
| 6–7 | `uint16` | Temperature interval, giây | `60` | Không |
| 8–9 | `uint16` | BLE interval, ms | `1000` | Không (chu kỳ BLE đang cố định 1 giây) |
| 10 | `uint8` | Ngưỡng SpO2, % | `90` | Không |
| 11 | — | Reserved | `0` | |
| 12–13 | `int16` | Ngưỡng nhiệt độ, centi-°C | `3800` | Không |
| 14 | `uint8` | Power mode | `0` | Không |
| 15–29 | — | Reserved | `0` | |
| 30–31 | `uint16` | CRC-16/MODBUS của byte `0–29` | | Kiểm tra lúc boot. Với `Set config`, firmware bỏ qua CRC đầu đọc gửi và tự tính lại khi lưu |

Cấu hình mặc định: `01 00 3C 00 3C 00 3C 00 E8 03 5A 00 D8 0E 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1C 02`.

Header log (16 byte):

| Byte | Kiểu | Trường | Ghi chú |
|---:|---|---|---|
| 0–3 | `uint32` | `base_timestamp` | Chưa dùng, luôn `0` |
| 4–5 | `uint16` | `interval_s` | Luôn `60`, chưa nối với cấu hình |
| 6–7 | `uint16` | `write_index` | Ô sẽ ghi tiếp theo, `0…17` |
| 8–9 | `uint16` | `record_count` | `0…18` |
| 10–11 | `uint16` | `newest_sequence` | `0` khi log rỗng |
| 12–13 | `uint16` | `oldest_sequence` | `newest_sequence − record_count + 1` |
| 14–15 | `uint16` | CRC-16/MODBUS của byte `0–13` | Sai CRC hoặc index ngoài phạm vi lúc boot: log bị xóa |

Record (24 byte), trùng từng byte với gói `RECOVERY_DATA` ([mục 11.1](#111-byte-layout)):

| Byte | Kiểu | Trường |
|---:|---|---|
| 0–1 | `uint16` | Sequence, do `NFC_Log_Add` gán, bắt đầu từ `1` sau khi xóa log |
| 2–5 | `uint32` | Timestamp, Unix time, giây |
| 6–21 | 16 byte | Sensor payload, cùng layout với `SENSOR_DATA` |
| 22–23 | `uint16` | CRC-16/MODBUS của byte `0–21` |

Bản đồ EEPROM. Bộ nhớ người dùng của ST25DV04K là 512 byte, địa chỉ I2C `0x0000`–`0x01FF` (datasheet DS10925, bảng 3):

| Địa chỉ | Kích thước | Nội dung |
|---|---:|---|
| `0x0000`–`0x001F` | 32 | Cấu hình |
| `0x0020`–`0x002F` | 16 | Header log |
| `0x0030`–`0x003F` | 16 | Không dùng |
| `0x0040`–`0x01EF` | 432 | 18 record × 24 byte, bộ đệm vòng |
| `0x01F0`–`0x01FF` | 16 | Không dùng |

Các giới hạn vùng nhớ được kiểm tra lúc biên dịch (`_Static_assert` trong `nfc_log.c`).

Các lỗi lưu trữ đã sửa trong code. Phần sửa đã biên dịch và chạy thử trên PC với tag giả lập, **chưa chạy trên phần cứng**:

- Dữ liệu trước đây được đọc/ghi bằng `ST25DV_ReadRegister/WriteRegister`, tức vùng **cấu hình hệ thống** của tag (địa chỉ I2C `0xAE`), không phải bộ nhớ người dùng. Ghi vào vùng đó cần mở I2C security session nên bị tag từ chối: chưa có gì từng được lưu. Nay dùng bộ nhớ người dùng (`0xA6`) qua `NFC_IO_ReadUserMemory/WriteUserMemory`.
- Struct cấu hình dài 34 byte do padding, 2 byte CRC của nó đè lên 2 byte đầu của header log. Nay là 32 byte cố định.
- CRC của header log được tính trên cả chính trường CRC. Nay tính trên byte `0–13`.
- Record trong RAM dài 28 byte nhưng chỉ 24 byte đầu được ghi, nên CRC không được lưu; CRC lại tính trên 22 byte đầu của struct, cắt ngang sensor data; sequence không được gán. Nay record có layout 24 byte ở trên.
- `Get log` chép record theo bước 28 byte vào bộ đệm 256 byte, tràn từ 10 record trở lên. Nay giới hạn ở 10 record × 24 byte.
- 19 record × 24 byte vượt quá 512 byte. Nay là 18 record.

Hạn chế còn lại:

- Tag xuất xưởng có `MB_MODE = 0` (cấm bật Fast Transfer Mode). Firmware không ghi `MB_MODE` (cần I2C password), nên trên tag chưa cấu hình, `ST25DV_SetMBEN_Dyn` không có tác dụng và **mailbox không nhận được lệnh nào**.
- Tag từ chối ghi EEPROM khi mailbox đang bật (`MB_EN = 1`). `NFC_IO_WriteUserMemory` tắt mailbox, ghi, rồi bật lại; việc tắt làm **mất message đang nằm trong mailbox** mà bên kia chưa đọc.
- `Get log` chỉ trả 10 record cũ nhất; chưa có cách đọc phần còn lại qua NFC (cần giao thức chia gói). Record sai CRC bị bỏ qua, nên số record trong phản hồi tính theo độ dài message, không theo trường `uint16` đầu.
- Cấu hình nằm ở block 0 của bộ nhớ người dùng, chỗ của CC file NFC Forum Type 5. Tag sẽ không còn nội dung NDEF hợp lệ.
- Đọc I2C lỗi lúc boot (ví dụ đang có trường RF) vẫn làm firmware coi log là rỗng và ghi đè header.
- Mỗi record ghi 24 byte record và 16 byte header, khoảng 50 ms chờ EEPROM (5 ms cho mỗi trang 4 byte), chạy chặn trong task cảm biến.

### 16.2 LoRaWAN uplink

LoRaWAN **không tự chạy**: SX1262 không phát gì cho tới khi app gửi lệnh LoRa join `0x0F` ([mục 5.3](#53-chi-tiết-từng-lệnh)). Sau khi join, firmware mặc định **không** gửi uplink định kỳ (`LBM_UPLINK_PERIOD_S = 0`); uplink chỉ đi khi app gửi `0x10` hoặc khi nhấn nút SOS. Công suất phát mặc định bị giới hạn ở `0` dBm (`LBM_TX_POWER_MAX_DBM`, đổi bằng `0x12`). Firmware build với `LBM_APP_ENABLE = 0` thì không có LoRa: mọi lệnh LoRa trả kết quả `0x01`. Trạng thái LoRa đi qua `FE46` ([mục 10.1](#101-gói-0x20-lora-status-dở)); dữ liệu uplink thì webapp hiện **không** nhận. Nếu sau này webapp lấy dữ liệu qua The Things Stack (webhook/MQTT) thì dùng định dạng sau. Payload LoRaWAN dùng **big-endian**.

| FPort | Khi nào | Loại | Payload |
|---:|---|---|---|
| `101` | Lệnh `0x10`; hoặc định kỳ nếu `LBM_UPLINK_PERIOD_S` > 0 | Unconfirmed | 4 byte: bộ đếm uplink, `uint32` BE, gói đầu là `0` |
| `102` | Nhấn nút SOS (PB5) | Confirmed, emergency | 5 byte: `0x01` (mã sự kiện SOS), rồi số lần nhấn kể từ khi khởi động, `uint32` BE |

Nút SOS hiện chỉ gửi qua LoRaWAN; nó **không** bật cờ `EMERGENCY` hay state `Emergency` trên BLE. Payload formatter cho The Things Stack nằm trong `README.md`.

---

## 17. Hạn chế và điểm lệch đã biết

### 17.1 Firmware

| # | Nội dung | Ảnh hưởng tới app |
|---|---|---|
| F1 | Reset counter (`FE43` byte 6) luôn `0` | Không dùng được để phát hiện thiết bị reset |
| F2 | Cờ `EMERGENCY` và `FALL_CANDIDATE` không có đường xóa ngoài reset | Sau `0x08`, cờ `0x10` còn mãi trong phiên |
| F3 | `Emergency` và `Error` không tắt cảm biến / luồng ECG | App gửi `0x02` để dọn |
| F4 | `power state` chỉ đổi theo lệnh; `power_policy` (profile theo điện áp supercap) chưa được nối vào | Không suy ra mức năng lượng từ `power state` |
| F5 | Nhịp tim giả khi MAX86150 không chạy, không có cờ hợp lệ | Xem mục 14, quy tắc 9 |
| F6 | Không có bảo mật tầng link | Bất kỳ central nào ở gần cũng gửi được lệnh, kể cả `0x0E` |
| F7 | Recovery chưa chạy end-to-end | Xem mục 11.3 |
| F8 | `FE44` là stub; `FE46` mới có gói LoRa status `0x20`, các lệnh debug khác chưa làm | Xem mục 9, 10 |
| F9 | Lỗi layout lưu trữ NFC đã sửa trong code, chưa thử trên phần cứng; mailbox chưa dùng được trên tag mặc định | Xem mục 16.1 |
| F10 | Nút SOS không phản ánh lên BLE | Xem mục 16.2 |
| F11 | Ngưỡng phát hiện đeo chưa hiệu chuẩn, chưa thử trên phần cứng | Cờ `0x40` có thể sai |

### 17.2 WebApp

W1–W4 đã được sửa ở webapp, nhánh `feat/max86150-protocol-v1` (2026-10-01). Giữ lại bảng để tra cứu:

| # | Nội dung | Vị trí | Trạng thái |
|---|---|---|---|
| W1 | Chế độ demo map mã lệnh lệch một đơn vị (coi `0` là bắt đầu đo, `1` là dừng, `4` là low-power, `2` là request data), sót lại từ thời mã lệnh bắt đầu từ `0x00` | `components/wearable-health.tsx`, hàm `send` | Đã sửa: demo chạy theo máy trạng thái mục 12 (`demoStatusAfter`) |
| W2 | `PowerProfile`, `PowerMode`, `PowerConfiguration` không tương ứng với trường nào trên đường truyền; `power state` chỉ là `1` hoặc `2` | `lib/protocol/wearableProtocol.ts` | Đã ghi chú là cấu hình phía app; hiển thị `power state` qua bảng `POWER_STATES` |
| W3 | `CHARACTERISTIC_METADATA.DEBUG_DATA.direction` ghi là `read`, trong khi GATT là Read/Write/Notify | `lib/protocol/wearableProtocol.ts` | Đã sửa: `properties` theo đúng bảng 4.2 |
| W4 | `syncTime()` trả về ngay sau khi ghi, không chờ `FE43` để biết thiết bị chấp nhận hay từ chối | `lib/ble/bleManager.ts` | Đã sửa: `sendCommand()` chờ `FE43`, báo lỗi khi error `0x01` hoặc không có phản hồi sau 2 giây |

---

## 18. Quy tắc thay đổi giao thức

1. **Không đổi** UUID, độ dài, offset, mã lệnh, mask, giá trị enum của các mục **[CHỐT]**. Webapp đã phát hành phải tiếp tục decode được.
2. **Mở rộng tương thích ngược** (không tăng version):
   - lệnh mới: cấp mã tiếp theo từ `0x14`, vẫn 8 byte, tham số little-endian từ byte `1`;
   - cờ mới: dùng bit reserved (`0x01`, `0x02`, `0x04`, `0x80` của `FE42`; `0x80` của `FE43`);
   - state hoặc error code mới: thêm giá trị mới, không đổi nghĩa giá trị cũ;
   - dữ liệu mới: ưu tiên characteristic còn trống (`FE44`, `FE46`) hoặc thêm characteristic mới `FE48`…
3. **Thay đổi phá vỡ tương thích** (đổi độ dài, offset, đơn vị, nghĩa của trường): tăng `WEARABLE_PROTOCOL_VERSION` (tối đa `15` vì chỉ có 4 bit) và webapp phải phân nhánh theo version.
4. Thêm characteristic phải sửa qua `BLE_Wearable_GATT.ioc` rồi sinh lại code, không sửa tay phần ngoài `USER CODE` trong `wearable.c`.
5. Mục **[STUB]** / **[DỞ]** chỉ được chuyển thành **[CHỐT]** khi: layout đã ghi vào tài liệu này, firmware đã chạy trên phần cứng, và webapp đã decode được dữ liệu thật.

Checklist cho mỗi thay đổi giao thức:

- [ ] Cập nhật tài liệu này (byte layout, bảng lệnh, test vector, mục 17).
- [ ] Firmware: `Application/wearable_data.h/.c`, `STM32_BLE/App/wearable_app.c`, và `wearable.c` / `.ioc` nếu đổi GATT.
- [ ] Firmware: `README.md` mục *BLE GATT profile* và các decoder trong đó.
- [ ] WebApp: `lib/protocol/wearableProtocol.ts`, `lib/ble/bleManager.ts`, component hiển thị liên quan.
- [ ] WebApp: `BLE_PROTOCOL.md`.
- [ ] Thử bằng nRF Connect với test vector mới, rồi thử bằng webapp.

---

## 19. Bảng đối chiếu mã nguồn

| Hạng mục | Firmware (`BLE_Wearable_GATT`) | WebApp (`WearableWebApp`) |
|---|---|---|
| UUID service và characteristic | `STM32_BLE/App/wearable.c`: `WEARABLE_UUID`, `CONTROL_UUID`… (sinh từ `.ioc`) | `lib/protocol/wearableProtocol.ts`: `SERVICE_UUID`, `CHARACTERISTICS` |
| Độ dài payload | `Application/wearable_data.h`: `WEARABLE_*_PAYLOAD_LENGTH`; `wearable.c`: `*_SIZE` | `*_LENGTH` trong `wearableProtocol.ts` |
| Mã lệnh CONTROL | `STM32_BLE/App/wearable_app.c`: `wearable_command_t`, nhánh `WEARABLE_CONTROL_WRITE_EVT` | `CONTROL_COMMANDS`, `commandPacket()`, `syncTimePacket()` |
| Encode / decode `SENSOR_DATA` | `Application/wearable_data.c`: `WearableData_EncodeSensor()` | `decodeSensor()` |
| Encode / decode `DEVICE_STATUS` | `WearableData_EncodeStatus()`; `wearable_app.c`: `WEARABLE_RefreshStatusSnapshot()` | `decodeStatus()` |
| Encode / decode `ECG_DATA` | `WearableData_EncodeECG()`; `wearable_app.c`: `WEARABLE_EcgTask()`, `WEARABLE_EcgQueuePacket()` | `decodeECGData()`, `components/ecg-monitor.tsx` |
| Flags | `wearable_data.h`: `WEARABLE_FLAG_*` | `SensorFlags` |
| Measurement state | `Application/StateManager/wearable_state_manager.h`: `wearable_state_t` | Bảng state trong `decodeStatus()` |
| Error code | `wearable_app.c`: `WEARABLE_ERROR_*` | Bảng `errors` trong `decodeStatus()` |
| Protocol version | `wearable_data.h`: `WEARABLE_PROTOCOL_VERSION` | `protocolVersion` trong `decodeStatus()` |
| Thời điểm notify | `wearable_app.c`: `WEARABLE_SensorTask()`, `WEARABLE_MotionInterruptTask()`, `WEARABLE_SendStatusIfChanged()` | — |
| Số liệu cảm biến, cờ đeo | `Application/SensorManager/sensor_manager.c` | `lib/qvar/qvarModule.ts` (chỉ hiển thị) |
| Đồng bộ thời gian | `Application/DeviceTime/device_time.c` | `BleManager.syncTime()` |
| Recovery | `Application/DataRecovery/data_recovery_manager.c`, `Application/NFC/nfc_log.c` | `decodeRecoveryData()`, `lib/wearableRepository.ts` |
| GAP, advertising, bảo mật | `Core/Inc/app_conf.h`, `STM32_BLE/App/app_ble.c` | `BleManager.scan()`, `BleManager.connect()` |
| NFC mailbox | `Application/NFC/nfc_manager.c`, `nfc_config.h`, `nfc_log.h` | — |
| LoRaWAN uplink | `Application/LoRaWAN/lbm_app.c`, `lbm_config.h` | — |
| Lệnh test LoRa, gói `FE46` `0x20` | `wearable_app.c`: `WEARABLE_LoraTask()`, `WEARABLE_SendLoraStatus()`; `wearable_data.c`: `WearableData_EncodeLoraStatus()`; `lbm_app.c`: `LBM_App_Join()`… | `LORA_COMMANDS`, `loraCommandPacket()`, `decodeLoraStatus()`, `components/lora-test.tsx` |
