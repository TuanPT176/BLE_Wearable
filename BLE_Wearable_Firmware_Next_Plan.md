# Kế hoạch phát triển tiếp theo — BLE Wearable Firmware trên STM32WB09

## 1. Trạng thái hiện tại

Project đã hoàn thành nền tảng BLE ban đầu:

- Advertising với tên `BLEWearable`
- Tự advertising lại sau disconnect
- Custom Wearable Health Service
- 3 characteristic:
  - `CONTROL`
  - `SENSOR_DATA`
  - `DEVICE_STATUS`
- Hỗ trợ Write command
- Hỗ trợ Notify độc lập cho sensor data và device status
- Có virtual timer và sequencer task
- Mock sensor data gửi định kỳ
- Project build thành công

Các phần chưa hoàn thiện:

- GATT Read response hoàn chỉnh
- Driver cảm biến thật
- ADC đo điện áp supercap
- ECG thật
- Reset counter lưu bền vững
- Deep sleep và power state thực tế

---

# 2. Mục tiêu giai đoạn tiếp theo

Ưu tiên phát triển theo thứ tự:

```text
1. Hoàn thiện BLE hiện tại
2. Tách application logic khỏi mock data
3. Đọc ADC điện áp supercap thật
4. Tích hợp driver cảm biến từng bước
5. Hoàn thiện power manager
6. Tích hợp ECG
7. Lưu trạng thái bền vững
8. Kiểm thử độ ổn định và đo năng lượng
```

Không nên tích hợp tất cả cảm biến cùng lúc.

---

# Giai đoạn 1 — Hoàn thiện BLE GATT hiện tại

## Task 1.1 — Hoàn thiện GATT Read cho SENSOR_DATA

### Mục tiêu

Khi điện thoại nhấn Read trên `SENSOR_DATA`, firmware phải trả về dữ liệu mới nhất.

### Yêu cầu

- Duy trì một buffer dữ liệu sensor hiện tại
- Khi có Read request:
  - Copy dữ liệu mới nhất vào characteristic
  - Trả về đúng chiều dài payload
- Không tạo dữ liệu mới trực tiếp trong callback Read

### Acceptance criteria

- nRF Connect đọc được đủ 16 byte
- Payload giống giá trị notification gần nhất
- Không lỗi khi Read liên tục nhiều lần

---

## Task 1.2 — Hoàn thiện GATT Read cho DEVICE_STATUS

### Mục tiêu

Điện thoại đọc được trạng thái hệ thống hiện tại.

### Acceptance criteria

- Read trả đủ 8 byte
- Trạng thái phản ánh đúng:
  - measurement state
  - sensor ready
  - error code
  - power state
  - supercap voltage
  - flags

---

## Task 1.3 — Chuẩn hóa command protocol

### Mục tiêu

Tránh nhầm lẫn command về sau.

### Đề xuất

```text
0x01 Start Measurement
0x02 Stop Measurement
0x03 Request Current Data
0x04 Set Normal Power Mode
0x05 Set Low-Power Mode
0x06 Start ECG
0x07 Stop ECG
0x08 Trigger Emergency Test
```

Hiện tại command bắt đầu từ `0x00`. Có thể giữ nguyên nếu muốn tương thích, nhưng nên chốt protocol sớm.

### Codex cần làm

- Tạo enum command rõ ràng
- Không dùng số hex rải rác trong code
- Kiểm tra payload length trước khi đọc
- Ignore command không hợp lệ
- Set error code nếu command sai

### Acceptance criteria

- Mỗi command có một enum duy nhất
- Không còn magic number trong switch-case
- Command sai không làm reset firmware

---

## Task 1.4 — Tạo protocol version

Thêm version vào `DEVICE_STATUS`, ví dụ:

```text
Byte 7 lower nibble = protocol version
```

Hoặc tạo macro:

```c
#define WEARABLE_PROTOCOL_VERSION 0x01
```

Mục tiêu là app điện thoại biết cách decode payload sau này.

---

# Giai đoạn 2 — Tách mock data khỏi BLE logic

## Task 2.1 — Tạo data model chung

Tạo file:

