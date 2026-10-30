#ifndef __MQ2_H
#define __MQ2_H

#include "stm32f10x.h"

/* MQ-2 烟雾/可燃气体传感器：PA0，ADC1 通道 0 */
void MQ2_Init(void);
u16  MQ2_Read(void);

#endif
