/*
 * Integration hook: call once from main() after HAL_Init, SystemClock_Config, and USART init.
 * Retarget printf to UART (see README) before calling.
 */

#include "stm32f4xx_hal.h"

#include "t5_profile_methods.h"

void T5_Profile_AfterInit(void)
{
  HAL_Delay(3000);
  T5_Profile_RunAll();
}