```text
Application/wearable_data.h
Application/wearable_data.c
```

Định nghĩa:

```c
typedef struct
{
    uint8_t  heart_rate_bpm;
    uint8_t  spo2_percent;
    int16_t  temperature_centi_c;
    uint16_t supercap_mv;
    uint8_t  power_state;
    uint8_t  flags;
} wearable_sensor_data_t;
```

Và:

```c
typedef struct
{
    uint8_t  measurement_state;
    uint8_t  sensor_ready;
    uint8_t  error_code;
    uint8_t  power_state;
    uint16_t supercap_mv;
    uint8_t  reset_counter;
    uint8_t  flags;
} wearable_device_status_t;
```

---

## Task 2.2 — Tạo SensorManager abstraction

Tạo:

```text
Application/sensor_manager.c
Application/sensor_manager.h
```

API đề xuất:

```c
bool SensorManager_Init(void);
bool SensorManager_Start(void);
bool SensorManager_Stop(void);
bool SensorManager_GetLatestData(wearable_sensor_data_t *data);
void SensorManager_Process(void);
```

Hiện tại SensorManager vẫn trả mock data.

Sau này chỉ thay phần implementation.

### Acceptance criteria

- BLE code không tự sinh mock HR/SpO2 nữa
- BLE chỉ gọi SensorManager
- Có thể thay mock bằng sensor thật mà không sửa GATT code

---

## Task 2.3 — Tạo WearableStateManager

Tạo state:

```text
IDLE
MEASURING
ECG_ACTIVE
LOW_POWER
EMERGENCY
ERROR
```

API:

```c
void WearableState_Set(...);
wearable_state_t WearableState_Get(void);
```

### Acceptance criteria

- Command CONTROL thay đổi state
- DEVICE_STATUS phản ánh đúng state
- Timer chỉ chạy khi state cho phép

---

# Giai đoạn 3 — Đọc điện áp supercap bằng ADC

Đây nên là phần phần cứng thật đầu tiên vì đơn giản hơn cảm biến y sinh.

## Task 3.1 — Cấu hình ADC

Cấu hình trong CubeMX:

- ADC channel nối tới cầu chia áp supercap
- Sampling time phù hợp trở kháng nguồn
- Không để ADC hoạt động liên tục
- Dùng single conversion hoặc low-rate periodic sampling

## Task 3.2 — Tạo driver voltage monitor

Tạo:

```text
Drivers/supercap_monitor.c
Drivers/supercap_monitor.h
```

API:

```c
bool SupercapMonitor_Init(void);
uint16_t SupercapMonitor_ReadMillivolts(void);
```

## Task 3.3 — Quy đổi ADC sang điện áp thật

Cần tính:

```text
Vsupercap = Vadc × divider_ratio
```

Thông số resistor divider phải được cấu hình bằng macro.

## Task 3.4 — Thêm lọc đơn giản

Dùng:

- Moving average 4–8 mẫu
- Hoặc median 3 mẫu

Không cần thuật toán phức tạp.

## Acceptance criteria

- Sai số đo dưới mức mục tiêu đã đặt
- Giá trị được cập nhật vào SENSOR_DATA và DEVICE_STATUS
- Không làm tăng dòng tiêu thụ đáng kể
- ADC không chạy liên tục

---

# Giai đoạn 4 — Tích hợp cảm biến nhiệt độ đầu tiên

Nhiệt độ là cảm biến dễ nhất để bring-up.

## Task 4.1 — Chọn driver target

Theo cấu hình phần cứng cuối cùng:

- MAX30208CLB+
- Hoặc sensor nhiệt độ thực tế trên PCB

## Task 4.2 — Viết driver cơ bản

API:

```c
bool TempSensor_Init(void);
bool TempSensor_TriggerOneShot(void);
bool TempSensor_ReadCentiC(int16_t *temperature);
bool TempSensor_Sleep(void);
```

## Task 4.3 — Test độc lập

- Read device ID nếu có
- One-shot measurement
- So sánh với nhiệt kế tham chiếu
- Test disconnect/reconnect BLE không ảnh hưởng sensor

## Acceptance criteria

