"""Read g_ecgDiag from a running board over SWD (hot plug, no halt/reset) and decode it.

The address comes from the .map of the build that is flashed. Usage:
    python read_ecg_diag.py [path/to/build.map]
Default map: ../Debug/BLE_p2pServer_GATT.map. Layout: ecg_diag_info_t in
Application/SensorManager/ecg_diag.h (update SIZE and the unpack formats with it).
"""
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "Debug", "BLE_p2pServer_GATT.map")
CLI = os.environ.get(
    "STM32_PROGRAMMER_CLI",
    r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe")
SIZE = 104
MAGIC = 0xEC6D1A60

REG_NAMES = {
    0x08: "FIFO_CONFIG", 0x09: "FIFO_DATA_CTRL1", 0x0A: "FIFO_DATA_CTRL2",
    0x0D: "SYS_CONTROL", 0x0E: "PPG_CONFIG1", 0x0F: "PPG_CONFIG2",
    0x10: "PROX_INT_THRESH", 0x11: "LED1_PA", 0x12: "LED2_PA",
    0x14: "LED_RANGE", 0x15: "LED_PILOT_PA", 0x3C: "ECG_CONFIG1", 0x3E: "ECG_CONFIG3",
}
EXPECTED = {0x08: 0x1F, 0x09: 0x09, 0x0A: 0x00, 0x0D: 0x04, 0x11: 0x00, 0x12: 0x00,
            0x15: 0x00, 0x3C: 0x03, 0x3E: 0x0D}


def find_address():
    text = open(MAP, encoding="utf-8", errors="replace").read()
    m = re.search(r"\.bss\.g_ecgDiag\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)", text)
    if not m:
        sys.exit("g_ecgDiag not in the map - wrong build?")
    if int(m.group(2), 16) != SIZE:
        sys.exit("g_ecgDiag size %s != %d - update this script" % (m.group(2), SIZE))
    return int(m.group(1), 16)


def read_bytes(address):
    out = subprocess.run([CLI, "-c", "port=SWD", "mode=HOTPLUG", "-r8", hex(address), str(SIZE)],
                         capture_output=True, text=True).stdout
    data = []
    for line in out.splitlines():
        m = re.match(r"\s*0x[0-9A-Fa-f]{8}\s*:\s*(.*)", line)
        if m:
            data += [int(b, 16) for b in m.group(1).split()]
    if len(data) < SIZE:
        sys.exit("SWD read failed:\n" + out)
    return bytes(data[:SIZE])


def main():
    address = find_address()
    raw = read_bytes(address)
    magic, interval, updates, req, regs_valid = struct.unpack_from("<IHHBB", raw, 0)
    regs = raw[10:36]
    sessions, overflows, dropped = struct.unpack_from("<III", raw, 36)

    print("g_ecgDiag @ 0x%08X" % address)
    if magic != MAGIC:
        sys.exit("magic 0x%08X != 0x%08X: firmware on the board is not this build" % (magic, MAGIC))
    print("connection interval : %.2f ms (%d x 1.25 ms), %d update(s)" % (interval * 1.25, interval, updates))
    print("diag interval req   : %s" % ("not requested" if req == 0xFF else "status 0x%02X" % req))
    print("ECG sessions        : %d" % sessions)
    print("FIFO overflows      : %d" % overflows)
    print("dropped BLE packets : %d" % dropped)
    (drains, pmin, pmax, psum, pcount, fmin, fmax, fsum, rmin, rmax, rsum,
     ssum, smin, smax, qmax, _rsv, txretry) = struct.unpack_from("<11IIBBBBI", raw, 48)
    if drains:
        print("drain timing (%d timer drains, %d extra TX-pool drains):" % (drains, txretry))
        if pcount:
            print("  real period        : min %.2f  mean %.2f  max %.2f ms" % (pmin / 1e3, psum / pcount / 1e3, pmax / 1e3))
        print("  fire -> I2C done   : min %.2f  mean %.2f  max %.2f ms" % (fmin / 1e3, fsum / drains / 1e3, fmax / 1e3))
        print("  I2C read alone     : min %.2f  mean %.2f  max %.2f ms" % (rmin / 1e3, rsum / drains / 1e3, rmax / 1e3))
        print("  samples per drain  : min %d  mean %.2f  max %d (FIFO depth 32)" % (smin, ssum / drains, smax))
        if pcount and psum:
            print("  implied ECG rate   : %.1f sps" % (ssum / drains / (psum / pcount / 1e6)))
        print("  max packet queue   : %d / 16" % qmax)
    else:
        print("drain timing        : no ECG session yet")
    if not regs_valid:
        print("registers           : not dumped (ECG_DIAG_DUMP_REGS = 0 or no ECG start yet)")
        return
    print("registers (after ECG configure):")
    for i in range(0, len(regs), 2):
        addr, val = regs[i], regs[i + 1]
        if addr == 0xEE and val == 0xEE:
            print("  read FAILED")
            continue
        exp = EXPECTED.get(addr)
        note = "" if exp is None else ("  ok" if val == exp else "  EXPECTED 0x%02X" % exp)
        print("  0x%02X %-16s = 0x%02X%s" % (addr, REG_NAMES.get(addr, "?"), val, note))


if __name__ == "__main__":
    main()
