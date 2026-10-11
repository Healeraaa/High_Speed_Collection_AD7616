#include "Module_IncentiveConfig.h"

#include "App_CurveFit.h"
#include "Module_Serial411.h"

#include <stddef.h>

#define INCENTIVE_INTEGER_COUNT   4U
#define INCENTIVE_PARAMETER_COUNT 6U
#define INCENTIVE_NOTIFY_CA_FINISHED (1UL << 0)

/* 默认保持旧行为；在应用初始化时可以明确切换为CA组合实现。 */
static GPCI_Implementation_TypeDef s_gpci_implementation =
    GPCI_IMPLEMENTATION_BY_CA;

/*
 * GPCI通过CA实现时，需要跨越多次“发送CA→等待下降沿”的过程，
 * 因此必须保存本轮执行状态，不能只使用函数内的临时变量。
 */
typedef struct
{
    volatile uint8_t is_running;   /* 1：正在执行GPCI-CA循环 */
    uint32_t total_cycles;         /* 本轮一共需要执行多少次CA */
    uint32_t completed_cycles;     /* 已经收到多少次CA下降沿 */
    Serial411_DoubleConverter_t ca_packet[SERIAL411_DATA_LENGTH];
} Incentive_GPCIContext_t;

static Incentive_GPCIContext_t s_gpci_context;

/* 接收CA完成通知的任务句柄，在Init中记录当前配置任务。 */
static TaskHandle_t s_incentive_task_handle = NULL;

/*
 * 一个表项对应一种已经标定过的电流量程。
 * 只有表中存在的增益组合，才允许继续发送配置。
 */
typedef struct
{
    IV_Gain_TypeDef iv_gain;
    Voltage_Gain_Stage1_TypeDef stage1;
    Voltage_Gain_Stage2_TypeDef stage2;
    CurveFit_Callback_t callback;
} Incentive_CurveFitMap_t;

static const Incentive_CurveFitMap_t s_curvefit_map[] =
{
    {IV_GAIN_33,   VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_1X,   CurveFit_Range1_33_1_1},
    {IV_GAIN_33,   VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_3_3X, CurveFit_Range2_33_1_3_3},
    {IV_GAIN_33,   VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_10X,  CurveFit_Range3_33_1_10},
    {IV_GAIN_1K,   VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_1X,   CurveFit_Range4_1K_1_1},
    {IV_GAIN_1K,   VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_3_3X, CurveFit_Range5_1K_1_3_3},
    {IV_GAIN_10K,  VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_1X,   CurveFit_Range6_10K_1_1},
    {IV_GAIN_10K,  VOLTAGE_GAIN_STAGE1_10X, VOLTAGE_GAIN_STAGE2_3_3X, CurveFit_Range7_10K_10_3_3},
    {IV_GAIN_100K, VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_1X,   CurveFit_Range8_100K_1_1},
    {IV_GAIN_100K, VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_3_3X, CurveFit_Range9_100K_1_3_3},
    {IV_GAIN_100K, VOLTAGE_GAIN_STAGE1_1X,  VOLTAGE_GAIN_STAGE2_10X,  CurveFit_Range10_100K_1_10},
    {IV_GAIN_100K, VOLTAGE_GAIN_STAGE1_10X, VOLTAGE_GAIN_STAGE2_3_3X, CurveFit_Range11_100K_10_3_3},
    {IV_GAIN_100K, VOLTAGE_GAIN_STAGE1_10X, VOLTAGE_GAIN_STAGE2_10X,  CurveFit_Range12_100K_10_10},
    {IV_GAIN_100K, VOLTAGE_GAIN_STAGE1_10X, VOLTAGE_GAIN_STAGE2_33X,  CurveFit_Range13_100K_10_33}
};

/**
 * @brief 检查上位机发送的数据结构是否完整、枚举值是否合法。
 */
