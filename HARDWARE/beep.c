/*
 * beep.c -- 喇叭与风扇执行器驱动
 *
 * 喇叭：PB6 = TIM4_CH1，1kHz PWM 方波；PNP 三极管驱动，低电平导通。
 *   静音：CCR = 满周期 -> 输出恒高 -> PNP 截止
 *   发声：CCR = 50%   -> 输出 1kHz 方波
 * 风扇：PB13 推挽输出，NPN 驱动，高电平启动。
 */

#include "beep.h"

#define BEEP_FREQ_HZ 1000
#define BEEP_PERIOD  (1000000 / BEEP_FREQ_HZ)

#define FAN_PORT GPIOB
#define FAN_PIN  GPIO_Pin_13

/* 喇叭初始化：TIM4_CH1 PWM，初始输出恒高（静音） */
void Beep_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    /* PB6 复用推挽输出（TIM4_CH1 默认映射） */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 72MHz / 72 = 1MHz；计 1000000/f 个数 = f Hz */
    TIM_TimeBaseStructure.TIM_Prescaler         = 72 - 1;
    TIM_TimeBaseStructure.TIM_Period            = (1000000 / BEEP_FREQ_HZ) - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    /* 通道1：PWM1，初始 CCR=满周期（输出恒高，PNP 截止、静音） */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = BEEP_PERIOD;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);

    TIM_Cmd(TIM4, ENABLE);              /* 计数器常开，用占空比控制响/不响 */
    Beep_Off();
}

void Beep_On(void)
{
    TIM_SetCompare1(TIM4, BEEP_PERIOD / 2);     /* 50% 占空比 */
}

void Beep_Off(void)
{
    TIM_SetCompare1(TIM4, BEEP_PERIOD);         /* 100% -> 恒高 */
}

/* 风扇初始化：PB13 推挽输出，默认关闭 */
void Fan_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_ResetBits(FAN_PORT, FAN_PIN);          /* 先拉低，避免初始化瞬间误启动 */
    GPIO_InitStructure.GPIO_Pin   = FAN_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(FAN_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(FAN_PORT, FAN_PIN);
}

void Fan_On(void)
{
    GPIO_SetBits(FAN_PORT, FAN_PIN);
}

void Fan_Off(void)
{
    GPIO_ResetBits(FAN_PORT, FAN_PIN);
}
