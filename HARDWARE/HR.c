 #include "stm32f10x.h"
 #include "stdio.h"
 #include "AD.h"
 
/* ==================== 心率检测参数 ==================== */
#define SAMPLE_HZ		50		/* 采样率 50Hz，即每 20ms 采一个点 */
#define REFRACTORY_N	13		/* 不应期 260ms，滤掉重搏波和抖动，心搏上限约 230BPM */
#define AVG_WINDOW		5		/* 滑动平均窗口，5点 = 100ms，抑制高频噪声 */
#define AMP_MIN			300		/* 最小峰峰值，低于它认为没有有效信号 */
#define BEAT_AVG_N		4		/* 心率取最近 4 个心搏间期的平均 */
#define INTERVAL_MAX	3000	/* 超过 3s 的心搏间期视为断信号，重新统计 */

/* 心率校准系数：Proteus 模拟信号源的时间基准实测比设定值慢 4%
   （VPULSE 和信号发生器都如此，换器件无效，是仿真环境的固有偏差）。
   乘上此系数可使显示值与信号源的标称心率一致。
   >>> 移植到真实硬件时，把分子改回 1（即不校准）。<<< */
#define HR_CALIB_NUM	104
#define HR_CALIB_DEN	100

/* ==================== 对外变量 ==================== */
volatile uint16_t ADValue;		/* 原始 AD 值，范围 0~4095 */
volatile uint16_t SigValue;		/* 滑动平均滤波后的信号，调试用 */
volatile uint16_t HeartRate;	/* 心率，单位 BPM，0 表示尚未测出 */

/* ==================== 检测内部状态 ==================== */
static uint16_t avgBuf[AVG_WINDOW];		/* 滑动平均环形缓冲 */
static uint8_t  avgIndex;
static uint32_t avgSum;

static uint16_t sigMax, sigMin;			/* 自适应信号上下限 */
static uint8_t  state;					/* 0=等上升穿越，1=等下降穿越 */
static uint8_t  refractory;				/* 不应期倒计时，单位：采样点数 */

static uint32_t sampleCount;			/* 采样点计数，同时作为时间戳 */
static uint8_t  beatValid;				/* 是否已记录过上一次心搏 */
static uint32_t lastBeatSample;

static uint16_t intervalBuf[BEAT_AVG_N];	/* 心搏间期环形缓冲，单位 ms */
static uint8_t  intervalIndex;
static uint8_t  intervalCount;

/**
  * 函    数：心率模块初始化
  * 说    明：TIM2 挂在 APB1 上，APB1 分频不为 1 时定时器时钟为 PCLK1 的 2 倍，
  *           即 36MHz * 2 = 72MHz。7200 分频得 10kHz，再 200 计数得 50Hz。
  *           TIM2 复位后默认就走内部时钟，无需额外配置时钟源。
  * 注意事项：调用前必须先 AD_Init()，否则中断里读到的是无效的 AD 值。
  */
void HR_Init(void)
{
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	/*时基单元配置*/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;		//72MHz / 7200 = 10kHz
	TIM_TimeBaseInitStructure.TIM_Period = 200 - 1;			//10kHz / 200 = 50Hz
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

	/*更新中断配置*/
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);					//先清标志，避免初始化后立刻进一次中断
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);				//使能更新中断

	/*NVIC配置*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/*启动定时器*/
	TIM_Cmd(TIM2, ENABLE);
}

/**
  * 函    数：心率检测核心，由 TIM2 中断每 20ms 调用一次
  * 说    明：采样 -> 滑动平均 -> 自适应双阈值迟滞检测 -> 心搏间期 -> 心率
  * 注意事项：仅本文件内部使用，不对外暴露
  */
static void HeartRate_Update(void)
{
	uint16_t range, thHigh, thLow, i;
	uint32_t interval, sum;

	/*1. 采样并做滑动平均滤波*/
	ADValue = AD_GetValue();
	avgSum -= avgBuf[avgIndex];
	avgBuf[avgIndex] = ADValue;
	avgSum += ADValue;
	avgIndex = (avgIndex + 1) % AVG_WINDOW;
	SigValue = avgSum / AVG_WINDOW;

	/*2. 跟踪信号上下限并随时间缓慢衰减，使阈值自适应信号幅度与偏置*/
	if (SigValue >= sigMax)		sigMax = SigValue;
	else if (sigMax > 0)		sigMax -= 4;

	if (SigValue <= sigMin)		sigMin = SigValue;
	else if (sigMin < 4095)		sigMin += 4;

	/*3. 峰峰值太小，说明没有有效信号，停止检测*/
	range = sigMax - sigMin;
	if (range < AMP_MIN)
	{
		state = 0;
		refractory = 0;
		HeartRate = 0;
		sampleCount++;
		return;
	}

	/*4. 在幅度 1/3 和 2/3 处设双阈值，形成迟滞，避免噪声反复触发*/
	thHigh = sigMin + range * 2 / 3;
	thLow  = sigMin + range / 3;

	if (refractory > 0) refractory--;

	/*5. 迟滞状态机：必须先跌破下限、再升破上限，才记作一次新心搏*/
	if (state == 0)
	{
		if (refractory == 0 && SigValue >= thHigh)
		{
			if (beatValid)						/*有上一次心搏才能算间期*/
			{
				interval = (sampleCount - lastBeatSample) * (1000 / SAMPLE_HZ);
				if (interval <= INTERVAL_MAX)
				{
					intervalBuf[intervalIndex] = (uint16_t)interval;
					intervalIndex = (intervalIndex + 1) % BEAT_AVG_N;
					if (intervalCount < BEAT_AVG_N) intervalCount++;

					/*心率 = 60s / 平均心搏间期，用多次平均抑制单次测量误差，再乘校准系数*/
					sum = 0;
					for (i = 0; i < intervalCount; i++) sum += intervalBuf[i];
					HeartRate = (uint16_t)(60000UL * intervalCount * HR_CALIB_NUM / (sum * HR_CALIB_DEN));
				}
				else								/*间期过长，说明信号断过，重新统计*/
				{
					intervalCount = 0;
					intervalIndex = 0;
					HeartRate = 0;
				}
			}
			lastBeatSample = sampleCount;
			beatValid = 1;
			state = 1;
			refractory = REFRACTORY_N;			/*进入不应期*/
		}
	}
	else
	{
		if (SigValue <= thLow) state = 0;		/*等信号回落到下限以下，才允许下一次触发*/
	}

	sampleCount++;
}

/**
  * 函    数：TIM2 中断服务函数
  * 说    明：在本文件中定义，覆盖启动文件里的弱定义符号。
  *           放在 HR.c 里是为了让定时器和检测算法同属一个模块，便于整体移植。
  */
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		HeartRate_Update();
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
