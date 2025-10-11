#include "bsp.h"
uint8_t t1 = 0;
uint8_t t2 = 1;
uint8_t t3 = 2;
uint8_t t4 = 3;
uint8_t str[] = "Hello World!";


int main(void)
{
  BSP_Init();
  while (1)
  {
    EventStartA(0);
    LL_mDelay(500);
    EventStopA(0);

    EventRecord2(1+EventLevelAPI,t1,t2);
    EventRecord4(2+EventLevelOp,t1,t2,t3,t4);
    EventRecordData(3+EventLevelOp,str,sizeof(str));
    printf("Hello World!\n");
  }

}