static Module_Status_t Module_IncentiveConfig_Validate(
    const Upper_RxData_t *rx_data)
{
    if (rx_data == NULL)
    {
        return Module_ERROR;
    }

    /* 必须是 Module_UpperRx_ParseData() 成功解析出来的数据。 */
    if (rx_data->is_valid == 0U)
    {
        return Module_ERROR;
    }

    /* 当前协议固定为4个公共整数参数和6个模式参数。 */
    if ((rx_data->int_count != INCENTIVE_INTEGER_COUNT) ||
        (rx_data->float_count != INCENTIVE_PARAMETER_COUNT))
    {
        return Module_ERROR;
    }

    /* 当前只支持 CV、DPV、CA 和 GPCI 四种模式。 */
    if (rx_data->mode > (uint32_t)INCENTIVE_MODE_GPCI)
    {
        return Module_ERROR;
    }

    /*
     * int_data[0]：WE通道，合法枚举值为0~3。
     * 注意：这是硬件枚举编号，不是直接使用通道数字1~4。
     */
    if (rx_data->int_data[0] > (uint32_t)WE_CHANNEL_2)
    {
        return Module_ERROR;
    }

    /* int_data[1]：IV增益，合法枚举值为0~3。 */
    if (rx_data->int_data[1] > (uint32_t)IV_GAIN_100K)
    {
        return Module_ERROR;
    }

    /* int_data[2]：第一级电压增益，合法枚举值为0~1。 */
    if (rx_data->int_data[2] > (uint32_t)VOLTAGE_GAIN_STAGE1_10X)
    {
        return Module_ERROR;
    }

    /* int_data[3]：第二级电压增益，合法枚举值为0~3。 */
    if (rx_data->int_data[3] > (uint32_t)VOLTAGE_GAIN_STAGE2_33X)
    {
        return Module_ERROR;
    }

    return Module_OK;
}

/**
 * @brief 根据三级增益配置查找对应的电流拟合曲线。
 *
 * @return 找到时返回回调函数；没有标定曲线时返回NULL。
 */
static CurveFit_Callback_t Module_IncentiveConfig_FindCurveFit(
    IV_Gain_TypeDef iv_gain,
    Voltage_Gain_Stage1_TypeDef stage1,
    Voltage_Gain_Stage2_TypeDef stage2)
{
    uint32_t i;
    uint32_t map_count;

    map_count = (uint32_t)(sizeof(s_curvefit_map) / sizeof(s_curvefit_map[0]));

    for (i = 0U; i < map_count; i++)
    {
        if ((s_curvefit_map[i].iv_gain == iv_gain) &&
            (s_curvefit_map[i].stage1 == stage1) &&
            (s_curvefit_map[i].stage2 == stage2))
        {
            return s_curvefit_map[i].callback;
        }
    }

    return NULL;
}

/**
 * @brief 按模式把六个参数放入发送缓冲区的0~5号位置。
 *
 * 四种模式的传输格式相同，区别是每个位置代表的含义不同。
 * 在这里分开书写，是为了阅读和以后增加各模式参数检查时更清楚。
 */
static void Module_IncentiveConfig_FillModeParameters(
    Incentive_Mode_TypeDef mode,
    const float parameters[INCENTIVE_PARAMETER_COUNT],
    Serial411_DoubleConverter_t packet[SERIAL411_DATA_LENGTH])
{
    switch (mode)
    {
        case INCENTIVE_MODE_CV:
            packet[0].double_val = (double)parameters[0]; /* 初始电位 */
            packet[1].double_val = (double)parameters[1]; /* 终止电位 */
            packet[2].double_val = (double)parameters[2]; /* 扫描极限1 */
            packet[3].double_val = (double)parameters[3]; /* 扫描极限2 */
            packet[4].double_val = (double)parameters[4]; /* 扫描速率 */
            packet[5].double_val = (double)parameters[5]; /* 循环次数 */
            break;

        case INCENTIVE_MODE_DPV:
            packet[0].double_val = (double)parameters[0]; /* 初始电位 */
            packet[1].double_val = (double)parameters[1]; /* 终止电位 */
            packet[2].double_val = (double)parameters[2]; /* 步进电位 */
            packet[3].double_val = (double)parameters[3]; /* 脉冲幅度 */
            packet[4].double_val = (double)parameters[4]; /* 脉冲宽度 */
            packet[5].double_val = (double)parameters[5]; /* 脉冲周期 */
            break;

        case INCENTIVE_MODE_CA:
            packet[0].double_val = (double)parameters[0]; /* 初始电位 */
            packet[1].double_val = (double)parameters[1]; /* 初始时间 */
            packet[2].double_val = (double)parameters[2]; /* 阶跃1电位 */
            packet[3].double_val = (double)parameters[3]; /* 阶跃1时间 */
            packet[4].double_val = (double)parameters[4]; /* 阶跃2电位 */
            packet[5].double_val = (double)parameters[5]; /* 阶跃2时间 */
            break;

        case INCENTIVE_MODE_GPCI:
            packet[0].double_val = (double)parameters[0]; /* 预激发电位 */
            packet[1].double_val = (double)parameters[1]; /* 反应电位 */
            packet[2].double_val = (double)parameters[2]; /* 恢复电位 */
            packet[3].double_val = (double)parameters[3]; /* 静置时间 */
            packet[4].double_val = (double)parameters[4]; /* 脉冲持续时间 */
            packet[5].double_val = (double)parameters[5]; /* 循环次数 */
            break;

        default:
            /* 调用本函数前已经检查模式，正常情况下不会进入这里。 */
            break;
    }
}

