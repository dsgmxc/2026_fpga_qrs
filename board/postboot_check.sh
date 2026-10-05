#!/bin/bash
# 开机后生产路径核对 —— 2026-10-05
# ⚠️ 本脚本【不加载位流】(那唯一一次由 qrs-ui.service 用掉),只读。
echo "=== 1. 系统 ==="
uptime
echo -n "qrs-ui.service : "; systemctl is-enabled qrs-ui.service; systemctl is-active qrs-ui.service
echo -n "c_ui 进程      : "; pgrep -a c_ui || echo "(无)"
echo -n "main.py 进程   : "; pgrep -af "cv/main.py" || echo "(无)"
echo
echo "=== 2. PL 状态(只读,不加载)==="
ST=$(cat /sys/class/fpga_manager/fpga0/state)
echo "fpga_manager/state = $ST"
if [ "$ST" != "operating" ]; then
  echo "!! PL 未配置 —— 不要读 0x43C00000,会挂死 CPU。到此为止。"
  exit 1
fi
echo -n "dmesg 里 FPGA Manager 行数: "; dmesg 2>/dev/null | grep -c "FPGA Manager"
dmesg 2>/dev/null | grep "FPGA Manager" | tail -3
echo
echo "=== 3. 寄存器(裸 /dev/mem) ==="
python3 - <<'PY'
import mmap, os, struct
def rd(off):
    fd = os.open("/dev/mem", os.O_RDONLY | os.O_SYNC)
    try:
        m = mmap.mmap(fd, 4096, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ, offset=0x43C00000)
        try: return struct.unpack("<I", m[off:off+4])[0]
        finally: m.close()
    finally: os.close(fd)
def slcr(a):
    fd = os.open("/dev/mem", os.O_RDONLY | os.O_SYNC)
    try:
        m = mmap.mmap(fd, 4096, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ, offset=a & ~0xFFF)
        try: return struct.unpack("<I", m[a & 0xFFF:a & 0xFFF + 4])[0]
        finally: m.close()
    finally: os.close(fd)

v = slcr(0xF8000170)
d0 = ((v >> 8) & 0x3F) or 1
d1 = ((v >> 20) & 0x3F) or 1
print(f"FCLK0        = {1000.0/d0/d1:.4f} MHz  (DIV0={d0} DIV1={d1} raw=0x{v:08X})")
print(f"0x00 VERSION = 0x{rd(0x00):08X}")
b = rd(0x14)
print(f"0x14 BIAS    = 0x{b:06X}  -> V_module = {-10 + 20*b/1048575:+.3f} V")
print(f"0x80 DDS     = 0x{rd(0x80):08X}")
cv = rd(0x84)
print(f"0x84 CV_CTRL = 0x{cv:08X}   dac_rst_n = {cv & 1}   "
      f"{'已放行 OK' if cv & 1 else '未放行 —— DAC 不会动'}")
print(f"0x60 LCD_SWAP= 0x{rd(0x60):08X}")
print(f"0x68 TOUCH   = 0x{rd(0x68):08X}")
print(f"0x74 DBG0    = 0x{rd(0x74):08X}")
PY
echo
echo "=== 4. qrs-ui 服务日志尾部 ==="
journalctl -u qrs-ui.service -n 30 --no-pager 2>/dev/null | tail -30
