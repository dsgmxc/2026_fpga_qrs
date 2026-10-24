#ifndef DISPLAY_PROTOCOL_H
#define DISPLAY_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

void DisplayProtocol_Init(void);
void DisplayProtocol_Process(void);

bool DisplayProtocol_RequestReset(void);
bool DisplayProtocol_RequestPath(uint32_t path);
/* 🆕 2026-10-15:参考电阻挡位按钮。@p index 0=A(10Ω) 1=B(100Ω) 2=C(1kΩ)
   3=D(10kΩ);越界直接拒、不动硬件。
   与 RequestPath 不同,这里**当场写 0x18**(挡位不占继电器稳定窗),
   返回前已回读确认完毕。 */
bool DisplayProtocol_RequestGear(uint32_t index);
bool DisplayProtocol_RequestBias(uint32_t bias_tenths);
bool DisplayProtocol_RequestPoint(uint32_t path, uint32_t bias_tenths);
bool DisplayProtocol_RequestPointAll(uint32_t bias_tenths);
bool DisplayProtocol_RequestSweep(uint32_t path, uint32_t stop_tenths);
bool DisplayProtocol_RequestRgThenSweepAll(uint32_t stop_tenths);
bool DisplayProtocol_RequestSweepStop(void);
bool DisplayProtocol_RequestSweepStatus(void);

/* 🆕 2026-10-24:三标准校准的一次采集(4 腿 × 4 挡 = 16 点)。
   @p standard 见 calibration.h 的 CalStandard:0 = OPEN(拔掉被测件)、
   1 = SHORT(D-G-S 短接 0 Ω)、2 = LOAD(标准板)。
   挡位切换当拍生效,继电器只在换腿时切(每腿等一个驻留),
   所以整轮约 4×驻留 + 16 次采集,期间界面照常响应。
   拒绝条件(返回 false 并打印原因):已有采集在跑 / 扫压在跑 /
   校准偏压超过标准板耐压 / LOAD 但标准板参数全为 0 /
   激励频率不是 10k/100k/1M。 */
bool DisplayProtocol_RequestCalibration(uint32_t standard);

/** @brief 1 = 正在跑校准采集(界面据此禁用会打架的按钮)。 */
bool DisplayProtocol_CalibrationBusy(void);

uint32_t DisplayProtocol_GetReceivedLineCount(void);
uint32_t DisplayProtocol_GetCrcErrorCount(void);
uint32_t DisplayProtocol_GetTransmitDropCount(void);

#endif /* DISPLAY_PROTOCOL_H */
