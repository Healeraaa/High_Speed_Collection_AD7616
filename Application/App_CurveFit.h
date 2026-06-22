#ifndef __APP_CURVEFIT_H__
#define __APP_CURVEFIT_H__

#include "stdint.h"

/**
 * @brief 拟合曲线回调函数指针类型
 * @param voltage 原始电压值（mV）
 * @return float 经过拟合曲线转换后的电流值
 */
typedef float (*CurveFit_Callback_t)(float voltage);

/**
 * @brief 注册拟合曲线回调函数
 * @param callback 回调函数指针
 */
void App_CurveFit_RegisterCallback(CurveFit_Callback_t callback);

/**
 * @brief 执行拟合曲线转换
 * @param voltage 原始电压值（mV）
 * @return float 转换后的电流值
 */
float App_CurveFit_Process(float voltage);

/* ============= 不同挡位的拟合曲线定义 ============= */

float CurveFit_Range1_33_1_1(float voltage);       // 33Ω + 1X + 1X     → 100mA
float CurveFit_Range2_33_1_3_3(float voltage);     // 33Ω + 1X + 3.3X   → 30.3mA
float CurveFit_Range3_33_1_10(float voltage);      // 33Ω + 1X + 10X    → 10mA
float CurveFit_Range4_1K_1_1(float voltage);       // 1KΩ + 1X + 1X     → 3.3mA
float CurveFit_Range5_1K_1_3_3(float voltage);     // 1KΩ + 1X + 3.3X   → 1mA
float CurveFit_Range6_10K_1_1(float voltage);      // 10KΩ + 1X + 1X    → 330uA
float CurveFit_Range7_10K_10_3_3(float voltage);   // 10KΩ + 10X + 3.3X → 100uA
float CurveFit_Range8_100K_1_1(float voltage);     // 100KΩ + 1X + 1X   → 33uA
float CurveFit_Range9_100K_1_3_3(float voltage);   // 100KΩ + 1X + 3.3X → 10uA
float CurveFit_Range10_100K_1_10(float voltage);   // 100KΩ + 1X + 10X  → 3.3uA
float CurveFit_Range11_100K_10_3_3(float voltage); // 100KΩ + 10X + 3.3X → 1uA
float CurveFit_Range12_100K_10_10(float voltage);  // 100KΩ + 10X + 10X → 330nA
float CurveFit_Range13_100K_10_33(float voltage);  // 100KΩ + 10X + 33X → 100nA

#endif
