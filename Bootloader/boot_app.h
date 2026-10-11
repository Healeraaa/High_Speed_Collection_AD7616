#ifndef __BOOT_APP_H__
#define __BOOT_APP_H__

#include <stdint.h>

/**
 * @brief 检查已安装 App 是否可以安全启动
 *
 * 检查顺序：
 * 1. 检查镜像信息和最终提交标记是否存在且相互一致；
 * 2. 检查向量表中的初始 MSP 和 Reset_Handler 地址是否合法；
 * 3. 按元数据保存的镜像长度重新计算整个 App 的 CRC32。
 *
 * @return 1U：App 有效；0U：App 无效
 */
uint8_t Boot_App_IsValid(void);

/**
 * @brief 清理 Boot 运行环境并跳转到 App
 *
 * 跳转前会复位时钟和外设、关闭中断、清理 Cache/MPU/NVIC，随后：
 * 1. 把 VTOR 指向 App 的向量表；
 * 2. 把 MSP 改为 App 向量表中的初始栈地址；
 * 3. 跳转到 App 的 Reset_Handler。
 *
 * @note 调用前必须先通过 Boot_App_IsValid() 校验；正常情况下不会返回。
 */
void Boot_App_Jump(void);

#endif
