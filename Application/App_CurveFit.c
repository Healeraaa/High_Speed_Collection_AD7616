#include "App_CurveFit.h"
#include "stdio.h"

/**
 * @brief 默认线性拟合曲线（备用方案）
 */
static float CurveFit_Default_Linear(float voltage)
{
    // 默认为1:1的线性转换
    return voltage;
}

/* 全局回调函数指针，初始化为默认线性转换 */
static CurveFit_Callback_t g_curvefit_callback = CurveFit_Default_Linear;

/**
 * @brief 注册拟合曲线回调函数
 */
void App_CurveFit_RegisterCallback(CurveFit_Callback_t callback)
{
    if (callback != NULL)
    {
        g_curvefit_callback = callback;
    }
}

/**
 * @brief 执行拟合曲线转换（回调函数始终存在，初始为默认线性转换）
 */
float App_CurveFit_Process(float voltage)
{
    return g_curvefit_callback(voltage);
}

/* ============= 13挡位拟合曲线实现（电流 = 电压 / (IV电阻 * Stage1 * Stage2)）============= */

float CurveFit_Range1_33_1_1(float voltage)//（100mA档位）
{
    return 28.041577 * voltage - -0.008448; // 33Ω * 1 * 1 的线性拟合（100mA档位）
}

float CurveFit_Range2_33_1_3_3(float voltage)//（30，3mA档位）
{
    return 8.486546 * voltage -0.000072;
}

float CurveFit_Range3_33_1_10(float voltage)//（10mA档位）
{
    float a = 1.0f / 330.0f;        // 33Ω * 1 * 10
    return a * voltage*1000.0f;
}

float CurveFit_Range4_1K_1_1(float voltage)//（3.3mA）
{
    // float a = 1.0f / 1000.0f;       // 1KΩ * 1 * 1
    // return a * voltage*1000.0f;
    return 1.007985 * voltage +0.000294;
}

float CurveFit_Range5_1K_1_3_3(float voltage)//（1mA档位）
{
    float a = 1.0f / 3300.0f;       // 1KΩ * 1 * 3.3
    return a * voltage*1000.0f;
}

float CurveFit_Range6_10K_1_1(float voltage)//（330uA档位）
{
    // float a = 1.0f / 10000.0f;      // 10KΩ * 1 * 1
    // return a * voltage*1000.0f;
    return 0.098804 * voltage -0.000100;
}

float CurveFit_Range7_10K_10_3_3(float voltage)//（100uA档位）
{
    float a = 1.0f / 33000.0f;     // 10KΩ * 1 * 3.3
    return a * voltage*1000.0f;
}

float CurveFit_Range8_100K_1_1(float voltage)//（33uA档位）
{
    // float a = 1.0f / 100000.0f;     // 100KΩ * 1 * 1
    // return a * voltage*1000.0f;
    return 0.009988 * voltage -0.000077;
}

float CurveFit_Range9_100K_1_3_3(float voltage)//（10uA档位）
{
    float a = 1.0f / 330000.0f;     // 100KΩ * 1 * 3.3
    return a * voltage*1000.0f;
}

float CurveFit_Range10_100K_1_10(float voltage)//（3.3uA档位）
{
    float a = 1.0f / 1000000.0f;    // 100KΩ * 1 * 10
    return a * voltage*1000.0f;
}

float CurveFit_Range11_100K_10_3_3(float voltage)//（1uA档位）
{
    float a = 1.0f / 3300000.0f;    // 100KΩ * 10 * 3.3
    return a * voltage*1000.0f;
}

float CurveFit_Range12_100K_10_10(float voltage)//（330nA档位）
{
    float a = 1.0f / 10000000.0f;   // 100KΩ * 10 * 10
    return a * voltage*1000.0f;
}

float CurveFit_Range13_100K_10_33(float voltage)//（100nA档位）
{
    float a = 1.0f / 33000000.0f;   // 100KΩ * 10 * 33
    return a * voltage*1000.0f;
}
