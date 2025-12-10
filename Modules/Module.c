#include "main.h"
#include "Module.h"
#include "Module_Key.h"




void Module_Config(void)
{
  Module_KEY_Config();
}

void Module_RunPer10ms(void)
{
  Moudle_Key_Scan10ms();
}