/**
 * @brief 填写所有模式共用的通道、增益和保留字段。
 */
static void Module_IncentiveConfig_FillCommonParameters(
    const Upper_RxData_t *rx_data,
    Serial411_DoubleConverter_t packet[SERIAL411_DATA_LENGTH])
{
    /*
     * packet[6]不是普通double，而是把一个double的8字节分别用作配置位。
     * 先将8字节全部清零，再填写当前协议使用的前4字节。
     */
    packet[6].u8_array[0] = (uint8_t)rx_data->int_data[0]; /* WE通道 */
    packet[6].u8_array[1] = (uint8_t)rx_data->int_data[1]; /* IV增益 */
    packet[6].u8_array[2] = (uint8_t)rx_data->int_data[2]; /* 电压增益1 */
    packet[6].u8_array[3] = (uint8_t)rx_data->int_data[3]; /* 电压增益2 */
    packet[6].u8_array[4] = (uint8_t)FEEDBACK_GND;
    packet[6].u8_array[5] = (uint8_t)FEEDBACK_GND;
    packet[6].u8_array[6] = (uint8_t)FEEDBACK_GND;
    packet[6].u8_array[7] = (uint8_t)FEEDBACK_GND;

    /* 当前固定为同时控制全部DAC通道；8、9号位置暂时保留。 */
    packet[7].double_val = (double)DAC_CH_ALL;
    packet[8].double_val = 0.0;
    packet[9].double_val = 0.0;
}

/**
 * @brief 复制一个完整的411发送数据区。
 */
static void Module_IncentiveConfig_CopyPacket(
    Serial411_DoubleConverter_t destination[SERIAL411_DATA_LENGTH],
    const Serial411_DoubleConverter_t source[SERIAL411_DATA_LENGTH])
{
    uint32_t i;

    for (i = 0U; i < SERIAL411_DATA_LENGTH; i++)
    {
        destination[i] = source[i];
    }
}

/**
 * @brief 清除GPCI-CA循环状态。
 */
static void Module_IncentiveConfig_ClearGPCIContext(void)
{
    s_gpci_context.is_running = 0U;
    s_gpci_context.total_cycles = 0U;
    s_gpci_context.completed_cycles = 0U;
}

/**
 * @brief 使用底层原生模式发送配置。
 */
static Module_Status_t Module_IncentiveConfig_SendNative(
    Incentive_Mode_TypeDef mode,
    CurveFit_Callback_t callback,
    Serial411_DoubleConverter_t packet[SERIAL411_DATA_LENGTH])
{
    App_CurveFit_RegisterCallback(callback);
    return Serial_SendPacket((uint8_t)mode, (double *)packet);
}

/**
 * @brief 将GPCI转换为CA，并只启动本轮的第一次CA事件。
 *
 * 后续CA不能在这里连续发送，必须等待PE1下降沿确认前一次已经结束。
 */
