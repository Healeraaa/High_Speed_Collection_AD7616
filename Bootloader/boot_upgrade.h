#ifndef __BOOT_UPGRADE_H__
#define __BOOT_UPGRADE_H__

#include "boot_protocol.h"

/**
 * @brief 清空本次升级会话的 RAM 状态
 *
 * 不擦除 Flash。上电进入升级模式时调用一次；START、写入失败或 CRC
 * 失败时也会调用，使后续 DATA 不能继续沿用一个已经失败的会话。
 */
void Boot_Upgrade_Init(void);

/**
 * @brief 处理 START 命令并擦除 Bank2
 * @param frame START 帧；Payload 为镜像长度和期望 CRC32，各 4 字节小端
 * @return BOOT_ERROR_NONE 表示擦除成功，可以开始接收 DATA
 */
BootError_t Boot_Upgrade_HandleStart(
    const BootFrame_t *frame
);

/**
 * @brief 处理一个 DATA 包并写入 App 区
 * @param frame DATA 帧；Payload 前 4 字节是镜像偏移，后面是固件数据
 * @param next_offset 返回设备下一次期望收到的镜像偏移
 * @return BOOT_ERROR_NONE 表示本包已经正确保存
 */
BootError_t Boot_Upgrade_HandleData(
    const BootFrame_t *frame,
    uint32_t *next_offset
);

/**
 * @brief 处理 END 命令，完成整体校验和掉电安全提交
 * @param frame END 帧；当前协议要求 Payload 为空
 * @param image_crc32 返回设备实际计算出的完整镜像 CRC32
 * @param image_size 返回最终确认的镜像长度
 * @return BOOT_ERROR_NONE 表示镜像已经可以安全启动
 */
BootError_t Boot_Upgrade_HandleEnd(
    const BootFrame_t *frame,
    uint32_t *image_crc32,
    uint32_t *image_size
);
#endif
