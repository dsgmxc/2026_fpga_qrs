#ifndef ENGINEERING_PARAMETERS_H
#define ENGINEERING_PARAMETERS_H

#include <stdbool.h>
#include <stdint.h>

#include "sweep_interpolation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    float ciss_pf;
    float coss_pf;
    float crss_pf;
    float rg_ohm;
    float qoss_nc;
    float eoss_uj;
    uint8_t ciss_valid;
    uint8_t coss_valid;
    uint8_t crss_valid;
    uint8_t rg_valid;
    uint8_t qoss_valid;
    uint8_t eoss_valid;
} EngineeringParametersResult;

void EngineeringParameters_Compute(
    const SweepInterpolationCurve capacitance_curves[3],
    const uint8_t curve_ready[3],
    float analysis_voltage_v,
    float rg_ohm,
    bool rg_valid,
    EngineeringParametersResult *result);

#ifdef __cplusplus
}
#endif

#endif
