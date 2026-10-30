/*
 * mq2.c -- MQ-2 烟雾/可燃气体传感器驱动
 *
 * 硬件：模拟输出接 PA0，ADC1 通道 0。
 *
 * Proteus 的 STM32 模型对 ADC2 支持不完整，所以烟雾和心率共用 ADC1：
 * 读烟雾时把通道临时切到 0，读完恢复通道 1（临界区内完成，不与心率中断冲突）。
 *
 * 注意：调用前必须先 AD_Init()，由它完成 ADC1 的时钟、校准和通道 1 配置。
 */

#include "mq2.h"

void MQ2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    /* ADC1 本身由 AD_Init() 初始化 */
}

u16 MQ2_Read(void)
{
    u32 sum;
    u8 i;

    __disable_irq();
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);

    /* 通道刚切换，内部采样保持电容还残留 PA1（心率信号）的电压，
     * 第一次转换会混入串扰 -> 作废，让电容充到 PA0 的真实电平。
     * 唤醒低功耗后 ADC 刚重新使能，此问题更明显。 */
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    (void)ADC_GetConversionValue(ADC1);

    /* 连续采样 4 次取平均，进一步平滑波动 */
    sum = 0;
    for (i = 0; i < 4; i++)
    {
        ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
        ADC_SoftwareStartConvCmd(ADC1, ENABLE);
        while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
        sum += ADC_GetConversionValue(ADC1);
    }

    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);  /* 恢复心率通道 */
    __enable_irq();

    return (u16)(sum / 4);
}
