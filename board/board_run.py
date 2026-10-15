#!/usr/bin/env python3
"""board_run.py —— 板上执行 / 传文件(宿主机侧)。

为什么需要它:宿主**没有 sshpass**,只能走 paramiko;而以前的临时脚本都放
`/tmp/runboard.py` / `/tmp/qb.py`,**重启即丢**(踩过)。本文件是持久版。

用法
----
    export QRS_BOARD_PASS=<你的板子口令>          # ⛔ 必填,见下
    python3 board/board_run.py "tail -50 /var/log/qrs-ui.log"     # 跑一条命令
    python3 board/board_run.py --put tools/relay_rw.py /home/xilinx/relay_rw.py
    python3 board/board_run.py --get /var/log/qrs-ui.log ./ui.log
    python3 board/board_run.py --shell                            # 简单交互

⛔ 本脚本只做 SSH —— **不碰位流、不调 Overlay()**。
   任何会重载位流的动作在上电周期内都不安全(见 docs/dev_handbook.md §7 红线 2)。

🔴 **口令从环境变量读,不写死在源码里。**
   本文件是**公开交付仓库**的一部分 —— 把口令提交进去就等于**发布**它,
   而且提交过一次就永久留在 git 历史里,改文件也删不掉。
   主机 / 用户名 / 端口只是网络拓扑,有默认值;口令**没有默认值**。

📌 板上脚本应放 `/home/xilinx/`;`/tmp` 重启即丢。
"""

from __future__ import annotations

import argparse
import os
import shlex
import sys
import time

# 网络拓扑可以有默认值(不是秘密);口令不可以 —— 见文件头 🔴
HOST = os.environ.get("QRS_BOARD_HOST", "192.168.2.99")
USER = os.environ.get("QRS_BOARD_USER", "xilinx")
PASS = os.environ.get("QRS_BOARD_PASS")          # ⛔ 无默认值
PORT = int(os.environ.get("QRS_BOARD_PORT", "22"))

try:
    import paramiko
except ImportError:
    sys.exit("缺 paramiko:  pip3 install paramiko")


def connect(timeout: float = 8.0):
    if not PASS:                      # 🔴 口令不入库 ⇒ 用之前必须由使用者自己提供
        sys.exit("[board_run] 未设置口令。请先:  export QRS_BOARD_PASS=<你的板子口令>\n"
                 "            (本脚本刻意不内置口令 —— 交付仓库是公开的,写进去就等于发布它。)")
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        c.connect(
            HOST, port=PORT, username=USER, password=PASS,
            timeout=timeout, banner_timeout=timeout, auth_timeout=timeout,
            look_for_keys=False, allow_agent=False,
        )
    except Exception as e:  # 板子没上电 / 网线没插 —— 先报清楚
        sys.exit(f"[board_run] 连不上 {USER}@{HOST}:{PORT} —— {e}\n"
                 f"            板子断电?网线?enp2s0 是否有 192.168.2.100?")
    return c


def run(c, cmd: str, timeout: float | None = None) -> int:
    """执行一条命令,stdout/stderr 实时回显。返回退出码。"""
    stdin, stdout, stderr = c.exec_command(cmd, timeout=timeout, get_pty=False)
    # 边读边打,避免长命令(扫压/采集)看着像卡住
    for line in iter(stdout.readline, ""):
        sys.stdout.write(line)
        sys.stdout.flush()
    err = stderr.read().decode("utf-8", "replace")
    if err.strip():
        sys.stderr.write(err)
    rc = stdout.channel.recv_exit_status()
    return rc


def put(c, local: str, remote: str) -> None:
    sftp = c.open_sftp()
    sftp.put(local, remote)
    st = sftp.stat(remote)
    print(f"[board_run] put  {local} -> {remote}  ({st.st_size} B)")
    sftp.close()


def get(c, remote: str, local: str) -> None:
    sftp = c.open_sftp()
    sftp.get(remote, local)
    print(f"[board_run] get  {remote} -> {local}  ({os.path.getsize(local)} B)")
    sftp.close()


def shell() -> None:
    """极简交互:一行一条命令。行内 `exit` / `quit` 退出。"""
    c = connect()
    print(f"[board_run] 已连上 {USER}@{HOST}。逐行执行,`exit` 退出。")
    print("            (需要 sudo 就在命令前自己写 sudo —— 免密)")
    try:
        while True:
            try:
                line = input("board> ")
            except EOFError:
                break
            if line.strip() in ("exit", "quit", ""):
                break
            rc = run(c, line)
            if rc:
                print(f"[board_run] 退出码 {rc}")
    finally:
        c.close()


def main() -> int:
    ap = argparse.ArgumentParser(
        description="板上执行 / 传文件(paramiko,替代 /tmp 里的一次性脚本)")
    ap.add_argument("cmd", nargs="?", help="要执行的命令(加引号)")
    ap.add_argument("--put", nargs=2, metavar=("LOCAL", "REMOTE"), help="上传文件")
    ap.add_argument("--get", nargs=2, metavar=("REMOTE", "LOCAL"), help="取回文件")
    ap.add_argument("--shell", action="store_true", help="简单交互模式")
    ap.add_argument("--timeout", type=float, default=None,
                    help="命令超时秒数(默认不限;扫压/采集类建议给)")
    a = ap.parse_args()

    if a.shell:
        shell()
        return 0

    c = connect()
    try:
        if a.put:
            put(c, *a.put)
            return 0
        if a.get:
            get(c, *a.get)
            return 0
        if not a.cmd:
            ap.print_help()
            return 2
        t0 = time.time()
        rc = run(c, a.cmd, timeout=a.timeout)
        print(f"[board_run] 退出码 {rc} · {time.time() - t0:.1f} s")
        return rc
    finally:
        c.close()


if __name__ == "__main__":
    sys.exit(main())


# ─────────────────────────────────────────────────────────────────────────────
# 附:常用一行命令(本文件顶部注释里也有,这里留一份可复制的)
#
#   tail -100 /var/log/qrs-ui.log                  ★ 界面/触摸/偏压/扫压的唯一证据源
#   sudo python3 /home/xilinx/relay_rw.py --diagnose
#   cd /home/xilinx/c_ui_src && make -j2
#   sudo systemctl start qrs-ui                    ⚠️ 平时必须 disabled
#   sudo python3 /home/xilinx/fb_dump.py           (或宿主机 tools/fb_dump.py)
#   md5sum /home/xilinx/qrs.bit                    ★ 位流身份,别用 mtime(板上无 RTC)
# ─────────────────────────────────────────────────────────────────────────────
