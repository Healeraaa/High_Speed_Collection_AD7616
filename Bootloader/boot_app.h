#ifndef __BOOT_APP_H__
#define __BOOT_APP_H__

#include <stdint.h>

/**
 * @brief 检查已安装 App 是否可以安全启动
 *
 * 校验内容包括提交标记、镜像元数据、向量表以及整个镜像的 CRC32。
 *
 * @return 1U：App 有效；0U：App 无效
 */
uint8_t Boot_App_IsValid(void);

/**
 * @brief 清理 Boot 运行环境并跳转到 App
 * @note 调用前必须先通过 Boot_App_IsValid() 校验；正常情况下不会返回
 */
void Boot_App_Jump(void);

#endif
