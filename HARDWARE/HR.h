#ifndef __HR_H
#define __HR_H

#include "stm32f10x.h"

/* ======================== 心率检测模块 ========================
   硬件依赖：ADC1_IN1（PA1）模拟输入
   移植步骤：1) 调用 AD_Init()
             2) 调用 HR_Init()
             3) 主循环读 HeartRate 变量（单位 BPM，0 表示尚未测出）

   采样与检测全部在 TIM2 中断里完成，主循环无需参与，也不会被
   刷屏等耗时操作影响采样周期。
   ============================================================== */

void HR_Init(void);						/* 启动心率采样定时器，需先调用 AD_Init() */

extern volatile uint16_t HeartRate;		/* 心率，单位 BPM，0 表示尚未测出 */

#endif
