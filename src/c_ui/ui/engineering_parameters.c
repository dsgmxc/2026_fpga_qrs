#include "engineering_parameters.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define ENGINEERING_INTEGRATION_INTERVALS 256u

static bool EngineeringParameters_IntegrateCoss(
    const SweepInterpolationCurve *curve,
    float target_voltage_v,
    float *qoss_nc,
    float *eoss_uj)
{
    float minimum_log_voltage;
    float maximum_log_voltage;
    float minimum_voltage;
    float maximum_voltage;
    float previous_voltage;
    float previous_capacitance;
    float charge_pf_v;
    float energy_pf_v2;
    uint32_t index;

    if ((curve == NULL) || (qoss_nc == NULL) || (eoss_uj == NULL) ||
        !isfinite(target_voltage_v) || (target_voltage_v <= 0.0f) ||
        !SweepInterpolation_GetLogVoltageRange(
            curve, &minimum_log_voltage, &maximum_log_voltage))
    {
        return false;
    }

    minimum_voltage = powf(10.0f, minimum_log_voltage);
    maximum_voltage = powf(10.0f, maximum_log_voltage);
    if (!isfinite(minimum_voltage) || !isfinite(maximum_voltage) ||
        (target_voltage_v < minimum_voltage) ||
        (target_voltage_v > maximum_voltage) ||
        !SweepInterpolation_Evaluate(curve, minimum_voltage,
                                     &previous_capacitance))
    {
        return false;
    }

    /* 0 V到首个实测电压之间使用首点Coss保持不变的工程近似。 */
    charge_pf_v = previous_capacitance * minimum_voltage;
    energy_pf_v2 = 0.5f * previous_capacitance *
                   minimum_voltage * minimum_voltage;
    previous_voltage = minimum_voltage;

    if (target_voltage_v > minimum_voltage)
    {
        float target_log_voltage = log10f(target_voltage_v);

        for (index = 1u;
             index <= ENGINEERING_INTEGRATION_INTERVALS;
             ++index)
        {
            float ratio = (float)index /
                          (float)ENGINEERING_INTEGRATION_INTERVALS;
            float voltage = powf(
                10.0f,
                minimum_log_voltage + ratio *
                (target_log_voltage - minimum_log_voltage));
            float capacitance;
            float delta_voltage;

            if (!SweepInterpolation_Evaluate(curve, voltage,
                                             &capacitance))
            {
                return false;
            }
            delta_voltage = voltage - previous_voltage;
            charge_pf_v += 0.5f *
                           (previous_capacitance + capacitance) *
                           delta_voltage;
            energy_pf_v2 += 0.5f *
                            (previous_voltage * previous_capacitance +
                             voltage * capacitance) * delta_voltage;
            previous_voltage = voltage;
            previous_capacitance = capacitance;
        }
    }

    /* pF*V转换为nC；pF*V^2转换为uJ。 */
    *qoss_nc = charge_pf_v * 1.0e-3f;
    *eoss_uj = energy_pf_v2 * 1.0e-6f;
    return isfinite(*qoss_nc) && isfinite(*eoss_uj) &&
           (*qoss_nc >= 0.0f) && (*eoss_uj >= 0.0f);
}

void EngineeringParameters_Compute(
    const SweepInterpolationCurve capacitance_curves[3],
    const uint8_t curve_ready[3],
    float analysis_voltage_v,
    float rg_ohm,
    bool rg_valid,
    EngineeringParametersResult *result)
{
    if (result == NULL)
    {
        return;
    }

    (void)memset(result, 0, sizeof(*result));
    if ((capacitance_curves == NULL) || (curve_ready == NULL) ||
        !isfinite(analysis_voltage_v) || (analysis_voltage_v <= 0.0f))
    {
        return;
    }

    if ((curve_ready[0] != 0u) &&
        SweepInterpolation_Evaluate(&capacitance_curves[0],
                                    analysis_voltage_v,
                                    &result->ciss_pf))
    {
        result->ciss_valid = 1u;
    }
    if ((curve_ready[1] != 0u) &&
        SweepInterpolation_Evaluate(&capacitance_curves[1],
                                    analysis_voltage_v,
                                    &result->coss_pf))
    {
        result->coss_valid = 1u;
        if (EngineeringParameters_IntegrateCoss(
                &capacitance_curves[1], analysis_voltage_v,
                &result->qoss_nc, &result->eoss_uj))
        {
            result->qoss_valid = 1u;
            result->eoss_valid = 1u;
        }
    }
    if ((curve_ready[2] != 0u) &&
        SweepInterpolation_Evaluate(&capacitance_curves[2],
                                    analysis_voltage_v,
                                    &result->crss_pf))
    {
        result->crss_valid = 1u;
    }
    if (rg_valid && isfinite(rg_ohm) && (rg_ohm >= 0.0f))
    {
        result->rg_ohm = rg_ohm;
        result->rg_valid = 1u;
    }
}
