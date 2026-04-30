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
 * 说明：当同时接收到完整的 "ceiod:...\\n" 和 "ceiof:...\\n" 两条命令时，自动置位 usart1_data_ready_flag
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
  
  // 检测是否接收到完整数据包（两条完整命令）
  if (ch == '\n' && usart1_rx_index > 1)
  {
    // 需要同时找到 "ceiod:" 和 "ceiof:"，以及至少两个 "\n"
    char *p_ceiod = (char *)strstr((const char *)usart1_rx_buffer, "ceiod:");
    char *p_ceiof = (char *)strstr((const char *)usart1_rx_buffer, "ceiof:");
    
    if (p_ceiod != NULL && p_ceiof != NULL)
    {
      // 检查是否同时有两个 "\n"（每条命令一个）
      char *p_first_newline = (char *)strchr((const char *)usart1_rx_buffer, '\n');
      if (p_first_newline != NULL)
      {
        char *p_second_newline = (char *)strchr(p_first_newline + 1, '\n');
        if (p_second_newline != NULL)
        {
          usart1_data_ready_flag = 1;  // 置位接收完成标志
        }
      }
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
 * @brief  解析 USART1 接收缓冲区中的两条完整命令数据
 * 协议格式: "ceiod:%d,%d,...\nceiof:%f,%f,...\n"
 * ceiod的第1个%d为模式(mode)，后续%d对应u8_array[0-...]
 * ceiof的%f直接对应data_converter[0-...]
 * @param  pRxData: 指向接收数据结构体的指针
 * @retval Module_OK - 两条命令都解析成功; Module_ERROR - 解析失败; Module_BUSY - 数据不完整
 */
Module_Status_t Module_USART1_ParseData(USART1_RxData_t *pRxData)
{
  char *p_ceiod_start = NULL;
  char *p_ceiof_start = NULL;
  char *p_end = NULL;
  char ceiod_buffer[128] = {0};
  char ceiof_buffer[128] = {0};
  char *p_token = NULL;
  char *p_saveptr = NULL;
  int int_val = -1;
  float float_val;
  uint32_t int_count = 0;
  uint32_t float_count = 0;
  
  if (pRxData == NULL)
  {
    return Module_ERROR;
  }
  
  // 初始化结构体
  pRxData->is_valid = 0;
  pRxData->float_count = 0;
  pRxData->int_count = 0;
  pRxData->mode = 0;
  memset(pRxData->int_data, 0, sizeof(pRxData->int_data));
  memset(pRxData->float_data, 0, sizeof(pRxData->float_data));
  
  // ========== 第一步：解析 "ceiod:" 格式的整数数据 ==========
  p_ceiod_start = (char *)strstr((const char *)usart1_rx_buffer, "ceiod:");
  if (p_ceiod_start == NULL)
  {
    return Module_ERROR;  // 缺少 ceiod 命令
  }
  
  // 查找 ceiod 后面的 "\n"
  p_end = (char *)strchr(p_ceiod_start, '\n');
  if (p_end == NULL)
  {
    return Module_BUSY;  // ceiod 数据不完整
  }
  
  // 提取 ceiod: 到 \n 之间的数据
  uint16_t len = (uint16_t)(p_end - p_ceiod_start);
  if (len > sizeof(ceiod_buffer) - 1)
  {
    len = sizeof(ceiod_buffer) - 1;
  }
  strncpy(ceiod_buffer, p_ceiod_start, len);
  ceiod_buffer[len] = '\0';
  
  // 使用 strtok 分割 ceiod 数据
  p_token = strtok_r(ceiod_buffer, ":", &p_saveptr);
  if (p_token == NULL || strcmp(p_token, "ceiod") != 0)
  {
    return Module_ERROR;
  }
  
  // 第一个%d作为mode
  p_token = strtok_r(NULL, ",", &p_saveptr);
  if (p_token == NULL)
  {
    return Module_ERROR;
  }
  if (sscanf(p_token, "%d", &int_val) != 1 || int_val < 0)
  {
    return Module_ERROR;
  }
  pRxData->mode = (uint32_t)int_val;
  
  // 循环提取后续整数数据到 int_data[]（不定长）
  while (int_count < USART1_MAX_INT_DATA)
  {
    p_token = strtok_r(NULL, ",", &p_saveptr);
    if (p_token == NULL)
    {
      break;
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
    
    // 尝试解析整数
    if (sscanf(p_token, "%d", &int_val) == 1 && int_val >= 0)
    {
      pRxData->int_data[int_count] = (uint32_t)int_val;
      int_count++;
    }
    else
    {
      break;  // 解析失败，停止
    }
  }
  
  pRxData->int_count = int_count;
  
  // ========== 第二步：解析 "ceiof:" 格式的浮点数数据 ==========
  p_ceiof_start = (char *)strstr((const char *)usart1_rx_buffer, "ceiof:");
  if (p_ceiof_start == NULL)
  {
    return Module_ERROR;  // 缺少 ceiof 命令
  }
  
  // 查找 ceiof 后面的 "\n"
  p_end = (char *)strchr(p_ceiof_start, '\n');
  if (p_end == NULL)
  {
    return Module_BUSY;  // ceiof 数据不完整
  }
  
  // 提取 ceiof: 到 \n 之间的数据
  len = (uint16_t)(p_end - p_ceiof_start);
  if (len > sizeof(ceiof_buffer) - 1)
  {
    len = sizeof(ceiof_buffer) - 1;
  }
  strncpy(ceiof_buffer, p_ceiof_start, len);
  ceiof_buffer[len] = '\0';
  
  // 使用 strtok 分割 ceiof 数据
  p_saveptr = NULL;  // 重置 saveptr
  p_token = strtok_r(ceiof_buffer, ":", &p_saveptr);
  if (p_token == NULL || strcmp(p_token, "ceiof") != 0)
  {
    return Module_ERROR;
  }
  
  // 循环提取所有浮点数数据到 float_data[]（不定长）
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
  
  // 验证是否解析到浮点数数据
  if (float_count == 0)
  {
    return Module_ERROR;  // 没有解析到任何浮点数
  }
  
  pRxData->float_count = float_count;
  pRxData->is_valid = 1;
  
  // 清空已处理的数据
  usart1_rx_index = 0;
  usart1_data_ready_flag = 0;
  memset(usart1_rx_buffer, 0, USART1_RX_BUFFER_SIZE);
  
  return Module_OK;
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