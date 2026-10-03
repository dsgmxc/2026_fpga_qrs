#ifndef DISPLAY_PROTOCOL_H
#define DISPLAY_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

void DisplayProtocol_Init(void);
void DisplayProtocol_Process(void);

bool DisplayProtocol_RequestReset(void);
bool DisplayProtocol_RequestPath(uint32_t path);
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
