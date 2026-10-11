#include "Module_ReceiveUpper.h"
#include "Module.h"
#include "bsp.h"
#include "stdio.h"
#include "string.h"

// ========================================================================== 上位机 USB CDC 接收缓冲区 ==========================================================================

static uint8_t upper_rx_buffer[UPPER_RX_BUFFER_SIZE] = {0};
static uint8_t upper_parse_buffer[UPPER_RX_BUFFER_SIZE] = {0};
static volatile uint16_t upper_rx_index = 0U;
static volatile uint8_t upper_data_ready_flag = 0U;
static volatile uint32_t upper_rx_drop_count = 0U;

/**
 * @brief  将单个USB CDC接收字节写入上位机缓冲区
 * @param  ch: 接收到的字节
 * @retval None
 * 
 * 说明：收齐完整的 "ceiod:...\\n" 和 "ceiof:...\\n" 后置位数据就绪标志
 */
static void Module_UpperRx_ReceiveChar(uint8_t ch)
{
  // 缓冲区满检查
  if (upper_rx_index >= UPPER_RX_BUFFER_SIZE - 1U)
  {
    upper_rx_index = 0U;
    upper_rx_buffer[0] = '\0';
    upper_rx_drop_count++;
  }
  
  upper_rx_buffer[upper_rx_index++] = ch;
  upper_rx_buffer[upper_rx_index] = '\0';  // 字符串终止符
  
  // 检测是否接收到完整数据包（两条完整命令）
  if (ch == '\n' && upper_rx_index > 1U)
  {
    // 需要同时找到 "ceiod:" 和 "ceiof:"，以及至少两个 "\n"
    char *p_ceiod = (char *)strstr((const char *)upper_rx_buffer, "ceiod:");
    char *p_ceiof = (char *)strstr((const char *)upper_rx_buffer, "ceiof:");
    
    if (p_ceiod != NULL && p_ceiof != NULL)
    {
      // 检查是否同时有两个 "\n"（每条命令一个）
      char *p_first_newline = (char *)strchr((const char *)upper_rx_buffer, '\n');
      if (p_first_newline != NULL)
      {
        char *p_second_newline = (char *)strchr(p_first_newline + 1, '\n');
        if (p_second_newline != NULL)
        {
          upper_data_ready_flag = 1U;  // 置位接收完成标志
        }
      }
    }
  }
}

void Module_UpperRx_ReceiveBytes(const uint8_t *p_data, uint32_t length)
{
  uint32_t i;

  if ((p_data == NULL) || (length == 0U))
  {
    return;
  }

  for (i = 0U; i < length; i++)
  {
    /* 上一组配置尚未被任务取走时，不覆盖这组完整数据。 */
    if (upper_data_ready_flag != 0U)
    {
      upper_rx_drop_count++;
      continue;
    }

    Module_UpperRx_ReceiveChar(p_data[i]);
  }
}

/**
 * @brief  判断USB CDC是否收到完整的一组上位机配置
 * @param  None
 * @retval 1 - 有完整数据包需要处理; 0 - 无新数据或已处理
 * 
 * 说明：本函数只读取标志，解析函数原子取走数据后才清除
 */
uint8_t Module_UpperRx_IsDataReady(void)
{
  return upper_data_ready_flag;
}

/**
 * @brief  解析USB CDC接收缓冲区中的两条完整命令数据
 * 协议格式: "ceiod:%d,%d,...\nceiof:%f,%f,...\n"
 * ceiod的第1个%d为模式(mode)，后续%d对应u8_array[0-...]
 * ceiof的%f直接对应data_converter[0-...]
 * @param  pRxData: 指向接收数据结构体的指针
 * @retval Module_OK - 两条命令都解析成功; Module_ERROR - 解析失败; Module_BUSY - 数据不完整
 */
Module_Status_t Module_UpperRx_ParseData(Upper_RxData_t *pRxData)
{
  uint16_t snapshot_length;
  uint32_t primask;
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

  /* USB接收在中断中发生，先原子取走完整数据，再在任务环境解析。 */
  primask = __get_PRIMASK();
  __disable_irq();

  if (upper_data_ready_flag == 0U)
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return Module_BUSY;
  }

  snapshot_length = upper_rx_index;
  memcpy(upper_parse_buffer, upper_rx_buffer, snapshot_length);
  upper_parse_buffer[snapshot_length] = '\0';

  upper_rx_index = 0U;
  upper_rx_buffer[0] = '\0';
  upper_data_ready_flag = 0U;

  if (primask == 0U)
  {
    __enable_irq();
  }
  
  // 初始化结构体
  pRxData->is_valid = 0;
  pRxData->float_count = 0;
  pRxData->int_count = 0;
  pRxData->mode = 0;
  memset(pRxData->int_data, 0, sizeof(pRxData->int_data));
  memset(pRxData->float_data, 0, sizeof(pRxData->float_data));
  
  // ========== 第一步：解析 "ceiod:" 格式的整数数据 ==========
  p_ceiod_start = (char *)strstr((const char *)upper_parse_buffer, "ceiod:");
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
  while (int_count < UPPER_RX_MAX_INT_DATA)
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
  p_ceiof_start = (char *)strstr((const char *)upper_parse_buffer, "ceiof:");
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
  while (float_count < UPPER_RX_MAX_FLOAT_DATA)
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
  
  return Module_OK;
}




uint32_t Module_UpperRx_GetDropCount(void)
{
    return upper_rx_drop_count;
}
