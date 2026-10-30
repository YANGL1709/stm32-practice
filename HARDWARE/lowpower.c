/*
 * lowpower.c -- Stop 模式/软件低功耗与按键唤醒
 */

#include "stm32f10x.h"
#include "lowpower.h"
#include "app.h"
#include "beep.h"
#include "oled.h"
#include "HR.h"
#include "Key.h"
#include "AD.h"
#include "mq2.h"
#include "ds1302.h"
#include "delay.h"

void Key_EXTI_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

    /* EXTI0~3 映射到 PB0~PB3，EXTI4 映射到 PB4（每 4 位一组，1 = PB） */
    AFIO->EXTICR[0] = (AFIO->EXTICR[0] & ~0xFFFFu) | 0x1111u;
    AFIO->EXTICR[1] = (AFIO->EXTICR[1] & ~0x000Fu) | 0x0001u;

    EXTI->IMR  |= 0x001Fu;              /* 使能 line0~4 */
    EXTI->FTSR |= 0x001Fu;              /* 下降沿触发 */
    EXTI->RTSR &= ~0x001Fu;
    EXTI->PR    = 0x001Fu;              /* 清除挂起标志 */

    NVIC_SetPriority(EXTI0_IRQn, 3);
    NVIC_SetPriority(EXTI1_IRQn, 3);
    NVIC_SetPriority(EXTI2_IRQn, 3);
    NVIC_SetPriority(EXTI3_IRQn, 3);
    NVIC_SetPriority(EXTI4_IRQn, 3);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI1_IRQn);
    NVIC_EnableIRQ(EXTI2_IRQn);
    NVIC_EnableIRQ(EXTI3_IRQn);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

void EXTI0_IRQHandler(void) { EXTI->PR = 0x0001u; }
void EXTI1_IRQHandler(void) { EXTI->PR = 0x0002u; }
void EXTI2_IRQHandler(void) { EXTI->PR = 0x0004u; }
void EXTI3_IRQHandler(void) { EXTI->PR = 0x0008u; }
void EXTI4_IRQHandler(void) { EXTI->PR = 0x0010u; }

void EnterStop(void)
{
    /* 先关外设和风扇，进一步降低电流 */
    Beep_Off();
    Fan_Off();
    OLED_Clear();                       /* 先清屏，保证仿真里画面一定熄灭 */
#if USE_STOP_MODE
    OLED_DisPlay_Off();                 /* 真低功耗时再关屏省电 */
#endif
    TIM_Cmd(TIM2, DISABLE);             /* 心率采样 */
    TIM_Cmd(TIM3, DISABLE);             /* 按键扫描 */
    TIM_Cmd(TIM4, DISABLE);             /* 喇叭 PWM */
    ADC_Cmd(ADC1, DISABLE);

    EXTI->PR = 0x001Fu;                 /* 清掉残留事件 */

#if USE_STOP_MODE
    /* 清除定时器的挂起中断，否则 WFI 会因为已有 pending 中断而立即返回
     * （心率报警时 TIM2 中断频繁，最容易触发这个现象） */
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI1_IRQn);
    NVIC_ClearPendingIRQ(EXTI2_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI4_IRQn);

    PWR->CR |= PWR_CR_LPDS;             /* Stop 下使用低功耗稳压器 */
    SCB->SCR |= SCB_SCR_SLEEPDEEP;      /* 进入 STOP 而不是 SLEEP */
    __WFI();                            /* 等待 EXTI 唤醒 */
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP;

    /* Stop 唤醒后时钟回到 HSI，需要重新配置到 72MHz */
    SystemInit();
    delay_init(72);
#else
    /* 软件低功耗：熄屏 + 停外设，轮询按键唤醒
     * 先等进入休眠时按着的 K3 松开，否则会立刻被当成唤醒键 */
    while ((GPIOB->IDR & 0x001Fu) != 0x001Fu)
    {
        delay_ms(20);
    }

    /* 再等待任意键按下唤醒 */
    while ((GPIOB->IDR & 0x001Fu) == 0x001Fu)
    {
        delay_ms(20);
    }
    delay_ms(20);                       /* 消抖 */
#endif

    EXTI->PR = 0x001Fu;

    /* 重新初始化外设 */
    AD_Init();
    MQ2_Init();
    HR_Init();
    Key_Init();
    Beep_Init();
    OLED_DisPlay_On();
    OLED_Clear();

    /* 唤醒后 ADC 刚重新使能/校准，先做一次废采样让模型稳定，
     * 避免唤醒后的前几次烟雾读数跳变 */
    (void)MQ2_Read();

    /* 丢弃唤醒按键产生的事件，避免一唤醒就切换页面 */
    delay_ms(300);
    {
        u8 n = 0;
        while (Key_GetNum() != 0 && n < 50) { delay_ms(20); n++; }
    }

    DS1302_read_realTime();
}
