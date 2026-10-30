#include "stm32f10x.h"
#include "Key.h"

/* 长按连发参数（TIM3 每 10ms 中断一次） */
#define KEY_REPEAT_DELAY   50       /* 按住 50 x 10ms = 500ms 后开始连发 */
#define KEY_REPEAT_PERIOD  10       /* 之后每 10 x 10ms = 100ms 连发一次 */
#define KEY_K3_LONG_TICKS  150      /* K3 长按 150 x 10ms = 1.5 秒 */
#define KEY_K0_LONG_TICKS  200      /* K0 长按 200 x 10ms = 2 秒（SOS） */

/**
  * 按键模块
  *   PB0~PB3 上拉输入，按键另一端接 GND，低电平有效。
  *
  * 为什么用定时器中断扫描？
  *   本工程主循环约 1.3 秒一轮（DS18B20 转换 750ms + OLED 刷新），
  *   如果在主循环里轮询按键，快速点击会落在两次采样之间被漏掉。
  *   TIM3 每 10ms 扫描一次并做 20ms 去抖，按下沿记录到 Key_Event，
  *   主循环随时来取即可，不会丢按键。
  */

static volatile u8 Key_Event = 0;      /* 1~4：待处理的按键事件 */

/* 读原始键值：0=无按键，1~5=PB0~PB4 按下 */
static u8 Key_ReadRaw(void)
{
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0) return 1;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0) return 2;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_2) == 0) return 3;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_3) == 0) return 4;
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_4) == 0) return 5;
    return 0;
}

/**
  * 函    数：按键初始化
  * 说    明：配置 PB0~PB3 为上拉输入，并启动 TIM3 10ms 扫描中断。
  *           注意：本工程 TIM2 已被心率模块占用，按键使用 TIM3。
  */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* PB3/PB4 复位后默认是 JTDO/NJTRST（JTAG 占用），必须关闭 JTAG 才能当普通 IO 用。
     * SWJ_CFG = 010：关闭 JTAG-DP，保留 SW-DP（ST-Link 仍可下载/调试） */
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    AFIO->MAPR = (AFIO->MAPR & ~(0x7u << 24)) | (0x2u << 24);

    /* 引脚：上拉输入 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* TIM3：72MHz / 7200 = 10kHz，计 100 个数 = 10ms */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_TimeBaseStructure.TIM_Prescaler         = 7200 - 1;
    TIM_TimeBaseStructure.TIM_Period            = 100 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_ClearFlag(TIM3, TIM_FLAG_Update);       /* 先清标志，避免初始化后立刻进中断 */
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM3, ENABLE);
}

/**
  * 函    数：TIM3 中断服务函数（10ms 一次）
  * 说    明：连续 2 次采样（20ms）相同才认为状态稳定。
  *           K0/K1/K2：按下沿立即报键码；
  *           K3：松手时若为短按才报键码 4，按住 1.5 秒报键码 9（进入低功耗）；
  *           K0：短按松手报键码 1（切页），按住 2 秒报键码 10（SOS 呼救）；
  *           K1/K2 按住 500ms 后每 100ms 连发一次。
  */
void TIM3_IRQHandler(void)
{
    static u8 raw_prev  = 0;    /* 上一次原始采样值 */
    static u8 cnt       = 0;    /* 稳定计数 */
    static u8 stable    = 0;    /* 去抖后的状态 */
    static u8 hold_cnt  = 0;    /* K1/K2 连发计时 */
    static u8 rep_cnt   = 0;    /* 连发间隔计数 */
    static u8 k3_hold   = 0;    /* K3 按住时长 */
    static u8 k3_long   = 0;    /* K3 已触发长按 */
    static u8 k0_hold   = 0;    /* K0 按住时长 */
    static u8 k0_long   = 0;    /* K0 已触发长按（SOS） */
    u8 raw;

    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == RESET)
        return;
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

    raw = Key_ReadRaw();

    if (raw == raw_prev)
    {
        if (cnt < 250) cnt++;
    }
    else
    {
        raw_prev = raw;
        cnt = 0;
    }

    if (cnt >= 2)                       /* 连续 20ms 不变 */
    {
        if (raw != stable)
        {
            u8 prev = stable;

            stable   = raw;
            hold_cnt = 0;
            rep_cnt  = 0;

            if (raw == 4)               /* K3 按下：等松手或长按再决定 */
            {
                k3_hold = 0;
                k3_long = 0;
            }
            else if (raw == 1)          /* K0 按下：等松手或长按再决定 */
            {
                k0_hold = 0;
                k0_long = 0;
            }
            else if (raw != 0)          /* 其它键：按下沿报键码 */
            {
                Key_Event = raw;
            }

            if (prev == 4 && !k3_long && raw != 4)
            {
                Key_Event = 4;          /* K3 短按松手才报 4 */
            }
            if (prev == 1 && !k0_long && raw != 1)
            {
                Key_Event = 1;          /* K0 短按松手才报 1（切页） */
            }
        }
    }

    /* K0 长按 2 秒：报键码 10（SOS 呼救） */
    if (stable == 1 && !k0_long)
    {
        if (++k0_hold >= KEY_K0_LONG_TICKS)
        {
            k0_long = 1;
            Key_Event = 10;
        }
    }

    /* K3 长按 2 秒：报键码 9（低功耗） */
    if (stable == 4 && !k3_long)
    {
        if (++k3_hold >= KEY_K3_LONG_TICKS)
        {
            k3_long = 1;
            Key_Event = 9;
        }
    }

    /* K1/K2 长按连发 */
    if (stable == 2 || stable == 3)
    {
        if (hold_cnt < 255) hold_cnt++;

        if (hold_cnt == KEY_REPEAT_DELAY)       /* 按住 500ms，立即发一次 */
        {
            Key_Event = stable;
            rep_cnt = 0;
        }
        else if (hold_cnt > KEY_REPEAT_DELAY)   /* 之后每 100ms 一次 */
        {
            if (++rep_cnt >= KEY_REPEAT_PERIOD)
            {
                rep_cnt = 0;
                Key_Event = stable;
            }
        }
    }
    else
    {
        hold_cnt = 0;
        rep_cnt  = 0;
    }
}

/**
  * 函    数：取出一个按键事件（非阻塞）
  * 参    数：无
  * 返 回 值：0=没有新按键，1~4=键码
  */
uint8_t Key_GetNum(void)
{
    uint8_t k = Key_Event;

    if (k != 0)
        Key_Event = 0;
    return k;
}
