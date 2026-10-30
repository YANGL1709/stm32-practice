#ifndef __BEEP_H
#define __BEEP_H
#include "stm32f10x.h"

/* 喇叭：PB6 / TIM4_CH1，1kHz PWM，PNP 驱动（低电平导通） */
void Beep_Init(void);
void Beep_On(void);
void Beep_Off(void);

/* 风扇/继电器：PB13，NPN 驱动，高电平启动 */
void Fan_Init(void);
void Fan_On(void);
void Fan_Off(void);

#endif
