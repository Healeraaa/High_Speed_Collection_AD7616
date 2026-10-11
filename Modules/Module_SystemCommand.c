#include "Module_SystemCommand.h"

#include "bsp_usart.h"
#include "sys.h"

#include <string.h>

/*
 * 系统命令模块
 * ============
 *
 * USART1_IRQHandler只负责把每个收到的字节放入BSP环形缓冲区。本模块每
 * 10 ms从环形缓冲区取出所有待处理字节，遇到回车或换行后才比较命令。
 * 因此strcmp、串口回复、备份寄存器写入和系统复位都不会发生在中断中。
 */

/* 当前只支持两条短命令，32字节足够容纳命令和末尾的字符串结束符。 */
#define MODULE_SYSTEM_COMMAND_BUFFER_SIZE  32U

static char s_command_buffer[MODULE_SYSTEM_COMMAND_BUFFER_SIZE];
static uint16_t s_command_length;
static uint8_t s_discard_until_line_end;

/**
 * @brief 回复OK并确保最后一个停止位已经发送完毕
 * @note  BSP_USART1_SendBuffer()返回时已经等待USART1的TC标志。
 */
static void Module_SystemCommand_SendOk(void)
{
    static const uint8_t response[] = "OK\r\n";

    BSP_USART1_SendBuffer(
        response,
        (uint16_t)(sizeof(response) - 1U));
}

/**
 * @brief 在所有写寄存器和串口发送完成后触发CPU系统复位
 */
static void Module_SystemCommand_ResetNow(void)
{
    /*
     * DSB保证之前对RTC备份寄存器和USART寄存器的访问已经完成；
     * ISB清空流水线，随后由NVIC触发与按复位键相同的系统复位流程。
     */
    __DSB();
    __ISB();
    NVIC_SystemReset();

    /* NVIC_SystemReset正常不会返回；保留死循环防止异常情况下继续运行。 */
    while (1)
    {
    }
}

/**
 * @brief 处理一个已经去掉回车/换行的完整命令
 */
static void Module_SystemCommand_Execute(const char *command)
{
    if (strcmp(command, "reboot") == 0)
    {
        /*
         * reboot只做普通复位，不请求进入Bootloader。先清除可能存在的旧
         * 标志，可以保证BOOT按“未按K1”的正常路径重新启动有效APP。
         */
        System_BootRequest_Clear();
        Module_SystemCommand_SendOk();
        Module_SystemCommand_ResetNow();
    }

    if (strcmp(command, "reboot loader") == 0)
    {
        /*
         * 标志必须在复位前写好。复位后BOOT会读取并立即清除它，然后调用
         * 与K1按下时完全相同的Boot_Service_Run()升级服务。
         */
        System_BootRequest_Set();
        Module_SystemCommand_SendOk();
        Module_SystemCommand_ResetNow();
    }

    /* 未识别命令不回复、不复位，继续等待下一行。 */
}

void Module_SystemCommand_Config(void)
{
    s_command_length = 0U;
    s_discard_until_line_end = 0U;
    s_command_buffer[0] = '\0';
}

void Module_SystemCommand_RunPer10ms(void)
{
    uint8_t received_byte;

    /* 一次运行把环形缓冲区中的全部待处理字节取完。 */
    while (BSP_USART1_ReadByte(&received_byte) != 0U)
    {
        if ((received_byte == (uint8_t)'\r') ||
            (received_byte == (uint8_t)'\n'))
        {
            if (s_discard_until_line_end != 0U)
            {
                /* 超长行已经被丢弃；行结束后恢复接收下一条新命令。 */
                s_discard_until_line_end = 0U;
                s_command_length = 0U;
                continue;
            }

            if (s_command_length == 0U)
            {
                /* 忽略空行，也自然兼容CRLF中的第二个结束字符。 */
                continue;
            }

            s_command_buffer[s_command_length] = '\0';
            Module_SystemCommand_Execute(s_command_buffer);

            /* 普通未知命令会返回这里；有效命令会在复位函数中停止执行。 */
            s_command_length = 0U;
            s_command_buffer[0] = '\0';
            continue;
        }

        if (s_discard_until_line_end != 0U)
        {
            continue;
        }

        if (s_command_length < (MODULE_SYSTEM_COMMAND_BUFFER_SIZE - 1U))
        {
            s_command_buffer[s_command_length] = (char)received_byte;
            s_command_length++;
        }
        else
        {
            /*
             * 行长度超过缓存时，不解析截断后的内容，而是丢弃到下一次行
             * 结束，避免一个超长字符串的前缀被错误识别成有效命令。
             */
            s_command_length = 0U;
            s_command_buffer[0] = '\0';
            s_discard_until_line_end = 1U;
        }
    }
}
