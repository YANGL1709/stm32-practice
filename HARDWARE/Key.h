#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

void Key_Init(void);        /* PB0~PB4 上拉输入 + TIM3 10ms 扫描中断 */
uint8_t Key_GetNum(void);   /* 非阻塞：0=无按键，1~5=按键事件，9=K3长按(1.5s)，10=K0长按(2s)SOS */

#endif
