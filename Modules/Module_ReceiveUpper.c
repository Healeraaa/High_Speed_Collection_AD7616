#include "Module_ReceiveUpper.h"
#include "Module.h"
#include "bsp.h"
#include "stdio.h"
#include "string.h"

// ========================================================================== USART1 接收缓冲区 ==========================================================================

static uint8_t usart1_rx_buffer[USART1_RX_BUFFER_SIZE] = {0};  // 接收缓冲区
static uint16_t usart1_rx_index = 0;                            // 接收指针
static uint8_t usart1_data_ready_flag = 0;                      // 接收完成标志位

/**
 * @brief  USART1 接收单个字符 (在中断处理函数中调用)
 * @param  ch: 接收到的字节
 * @retval None
 * 
 * 说明：当接收到完整数据包（包含 "ceio:" 和 "\n"）时，自动置位 usart1_data_ready_flag
 */
void Module_USART1_ReceiveChar(uint8_t ch)
{
  // 缓冲区满检查
  if (usart1_rx_index >= USART1_RX_BUFFER_SIZE - 1)
  {
    usart1_rx_index = 0;  // 缓冲区复位
    usart1_data_ready_flag = 0;  // 清除标志位
  }
  
  usart1_rx_buffer[usart1_rx_index++] = ch;
  usart1_rx_buffer[usart1_rx_index] = '\0';  // 字符串终止符
  
  // 检测是否接收到完整数据包
  if (ch == '\n' && usart1_rx_index > 1)
  {
    // 查找 "ceio:" 开头
    if (strstr((const char *)usart1_rx_buffer, "ceio:") != NULL)
    {
      usart1_data_ready_flag = 1;  // 置位接收完成标志
    }
  }
}

/**
 * @brief  判断 USART1 是否有完整数据包待处理（原子操作：读取并清除标志位）
 * @param  None
 * @retval 1 - 有完整数据包需要处理; 0 - 无新数据或已处理
 * 
 * 说明：读取标志位后自动清除，确保每个数据包仅处理一次
 * 推荐在主循环中使用此函数快速判断是否需要处理数据
 */
uint8_t Module_USART1_IsDataReady(void)
{
  // 读取标志位
  uint8_t flag = usart1_data_ready_flag;
  
  // 如果有完整数据包，清除标志位（原子操作）
  if (flag)
  {
    usart1_data_ready_flag = 0;
  }
  
  return flag;
}

/**
 * @brief  解析 USART1 接收缓冲区中的数据 (格式: "ceio:%d,%f,%f,...\n")
 * @param  pRxData: 指向接收数据结构体的指针
 * @retval BSP_OK - 解析成功; BSP_ERROR - 解析失败; BSP_BUSY - 数据不完整
 * 
 * 支持可变长度的浮点数数据，自动检测实际个数
 */
BSP_Status_t Module_USART1_ParseData(USART1_RxData_t *pRxData)
{
  char *p_start = NULL;
  char *p_end = NULL;
  char parse_buffer[128] = {0};
  char *p_token = NULL;
  char *p_saveptr = NULL;
  int mode_val = -1;
  float float_val;
  uint32_t float_count = 0;
  
  if (pRxData == NULL)
  {
    return BSP_ERROR;
  }
  
  pRxData->is_valid = 0;
  pRxData->float_count = 0;
  pRxData->mode = 0;
  
  // 查找 "ceio:" 开头
  p_start = (char *)strstr((const char *)usart1_rx_buffer, "ceio:");
  if (p_start == NULL)
  {
    return BSP_ERROR;  // 未找到起始标记
  }
  
  // 查找 "\n" 结尾
  p_end = (char *)strchr(p_start, '\n');
  if (p_end == NULL)
  {
    return BSP_BUSY;  // 数据不完整
  }
  
  // 提取 ceio: 到 \n 之间的数据
  uint16_t len = (uint16_t)(p_end - p_start);
  if (len > sizeof(parse_buffer) - 1)
  {
    len = sizeof(parse_buffer) - 1;
  }
  strncpy(parse_buffer, p_start, len);
  parse_buffer[len] = '\0';
  
  // 使用 strtok 分割数据
  // 第一步：提取 "ceio" 部分
  p_token = strtok_r(parse_buffer, ":", &p_saveptr);
  if (p_token == NULL || strcmp(p_token, "ceio") != 0)
  {
    return BSP_ERROR;
  }
  
  // 第二步：提取模式 %d
  p_token = strtok_r(NULL, ",", &p_saveptr);
  if (p_token == NULL)
  {
    return BSP_ERROR;
  }
  
  if (sscanf(p_token, "%d", &mode_val) != 1 || mode_val < 0)
  {
    return BSP_ERROR;
  }
  pRxData->mode = (uint32_t)mode_val;
  
  // 第三步：循环提取浮点数数据 %f
  while (float_count < USART1_MAX_FLOAT_DATA)
  {
    p_token = strtok_r(NULL, ",", &p_saveptr);
    if (p_token == NULL)
    {
      break;  // 没有更多数据
    }
    
    // 移除末尾的 \n 或空格
    char *p_temp = p_token;
    while (*p_temp && (*p_temp == ' ' || *p_temp == '\n' || *p_temp == '\r'))
    {
      p_temp++;
    }
    if (*p_temp == '\0')
    {
      break;
    }
    
    // 尝试解析浮点数
    if (sscanf(p_token, "%f", &float_val) == 1)
    {
      pRxData->float_data[float_count] = float_val;
      float_count++;
    }
    else
    {
      break;  // 解析失败，停止
    }
  }
  
  // 验证是否解析到数据
  if (float_count == 0)
  {
    return BSP_ERROR;  // 没有解析到任何浮点数
  }
  
  pRxData->float_count = float_count;
  pRxData->is_valid = 1;
  
  // 清空已处理的数据
  usart1_rx_index = 0;
  usart1_data_ready_flag = 0;  // 清除标志位
  memset(usart1_rx_buffer, 0, USART1_RX_BUFFER_SIZE);
  
  return BSP_OK;
}




/**
 * @brief  串口接收中断处理函数
 */
void USART1_IRQ_Task(void)
{
    if (LL_USART_IsActiveFlag_RXNE_RXFNE(USART1) == SET) {
        uint8_t rx_data = LL_USART_ReceiveData8(USART1);
        Module_USART1_ReceiveChar(rx_data);  // 将接收的字符存入缓冲区
    }
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
    USART1_IRQ_Task();
}