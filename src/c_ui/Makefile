# ============================================================================
#  c_ui —— PYNQ 上的 LVGL 界面进程
# ============================================================================
#
#  📌 **在板上原生编译**(Zynq 的 A9 上直接 make),不做交叉编译。
#     理由:前序 LVGL 8.2 是按 STM32 的裸机环境配的,交叉编译要自己搭
#     sysroot 并且和板上的 glibc 版本对齐(风险 R4);板上 `apt install
#     build-essential` 之后直接编,这些坑一个都不用踩。代价是 A9 上慢 ——
#     一次性成本。
#
#  用法:
#      make -j2          # A9 是双核,-j2 就够;别 -j8,内存扛不住
#      ./c_ui --help
#      make clean
#
#  ⚠️ **必须用 `-std=gnu11`,不能写 `-std=c11`。**
#     用 c11 时会报 `implicit declaration of function 'clock_gettime'` /
#     `'nanosleep'`,`CLOCK_MONOTONIC` 也会是未定义 —— 因为 POSIX 的接口
#     要 `_POSIX_C_SOURCE` 才暴露,而 `-std=gnu11` 会自动带上 `_DEFAULT_SOURCE`
#     (等价于 `_POSIX_C_SOURCE 200809L`)。
#     若哪天非要锁 c11,就得同时加 `-D_POSIX_C_SOURCE=200809L`,两者择一。
#
#  📌 `third_party/lvgl/` 是 **LVGL 8.2.0 原样拷贝,一个字节没改**
#     (见 `third_party/LICENSE_LVGL.txt`)。所以本文件里没有针对它的补丁;
#     所有配置都走 `app/lv_conf.h`。
# ============================================================================

TARGET  := c_ui
BUILD   := build

# ---- 源文件 ----------------------------------------------------------------
# app/  = 我们的代码(驱动 + main)
# ui/   = 前序 ui_demo.c 全家桶,**原样搬,不改**
# ---- 前序的 ui_demo.c 一行不改,靠 -I 把它的 include 路径映射过去 ----
# ui_demo.c 里写的是 `#include "BSP/LCD/lcd_panel_config.h"`、
# `#include "BSP/LVGL/lv_port_indev.h"` 这类前序工程里的相对路径。
# 我们不改它,而是把 `port/` 加进搜索路径,让 `port/BSP/...` 顶上去。
# ⇒ 好处:ui_demo.c 仍可与前序工程**逐字对照**,报告里可辩护。
APP_SRC := $(wildcard app/*.c) $(wildcard ui/*.c)
LVGL_SRC := $(shell find third_party/lvgl/src -name '*.c' | sort)

SRC := $(APP_SRC) $(LVGL_SRC)
OBJ := $(patsubst %.c,$(BUILD)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

# ---- 头文件搜索路径 --------------------------------------------------------
#   -Iapp                   lv_conf.h(见下)、pynq_regs.h
#   -Iui                    display_protocol.h / ui_demo.h
#   -Iport                  ★ 关键:让 ui_demo.c 的 "BSP/..." 路径能解析
#   -Ithird_party/lvgl      让 `#include "lvgl.h"` 找到 lvgl/lvgl.h
INC := -Iapp -Iui -Iport -Ithird_party/lvgl

CFLAGS  := -std=gnu11 -O2 -g -DLV_CONF_INCLUDE_SIMPLE $(INC)
LDFLAGS := -lm

# 我们自己写的代码要求严;vendor 代码(ui/、third_party/)不拿 -Werror 卡它
CFLAGS_STRICT := -Wall -Wextra
# ui_demo.c 里有不少只用来对齐的未用参数 —— 前序就是那样,不为它改代码
CFLAGS_UI     := -Wall -Wextra -Wno-unused-parameter

# 目录提前建好(免得并行 make 时 mkdir 打架)
$(shell mkdir -p $(sort $(dir $(OBJ))))

# ============================================================================
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)
	@echo
	@echo "✅ 编好了:$@"
	@echo "   跑之前先确认:  ls -l /dev/mem   (必须存在,且要以 root 跑)"
	@echo "   单独试:        sudo ./$(TARGET) --fb0 0x<...> --fb1 0x<...> --help"

# ---- 编译规则 --------------------------------------------------------------
$(BUILD)/app/%.o: app/%.c
	$(CC) $(CFLAGS) $(CFLAGS_STRICT) -MMD -MP -c $< -o $@

$(BUILD)/ui/%.o: ui/%.c
	$(CC) $(CFLAGS) $(CFLAGS_UI) -MMD -MP -c $< -o $@

# LVGL:不加 -Wall。它是 vendor 代码,刷屏的警告会盖住我们自己的问题。
$(BUILD)/third_party/%.o: third_party/%.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

# ============================================================================
clean:
	rm -rf $(BUILD) $(TARGET)

# 上板第一件事:确认工具链齐、版本够新(LVGL 8.2 要 C99 的
# `__has_include`,gcc 5 以上都有,这里主要看有没有装 make/gcc)
check:
	@echo "--- 工具链 ---"
	@$(CC) --version | head -1 || echo "❌ 没有 gcc ⇒ apt install build-essential"
	@make --version | head -1 || echo "❌ 没有 make"
	@echo "--- /dev/mem(要读它才能映射寄存器与帧缓存)---"
	@ls -l /dev/mem 2>/dev/null || echo "❌ 找不到 /dev/mem"
	@echo "--- 帧缓存要靠 Python 传地址,先看内核有没有开 STRICT_DEVMEM ---"
	@grep -i "STRICT_DEVMEM" /boot/config-$$(uname -r) 2>/dev/null \
		|| echo "   (查不到 config,上板后按 README 的风险 R1 那节走)"
	@echo "--- 源码计数(自查用)---"
	@echo "   app/ + ui/ : $(words $(APP_SRC)) 个 .c"
	@echo "   lvgl       : $(words $(LVGL_SRC)) 个 .c"

# 编出来多大、用了哪些库(排障用)
size: $(TARGET)
	@ls -lh $(TARGET)
	@ldd $(TARGET) 2>/dev/null || echo "(静态链接)"

.PHONY: all clean check size
