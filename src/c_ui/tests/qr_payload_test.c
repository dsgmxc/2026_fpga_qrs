/*
 * 二维码载荷纯逻辑单测(宿主上跑,不依赖 LVGL / 板卡)。
 *
 * 编译:
 *   gcc -std=gnu11 -O2 -Wall -Wextra -Isrc/c_ui/app -Isrc/c_ui/ui \
 *       src/c_ui/tests/qr_payload_test.c \
 *       src/c_ui/app/cv_qr_payload.c \
 *       src/c_ui/app/ai_cv2spice_acv1.c \
 *       src/c_ui/app/ai_cv_sweep_payload.c \
 *       src/c_ui/ui/sweep_interpolation.c -lm -o /tmp/qr_payload_test
 *
 * 运行:
 *   /tmp/qr_payload_test /tmp/qr_url.txt
 *
 * 断言之外,把生成的 URL 写到文件里供 Node 脚本用前序网页解码器交叉验证。
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cv_qr_payload.h"

#define TEST_POINTS 60u
#define TEST_START_V 0.1f
#define TEST_STOP_V 100.0f

static int g_failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        (void)printf("FAIL: %s\n", message);
        ++g_failures;
    }
}

/* 与 `pynq_protocol.c::sweep_bias_volts_at` 相同的对数排布。 */
static float test_voltage(uint32_t index)
{
    const float ratio = (float)index / (float)(TEST_POINTS - 1u);
    float volts = TEST_START_V *
                  powf(TEST_STOP_V / TEST_START_V, ratio);

    if (volts < TEST_START_V)
    {
        volts = TEST_START_V;
    }
    if (volts > TEST_STOP_V)
    {
        volts = TEST_STOP_V;
    }
    return volts;
}

/* 与 `pynq_protocol.c::demo_capacitance` 相同。 */
static float test_capacitance(uint32_t path, float voltage)
{
    static const float c_max[3] = {980.0f, 240.0f, 48.0f};
    static const float c_min[3] = {420.0f, 62.0f, 7.5f};
    const float t = 1.0f / (1.0f + powf(voltage / 2.2f, 0.85f));

    if (path >= 3u)
    {
        path = 0u;
    }
    return c_min[path] + ((c_max[path] - c_min[path]) * t);
}

static void fill_case(float voltage[3][TEST_POINTS],
                      float capacitance[3][TEST_POINTS],
                      uint8_t valid[3][TEST_POINTS])
{
    uint32_t path;
    uint32_t point;

    for (path = 0u; path < 3u; ++path)
    {
        for (point = 0u; point < TEST_POINTS; ++point)
        {
            voltage[path][point] = test_voltage(point);
            capacitance[path][point] =
                test_capacitance(path, voltage[path][point]);
            valid[path][point] = 1u;
        }
    }
}

static void run_case(const char *label,
                     float voltage[3][TEST_POINTS],
                     float capacitance[3][TEST_POINTS],
                     uint8_t valid[3][TEST_POINTS],
                     const char *url_path)
{
    const float *voltage_v[3];
    const float *capacitance_pf[3];
    const uint8_t *valid_p[3];
    const uint32_t point_count[3] = {TEST_POINTS, TEST_POINTS, TEST_POINTS};
    CV_QR_PayloadInfo info;
    char url[CV_QR_PAYLOAD_URL_CAPACITY];
    uint32_t path;
    FILE *file;

    for (path = 0u; path < 3u; ++path)
    {
        voltage_v[path] = voltage[path];
        capacitance_pf[path] = capacitance[path];
        valid_p[path] = valid[path];
    }

    (void)printf("--- %s ---\n", label);
    if (!CV_QR_Payload_Build(voltage_v, capacitance_pf, valid_p, point_count,
                             1000000u, 1.0f, 3.6f,
                             url, sizeof(url), &info))
    {
        expect_true(0, "CV_QR_Payload_Build returned false");
        return;
    }
    (void)printf("url length=%u acv1_nodes=%u scv1_points=%u "
                 "scv1=%d start=%.6f stop=%.6f crc=0x%08lx/0x%08lx\n",
                 (unsigned)info.url_length,
                 (unsigned)info.acv1_nodes,
                 (unsigned)info.scv1_points,
                 (int)info.scv1_present,
                 (double)info.start_voltage_v,
                 (double)info.stop_voltage_v,
                 (unsigned long)info.acv1_crc32,
                 (unsigned long)info.scv1_crc32);
    (void)printf("%s\n", url);

    expect_true(strncmp(url,
                        "https://fox4327.github.io/ai-cv2spice-web/#m=",
                        strlen("https://fox4327.github.io/ai-cv2spice-web/#m="))
                == 0,
                "URL must start with the predecessor base URL and #m=");
    expect_true(info.acv1_nodes == 12u, "100 V sweep must produce 12 nodes");
    expect_true(info.scv1_points == 141u, "100 V sweep grid must be 141 points");
    expect_true(info.scv1_present, "SCV1 must be present");
    expect_true(info.url_length == strlen(url), "URL length mismatch");
    expect_true(strstr(url, "&c=") != NULL, "URL must contain &c=");

    if (url_path != NULL)
    {
        file = fopen(url_path, "w");
        if (file != NULL)
        {
            (void)fprintf(file, "%s\n", url);
            (void)fclose(file);
        }
    }
}

int main(int argc, char **argv)
{
    static float voltage[3][TEST_POINTS];
    static float capacitance[3][TEST_POINTS];
    static uint8_t valid[3][TEST_POINTS];
    const char *url_path = (argc > 1) ? argv[1] : "/tmp/qr_url.txt";
    uint32_t path;

    fill_case(voltage, capacitance, valid);
    run_case("all valid", voltage, capacitance, valid, url_path);

    /* 单个无效点:插值会跨过去,SCV1 仍是完整曲线;ACV1 照常生成。 */
    fill_case(voltage, capacitance, valid);
    for (path = 0u; path < 3u; ++path)
    {
        valid[path][30] = 0u;
        capacitance[path][30] = 0.0f;
    }
    run_case("one invalid point per path", voltage, capacitance, valid, NULL);

    /* 低电压段全部无效 → 公共区间从首个有效点开始,节点仍可生成。 */
    fill_case(voltage, capacitance, valid);
    for (path = 0u; path < 3u; ++path)
    {
        uint32_t point;

        for (point = 0u; point < TEST_POINTS; ++point)
        {
            if (voltage[path][point] < 1.0f)
            {
                valid[path][point] = 0u;
                capacitance[path][point] = 0.0f;
            }
        }
    }
    run_case("low-voltage points removed", voltage, capacitance, valid, NULL);

    (void)printf("%s (%d failure(s))\n",
                 (g_failures == 0) ? "PASS" : "FAIL", g_failures);
    return (g_failures == 0) ? 0 : 1;
}
