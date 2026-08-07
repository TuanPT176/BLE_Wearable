/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "My Project", "index.html", [
    [ "Kế hoạch phát triển tiếp theo — BLE Wearable Firmware trên STM32WB09", "md__b_l_e___wearable___firmware___next___plan.html", [
      [ "1. Trạng thái hiện tại", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md1", null ],
      [ "2. Mục tiêu giai đoạn tiếp theo", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md3", null ],
      [ "Giai đoạn 1 — Hoàn thiện BLE GATT hiện tại", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md5", [
        [ "Task 1.1 — Hoàn thiện GATT Read cho SENSOR_DATA", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md6", [
          [ "Mục tiêu", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md7", null ],
          [ "Yêu cầu", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md8", null ],
          [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md9", null ]
        ] ],
        [ "Task 1.2 — Hoàn thiện GATT Read cho DEVICE_STATUS", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md11", [
          [ "Mục tiêu", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md12", null ],
          [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md13", null ]
        ] ],
        [ "Task 1.3 — Chuẩn hóa command protocol", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md15", [
          [ "Mục tiêu", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md16", null ],
          [ "Đề xuất", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md17", null ],
          [ "Codex cần làm", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md18", null ],
          [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md19", null ]
        ] ],
        [ "Task 1.4 — Tạo protocol version", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md21", null ]
      ] ],
      [ "Giai đoạn 2 — Tách mock data khỏi BLE logic", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md23", [
        [ "Task 2.1 — Tạo data model chung", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md24", null ],
        [ "Task 2.2 — Tạo SensorManager abstraction", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md26", [
          [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md27", null ]
        ] ],
        [ "Task 2.3 — Tạo WearableStateManager", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md29", [
          [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md30", null ]
        ] ]
      ] ],
      [ "Giai đoạn 3 — Đọc điện áp supercap bằng ADC", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md32", [
        [ "Task 3.1 — Cấu hình ADC", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md33", null ],
        [ "Task 3.2 — Tạo driver voltage monitor", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md34", null ],
        [ "Task 3.3 — Quy đổi ADC sang điện áp thật", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md35", null ],
        [ "Task 3.4 — Thêm lọc đơn giản", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md36", null ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md37", null ]
      ] ],
      [ "Giai đoạn 4 — Tích hợp cảm biến nhiệt độ đầu tiên", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md39", [
        [ "Task 4.1 — Chọn driver target", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md40", null ],
        [ "Task 4.2 — Viết driver cơ bản", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md41", null ],
        [ "Task 4.3 — Test độc lập", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md42", null ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md43", null ]
      ] ],
      [ "Giai đoạn 5 — Tích hợp ST1VAFE3BX phần IMU trước", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md45", [
        [ "Task 5.1 — Bring-up giao tiếp", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md46", null ],
        [ "Task 5.2 — Accelerometer basic", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md47", null ],
        [ "Task 5.3 — Interrupt wake-up", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md48", null ],
        [ "Task 5.4 — Fall candidate event", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md49", null ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md50", null ]
      ] ],
      [ "Giai đoạn 6 — Tích hợp PPG / HR / SpO2", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md52", [
        [ "Task 6.1 — Driver bring-up", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md53", null ],
        [ "Task 6.2 — Raw PPG streaming mode", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md54", null ],
        [ "Task 6.3 — HR extraction", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md55", null ],
        [ "Task 6.4 — SpO2", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md56", null ],
        [ "Task 6.5 — Adaptive sampling", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md57", null ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md58", null ]
      ] ],
      [ "Giai đoạn 7 — Tích hợp ECG thật", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md60", [
        [ "Task 7.1 — vAFE bring-up", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md61", null ],
        [ "Task 7.2 — ECG state riêng", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md62", null ],
        [ "Task 7.3 — ECG buffer", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md63", null ],
        [ "Task 7.4 — ECG transport design", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md64", null ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md65", null ]
      ] ],
      [ "Giai đoạn 8 — Hoàn thiện Power Manager", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md67", [
        [ "Task 8.1 — State machine năng lượng", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md68", null ],
        [ "Task 8.2 — Hysteresis", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md69", null ],
        [ "Task 8.3 — Action theo state", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md70", [
          [ "DEPLETED", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md71", null ],
          [ "NORMAL", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md72", null ],
          [ "SURPLUS", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md73", null ],
          [ "EMERGENCY", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md74", null ]
        ] ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md75", null ]
      ] ],
      [ "Giai đoạn 9 — Reset counter và persistence", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md77", [
        [ "Task 9.1 — Reset counter", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md78", null ],
        [ "Task 9.2 — Lưu flash an toàn", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md79", null ],
        [ "Task 9.3 — Wear leveling tối thiểu", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md80", null ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md81", null ]
      ] ],
      [ "Giai đoạn 10 — Low-power thật", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md83", [
        [ "Task 10.1 — Tắt low-power debug mode", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md84", null ],
        [ "Task 10.2 — Sleep policy", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md85", null ],
        [ "Task 10.3 — Wake sources", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md86", null ],
        [ "Task 10.4 — Đo dòng", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md87", null ],
        [ "Acceptance criteria", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md88", null ]
      ] ],
      [ "Giai đoạn 11 — Test matrix", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md90", [
        [ "BLE", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md91", null ],
        [ "Sensor", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md92", null ],
        [ "Power", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md93", null ],
        [ "Long-run", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md94", null ]
      ] ],
      [ "3. Thứ tự Codex nên triển khai", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md96", null ],
      [ "4. Việc nên làm ngay tiếp theo", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md98", [
        [ "Sprint tiếp theo đề xuất", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md99", null ]
      ] ],
      [ "5. Definition of Done cho sprint tiếp theo", "md__b_l_e___wearable___firmware___next___plan.html#autotoc_md101", null ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"index.html"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';