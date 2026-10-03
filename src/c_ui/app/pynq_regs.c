/**
 * @file pynq_regs.c
 * @brief /dev/mem 映射 + 寄存器读写(见 pynq_regs.h 里的三条 ⚠️)
 */

#include "pynq_regs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define PAGE_SIZE 4096u

static int      s_mem_fd   = -1;
static void    *s_reg_base = NULL;
static uint32_t s_reg_len  = 0;

/* ------------------------------------------------------------------ */
int cv_regs_init(uint32_t axi_base)
{
    if (s_reg_base != NULL) {
        return 0;                            /* 幂等:重复调用不再开一次 */
    }

    s_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (s_mem_fd < 0) {
        fprintf(stderr,
                "[regs] 打不开 /dev/mem:%s\n"
                "       ⇒ 本进程必须以 root 跑(main.py 已经是)。\n",
                strerror(errno));
        return -1;
    }

    s_reg_len  = PAGE_SIZE;                  /* AXI 窗口 4 KB */
    s_reg_base = mmap(NULL, s_reg_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                      s_mem_fd, (off_t)axi_base);
    if (s_reg_base == MAP_FAILED) {
        fprintf(stderr, "[regs] mmap 0x%08X 失败:%s\n",
                axi_base, strerror(errno));
        close(s_mem_fd);
        s_mem_fd   = -1;
        s_reg_base = NULL;
        return -2;
    }

    printf("[regs] /dev/mem 已映射 AXI 0x%08X(%u B)\n", axi_base, s_reg_len);
    return 0;
}

/* ------------------------------------------------------------------ */
void cv_regs_close(void)
{
    if (s_reg_base != NULL) {
        munmap(s_reg_base, s_reg_len);
        s_reg_base = NULL;
    }
    if (s_mem_fd >= 0) {
        close(s_mem_fd);
        s_mem_fd = -1;
    }
}

/* ------------------------------------------------------------------ */
uint32_t cv_reg_read(uint32_t off)
{
    if (s_reg_base == NULL) {
        fprintf(stderr, "[regs] 读 0x%02X 时还没 init —— 返回 0 是**假值**,别拿它下结论\n", off);
        return 0;
    }
    /* volatile + 内存屏障:AXI 从机是设备,不能被编译器重排或合并 */
    return *(volatile uint32_t *)((volatile char *)s_reg_base + off);
}

void cv_reg_write(uint32_t off, uint32_t val)
{
    if (s_reg_base == NULL) {
        fprintf(stderr, "[regs] 写 0x%02X 时还没 init —— **写丢了**\n", off);
        return;
    }
    *(volatile uint32_t *)((volatile char *)s_reg_base + off) = val;
    __sync_synchronize();                    /* 写要在下一次读之前落到总线上 */
}

/* ------------------------------------------------------------------ */
void *cv_phys_map(uint32_t phys, uint32_t len)
{
    if (s_mem_fd < 0) {
        /* ⚠️ 这里**不**替调用方猜 AXI 基址 —— 猜错了会映射到一段无关内存,
         *    读出来是垃圾而不报错。基址只有一个来源:命令行参数。 */
        fprintf(stderr, "[regs] cv_phys_map 之前必须先 cv_regs_init(axi_base)\n");
        errno = EINVAL;
        return NULL;
    }
    if (phys % PAGE_SIZE) {
        fprintf(stderr, "[regs] cv_phys_map:地址 0x%08X 未页对齐 —— mmap 会失败\n", phys);
        errno = EINVAL;
        return NULL;
    }

    void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED,
                   s_mem_fd, (off_t)phys);
    if (p == MAP_FAILED) {
        fprintf(stderr,
                "[regs] mmap 物理内存 0x%08X (%u B) 失败:%s\n"
                "       ⇒ 若 errno 是 EPERM/EACCES,基本可以确定是风险 R1:\n"
                "         内核开了 CONFIG_STRICT_DEVMEM,/dev/mem 不允许映射普通 RAM。\n"
                "         (可以试 `grep STRICT_DEVMEM /boot/config-$(uname -r)` 确认)\n"
                "       ⇒ 备选方案见 docs/display_subsystem.md 的风险表。\n",
                phys, len, strerror(errno));
        return NULL;
    }
    return p;
}

void cv_phys_unmap(void *p, uint32_t len)
{
    if (p != NULL) {
        munmap(p, len);
    }
}

/* ------------------------------------------------------------------ */
uint32_t cv_millis(void)
{
    struct timespec ts;
    /* ⚠️ 必须是 MONOTONIC —— REALTIME 会被 NTP/时区调整拽着跳,
     *    而 LVGL 的 tick 只能单调递增。 */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

void cv_delay_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec  = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------ */
int cv_lcd_swap_wait(int target, int timeout_ms)
{
    if (target != 0 && target != 1) {
        return -2;
    }

    uint32_t t0 = cv_millis();

    /* 已经在目标缓冲上就不用请求了 —— 但**也要确认 pending 是 0**,
     * 否则"上一次的请求还没生效",这一刻的前台其实还是旧的。 */
    uint32_t st = cv_reg_read(CV_REG_LCD_SWAP);
    if (CV_SWAP_FRONT(st) != (uint32_t)target || CV_SWAP_PENDING(st)) {
        cv_reg_write(CV_REG_LCD_SWAP, (uint32_t)target);
    }

    do {
        st = cv_reg_read(CV_REG_LCD_SWAP);
        if (CV_SWAP_PENDING(st) == 0u && CV_SWAP_FRONT(st) == (uint32_t)target) {
            return 0;
        }
        cv_delay_ms(1);                      /* 一帧 17 ms,1 ms 粒度够细 */
    } while ((int)(cv_millis() - t0) < timeout_ms);

    fprintf(stderr,
            "[lcd] 换址超时(%d ms):请求 %d,读回 front=%u pending=%u\n"
            "      ⇒ 检查:① LCD_CTRL 是否 = %u(SRC_DDR);"
            "② 位流是不是含显示通路那一版。\n"
            "      ⇒ 换址前的 0x64 = 0x%08X(underrun=%u axi_err=%u)\n",
            timeout_ms, target,
            CV_SWAP_FRONT(st), CV_SWAP_PENDING(st), CV_LCD_SRC_DDR,
            cv_reg_read(CV_REG_FB_STATUS),
            CV_FB_UNDERRUN(cv_reg_read(CV_REG_FB_STATUS)),
            CV_FB_AXIERR(cv_reg_read(CV_REG_FB_STATUS)));
    return -1;
}