- Đọc nhiệt độ ổn định
- Không dùng polling blocking dài
- Dữ liệu thay mock temperature

---

# Giai đoạn 5 — Tích hợp ST1VAFE3BX phần IMU trước

Không nên bật ECG ngay.

## Task 5.1 — Bring-up giao tiếp

- SPI hoặc I2C theo schematic cuối cùng
- Đọc WHO_AM_I
- Reset software
- Kiểm tra register read/write

## Task 5.2 — Accelerometer basic

- Cấu hình ODR thấp
- Đọc XYZ
- Verify orientation
- Gửi dữ liệu raw qua debug log

## Task 5.3 — Interrupt wake-up

- Cấu hình motion interrupt
- MCU ngủ
- Cử động tay làm wake-up
- Không đọc liên tục nếu không cần

## Task 5.4 — Fall candidate event

Giai đoạn đầu chỉ cần:

- free-fall interrupt
- impact threshold
- post-event confirmation đơn giản

Chưa cần MLC ngay.

## Acceptance criteria

- WHO_AM_I đúng
- Interrupt hoạt động
- MCU wake được từ sensor event
- Không có polling liên tục

---

# Giai đoạn 6 — Tích hợp PPG / HR / SpO2

## Task 6.1 — Driver bring-up

- Device ID
- Reset
- FIFO
- Interrupt data-ready
- LED current configuration

## Task 6.2 — Raw PPG streaming mode

Ban đầu chỉ:

- Read raw samples
- Gửi debug qua UART hoặc BLE ở tốc độ thấp
- Không tính HR/SpO2 ngay

## Task 6.3 — HR extraction

- Tạo buffer cửa sổ
- DC removal
- Peak detection
- Tính BPM

## Task 6.4 — SpO2

Chỉ thực hiện sau khi raw red/IR ổn định.

## Task 6.5 — Adaptive sampling

Theo power state:

```text
NORMAL      sampling thấp
SURPLUS     sampling cao hơn
LOW_POWER   PPG tắt hoặc duty cycle thấp
```

## Acceptance criteria

- FIFO không overflow
- Dữ liệu raw ổn định
- HR hợp lý trong thử nghiệm tĩnh
- Power manager có thể bật/tắt PPG

---

# Giai đoạn 7 — Tích hợp ECG thật

## Task 7.1 — vAFE bring-up

- Cấu hình ST1VAFE3BX vAFE
- Đọc raw ECG
- Kiểm tra electrode connection
- Kiểm tra saturation

## Task 7.2 — ECG state riêng

ECG chỉ chạy khi:

- command Start ECG
- anomaly trigger
- emergency
- manual test

## Task 7.3 — ECG buffer

Dùng ring buffer.

Không gửi từng sample riêng lẻ nếu không cần.

## Task 7.4 — ECG transport design

Không nhét ECG waveform vào `SENSOR_DATA` 16 byte.

Đề xuất sau này thêm characteristic riêng:

```text
ECG_DATA
Properties: Notify
Length: 20–244 byte tùy MTU
```

Hoặc thêm service riêng.

## Acceptance criteria

- ECG raw đọc ổn định
- Không block BLE
- Không overflow buffer
- ECG start/stop hoạt động bằng CONTROL command

---

# Giai đoạn 8 — Hoàn thiện Power Manager

## Task 8.1 — State machine năng lượng

Đề xuất:

```text
COLD_START
DEPLETED
NORMAL
SURPLUS
EMERGENCY
```

## Task 8.2 — Hysteresis

Không đổi state liên tục quanh một ngưỡng.

Ví dụ:

```text
NORMAL -> DEPLETED khi Vcap < V_LOW
DEPLETED -> NORMAL khi Vcap > V_RECOVER
```

với:

```text
V_RECOVER > V_LOW
```

## Task 8.3 — Action theo state

### DEPLETED

- Stop PPG
- Stop ECG
- BLE advertising chậm hoặc off
- Chỉ giữ motion wake-up

### NORMAL

- Sensor duty cycle bình thường
- BLE notify định kỳ

### SURPLUS

- Sampling cao hơn
- Đồng bộ dữ liệu thường xuyên hơn