static Module_Status_t Module_IncentiveConfig_StartGPCIByCA(
    const Upper_RxData_t *rx_data,
    CurveFit_Callback_t callback,
    Serial411_DoubleConverter_t packet[SERIAL411_DATA_LENGTH])
{
    double ca_parameters[INCENTIVE_PARAMETER_COUNT];
    uint32_t cycle_count;
    uint32_t i;
    Module_Status_t status;

    /* 上一轮还没有结束时，不允许覆盖保存的数据包和循环计数。 */
    if (s_gpci_context.is_running != 0U)
    {
        return Module_BUSY;
    }

    status = Module_IncentiveConfig_ConvertGPCIToCA(
        rx_data->float_data,
        ca_parameters,
        &cycle_count);
    if (status != Module_OK)
    {
        return status;
    }

    /* 把转换后的CA六参数放入0~5号位置。 */
    for (i = 0U; i < INCENTIVE_PARAMETER_COUNT; i++)
    {
        packet[i].double_val = ca_parameters[i];
    }

    /* 保存完整CA包，下降沿到来后会重复发送同一份数据。 */
    Module_IncentiveConfig_CopyPacket(s_gpci_context.ca_packet, packet);
    s_gpci_context.total_cycles = cycle_count;
    s_gpci_context.completed_cycles = 0U;
    s_gpci_context.is_running = 1U;

    App_CurveFit_RegisterCallback(callback);

    /* 对底层来说，这始终是一条普通CA命令。 */
    status = Serial_SendPacket(
        (uint8_t)INCENTIVE_MODE_CA,
        (double *)s_gpci_context.ca_packet);

    if (status != Module_OK)
    {
        /* 第一次发送失败，本轮循环没有真正启动。 */
        Module_IncentiveConfig_ClearGPCIContext();
    }

    return status;
}

void Module_IncentiveConfig_Init(void)
{
    uint32_t i;

    Module_IncentiveConfig_ClearGPCIContext();

    for (i = 0U; i < SERIAL411_DATA_LENGTH; i++)
    {
        s_gpci_context.ca_packet[i].double_val = 0.0;
    }

    /* Init由App_IncentiveSettingsTask调用，因此这里记录的就是配置任务。 */
    s_incentive_task_handle = xTaskGetCurrentTaskHandle();

    /* 与原任务代码保持一致的上电默认挡位。 */
    App_CurveFit_RegisterCallback(CurveFit_Range1_33_1_1);
}

void Module_IncentiveConfig_SetGPCIImplementation(
    GPCI_Implementation_TypeDef implementation)
{
    /* 非法枚举不改变当前选择，避免意外切换执行方式。 */
    if ((implementation == GPCI_IMPLEMENTATION_NATIVE) ||
        (implementation == GPCI_IMPLEMENTATION_BY_CA))
    {
        /* 执行过程中不允许切换实现方式。 */
        if (s_gpci_context.is_running == 0U)
        {
            s_gpci_implementation = implementation;
        }
    }
}

Module_Status_t Module_IncentiveConfig_ConvertGPCIToCA(
    const float gpci_parameters[6],
    double ca_parameters[6],
    uint32_t *cycle_count)
{
    double cycle_value;
    uint32_t converted_cycle_count;

    if ((gpci_parameters == NULL) ||
        (ca_parameters == NULL) ||
        (cycle_count == NULL))
    {
        return Module_ERROR;
    }

    /*
     * GPCI的第6个参数由文本浮点数解析而来，但语义上必须是整数。
     * 先用double检查范围，可以避免对负数、NaN或超范围数直接强转。
     */
    cycle_value = (double)gpci_parameters[5];
    if (!((cycle_value >= 1.0) && (cycle_value <= 4294967295.0)))
    {
        return Module_ERROR;
    }

    converted_cycle_count = (uint32_t)cycle_value;
    if ((double)converted_cycle_count != cycle_value)
    {
        return Module_ERROR;
    }

    /* GPCI预激发阶段对应CA初始阶段。 */
    ca_parameters[0] = (double)gpci_parameters[0];
    ca_parameters[1] = (double)gpci_parameters[3] / 2.0;

    /* GPCI反应阶段对应CA阶跃1阶段。 */
    ca_parameters[2] = (double)gpci_parameters[1];
    ca_parameters[3] = (double)gpci_parameters[4];

    /* GPCI恢复阶段对应CA阶跃2阶段。 */
    ca_parameters[4] = (double)gpci_parameters[2];
    ca_parameters[5] = (double)gpci_parameters[3] / 2.0;

    *cycle_count = converted_cycle_count;
    return Module_OK;
}

