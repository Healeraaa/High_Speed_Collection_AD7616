#include "bsp.h"
#include "Module.h"

uint8_t t1 = 0;
uint8_t t2 = 1;
uint8_t t3 = 2;
uint8_t t4 = 3;
uint8_t str[] = "Hello World!";

int main(void)
{
  BSP_Init();
  Module_Config();
  while (1)
  {
    uint8_t ucKeyCode = Moudle_Key_GetFifoBuffer();	/* 读取键值, 无键按下时返回 KEY_NONE = 0 */
		if (ucKeyCode != KEY_NONE)
		{
			switch (ucKeyCode)
			{
				case KEY_1_DOWN:			/* K1键按下 */
					printf("K1_Dowm\r\n");
					break;

				case KEY_1_UP:				/* K1键弹起 */
					printf("K1_Up\r\n");
					break;
				case KEY_1_LONG:				/* K1键弹起 */
					printf("K1_Long\r\n");
					break;

			

				default:
					/* 其它的键值不处理 */
					break;
			}
  		}
		LL_mDelay(10);  /* 延时10ms */
}

}


