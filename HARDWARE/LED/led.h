#ifndef	__LED_H
#define	__LED_H

#include "stm32f10x.h"                  // Device header

#define LED_GPIO_PROT		GPIOB
#define LED_GPIO_PIN	  GPIO_Pin_13

void LED_Init(void);
void LED_Toggle(void);
void LED_On(void);
void LED_Off(void);
void LED_Twinkle(void);

/* 风扇执行器：继电器驱动，高电平启动 */
#define FAN_On()   LED_On()
#define FAN_Off()  LED_Off()
#endif