Module_Status_t Module_IncentiveConfig_Handle(const Upper_RxData_t *rx_data)
{
    Serial411_DoubleConverter_t packet[SERIAL411_DATA_LENGTH];
    Incentive_Mode_TypeDef mode;
    IV_Gain_TypeDef iv_gain;
    Voltage_Gain_Stage1_TypeDef stage1;
    Voltage_Gain_Stage2_TypeDef stage2;
    CurveFit_Callback_t callback;

    /* 第一步：整组数据全部合法后，才开始生成新的发送包。 */
    if (Module_IncentiveConfig_Validate(rx_data) != Module_OK)
    {
        return Module_ERROR;
    }

    /*
     * GPCI-CA循环未完成时，拒绝新的配置，防止中途切换曲线、模式
     * 或覆盖下一轮CA要使用的数据包。
     */
    if (s_gpci_context.is_running != 0U)
    {
        return Module_BUSY;
    }

    mode = (Incentive_Mode_TypeDef)rx_data->mode;
    iv_gain = (IV_Gain_TypeDef)rx_data->int_data[1];
    stage1 = (Voltage_Gain_Stage1_TypeDef)rx_data->int_data[2];
    stage2 = (Voltage_Gain_Stage2_TypeDef)rx_data->int_data[3];

    /* 第二步：确认这个增益组合确实有对应的标定曲线。 */
    callback = Module_IncentiveConfig_FindCurveFit(iv_gain, stage1, stage2);
    if (callback == NULL)
    {
        return Module_ERROR;
    }

    /* 第三步：分别填写模式参数和公共硬件参数。 */
    Module_IncentiveConfig_FillModeParameters(mode, rx_data->float_data, packet);
    Module_IncentiveConfig_FillCommonParameters(rx_data, packet);

    /*
     * 第四步：GPCI可以选择直接交给底层，或者转换成CA组合执行。
     * 其他三种模式仍按原来的命令字直接发送。
     */
    if ((mode == INCENTIVE_MODE_GPCI) &&
        (s_gpci_implementation == GPCI_IMPLEMENTATION_BY_CA))
    {
        return Module_IncentiveConfig_StartGPCIByCA(
            rx_data,
            callback,
            packet);
    }

    return Module_IncentiveConfig_SendNative(mode, callback, packet);
}

void Module_IncentiveConfig_NotifyCAFinishedFromISR(
    BaseType_t *higher_priority_task_woken)
{
    /*
     * 普通CV/DPV/CA或原生GPCI同样可能产生PE1下降沿，
     * 只有GPCI-CA状态机运行期间才转发这个通知。
     */
    if ((s_gpci_context.is_running != 0U) &&
        (s_incentive_task_handle != NULL) &&
        (higher_priority_task_woken != NULL))
    {
        (void)xTaskNotifyFromISR(
            s_incentive_task_handle,
            INCENTIVE_NOTIFY_CA_FINISHED,
            eSetBits,
            higher_priority_task_woken);
    }
}

Module_Status_t Module_IncentiveConfig_ProcessGPCI(
    TickType_t wait_ticks)
{
    uint32_t notification_value;
    Module_Status_t status;

    notification_value = 0U;

    /*
     * 最多等待wait_ticks；下降沿到来时会立即被ISR唤醒。
     * eSetBits方式会把短时间内重复的相同通知合并，避免误累计多次。
     */
    if (xTaskNotifyWait(
            0U,
            INCENTIVE_NOTIFY_CA_FINISHED,
            &notification_value,
            wait_ticks) != pdTRUE)
    {
        return Module_BUSY;
    }

    if (((notification_value & INCENTIVE_NOTIFY_CA_FINISHED) == 0U) ||
        (s_gpci_context.is_running == 0U))
    {
        return Module_BUSY;
    }

    /* 一个下降沿代表当前这一轮CA已经完整执行结束。 */
    s_gpci_context.completed_cycles++;

    if (s_gpci_context.completed_cycles >= s_gpci_context.total_cycles)
    {
        /* 已完成上位机要求的全部循环，不再发送新的CA。 */
        Module_IncentiveConfig_ClearGPCIContext();
        return Module_OK;
    }

    /* 还有剩余循环：在任务环境中启动下一次完全相同的CA。 */
    status = Serial_SendPacket(
        (uint8_t)INCENTIVE_MODE_CA,
        (double *)s_gpci_context.ca_packet);

    if (status != Module_OK)
    {
        Module_IncentiveConfig_ClearGPCIContext();
        return Module_ERROR;
    }

    return Module_OK;
}