### EMERGENCY

- Ưu tiên BLE / LoRa alert
- Sau đó quay về state phù hợp năng lượng

## Acceptance criteria

- State thay đổi đúng theo voltage
- Không oscillate giữa hai state
- BLE payload phản ánh đúng power state

---

# Giai đoạn 9 — Reset counter và persistence

## Task 9.1 — Reset counter

Không nên ghi flash mỗi lần loop.

Chỉ update khi boot.

## Task 9.2 — Lưu flash an toàn

Tùy STM32WB09 support:

- Flash page riêng
- EEPROM emulation
- Hoặc NVM service có sẵn

## Task 9.3 — Wear leveling tối thiểu

Không ghi cùng một cell quá thường xuyên.

## Acceptance criteria

- Reset counter tăng sau reset
- Power cycle không mất
- Không ghi flash liên tục

---

# Giai đoạn 10 — Low-power thật

## Task 10.1 — Tắt low-power debug mode

Chỉ bật sau khi chức năng ổn định.

## Task 10.2 — Sleep policy

MCU ngủ khi:

- không có task pending
- không có BLE transaction đang chờ
- không có sensor read pending

## Task 10.3 — Wake sources

- BLE radio
- RTC / virtual timer
- ST1VAFE3BX interrupt
- Button
- ADC threshold nếu dùng comparator ngoài

## Task 10.4 — Đo dòng

Đo các mode:

```text
Advertising
Connected idle
Sensor notify 1 Hz
Measurement active
ECG active
Low power
Disconnected sleep
```

## Acceptance criteria

- Không mất BLE
- Không mất sensor interrupt
- Dòng giảm rõ so với baseline
- Wake-up ổn định

---

# Giai đoạn 11 — Test matrix

## BLE

- Connect/disconnect 50 lần
- Enable/disable notify nhiều lần
- Gửi command sai
- Read liên tục
- Reset khi đang connected
- Điện thoại ra khỏi vùng phủ

## Sensor

- Sensor missing
- I2C/SPI timeout
- FIFO overflow
- Invalid sample
- Sensor reset

## Power

- Vcap giảm qua threshold
- Brownout
- Cold start
- Recover từ low power
- Emergency khi năng lượng thấp

## Long-run

- Chạy tối thiểu 8 giờ
- Sau đó 24 giờ
- Không reset ngoài ý muốn
- Không mất notification bất thường
- Không timer leak

---

# 3. Thứ tự Codex nên triển khai

```text
Phase A
- Hoàn thiện Read
- Chuẩn hóa command
- Refactor data model
- Tách SensorManager

Phase B
- ADC supercap thật
- Power state cơ bản
- DeviceStatus hoàn chỉnh

Phase C
- Temperature sensor
- ST1VAFE3BX accelerometer
- Motion interrupt

Phase D
- PPG raw
- HR
- SpO2

Phase E
- ECG
- ECG streaming characteristic

Phase F
- Flash persistence
- Low power
- Long-run validation
```

---

# 4. Việc nên làm ngay tiếp theo

## Sprint tiếp theo đề xuất

1. Hoàn thiện GATT Read
2. Tách mock data ra khỏi BLE service
3. Tạo `SensorManager`
4. Tạo `WearableStateManager`
5. Đọc supercap voltage bằng ADC thật
6. Cập nhật voltage vào cả SENSOR_DATA và DEVICE_STATUS
7. Test lại bằng nRF Connect

Đây là sprint hợp lý nhất vì vẫn chưa phụ thuộc vào sensor board hoàn chỉnh.

---

# 5. Definition of Done cho sprint tiếp theo

Sprint được xem là hoàn thành khi:

- Build không warning nghiêm trọng
- CONTROL Write hoạt động
- SENSOR_DATA Read và Notify giống nhau
- DEVICE_STATUS Read và Notify giống nhau
- Mock data không còn nằm trực tiếp trong BLE code
- Supercap voltage đọc từ ADC thật
- Disconnect dừng timer
- Reconnect hoạt động
- Không có BLE API gọi trực tiếp từ ISR
- Code custom nằm trong USER CODE hoặc file riêng
