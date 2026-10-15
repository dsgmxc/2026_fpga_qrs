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

uint32_t DisplayProtocol_GetReceivedLineCount(void);
uint32_t DisplayProtocol_GetCrcErrorCount(void);
uint32_t DisplayProtocol_GetTransmitDropCount(void);

#endif /* DISPLAY_PROTOCOL_H */
