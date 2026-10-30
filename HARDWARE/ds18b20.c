/*
 * ds18b20.c -- DS18B20 单总线驱动（自包含，方便移植）
 *
 * 移植方法：
 *   1. 把 ds18b20.c 和 ds18b20.h 拷到新工程并加入编译；
 *   2. 修改下面“移植配置”里的 5 个宏（端口 / 引脚 / 引脚序号 / 时钟 / 延时倍率）；
 *   3. 本驱动依赖 delay.c 的 delay_us() 和 delay_ms()。
 *
 * 真实硬件 / Proteus 仿真：
 *   DQ_SCALE = 1  -> 真实硬件
 *   DQ_SCALE = 8  -> Proteus 仿真（该模型的 SysTick 按 HCLK 计数，
 *                    delay_us 实际只有标称值的 1/8，需要放大 8 倍补偿）
 *
 * Proteus 仿真注意事项（代码里已处理）：
 *   - 复位低电平用 5ms（该模型对短复位不产生应答）
 *   - 时隙的高电平阶段用“释放”（切输入上拉），不推挽拉高，
 *     否则会和传感器输出的数据位冲突
 *   - 方向切换直接写 GPIOx->CRL/CRH，不用 GPIO_Init（仿真里要几十微秒，
 *     会错过 15µs 的数据有效窗口）
 *   - 仿真里不要接 4.7k 外部上拉电阻（模型无法把总线拉低）
 */

#include "ds18b20.h"
#include "delay.h"

/* ==================== 移植配置 ==================== */
#define DQ_PORT          GPIOA
#define DQ_PIN           GPIO_Pin_4
#define DQ_PIN_INDEX     4                      /* 引脚号 0~15，用于 CRL/CRH */
#define DQ_CLK           RCC_APB2Periph_GPIOA

#define DQ_SCALE         8                      /* 仿真=8，真实硬件=1 */

#define RST_LOW_MS       5                      /* 复位低电平（ms） */
#define CONV_MS          750                    /* 12 位转换时间（ms） */
/* ================================================= */

/* 微秒级延时（按倍率补偿） */
#define DLY_US(us)       delay_us((u32)(us) * DQ_SCALE)

/* 拉低 / 置高（置高同时把 ODR 置 1，为后面切输入上拉做准备） */
#define DQ_LOW()         (DQ_PORT->BRR  = DQ_PIN)
#define DQ_HIGH()        (DQ_PORT->BSRR = DQ_PIN)

/* ---------------- 内部函数 ---------------- */

/* 写 CRL/CRH 配置引脚模式：0x3=推挽输出50MHz，0x8=上拉/下拉输入 */
static void DQ_Config(u32 mode)
{
    volatile u32 *cr;
    u32 shift;

    if (DQ_PIN_INDEX < 8)
    {
        cr    = &DQ_PORT->CRL;
        shift = DQ_PIN_INDEX * 4;
    }
    else
    {
        cr    = &DQ_PORT->CRH;
        shift = ((u32)DQ_PIN_INDEX - 8u) * 4u;
    }
    *cr = (*cr & ~(0xFu << shift)) | (mode << shift);
}

/* 推挽输出模式 */
static void DQ_ModeOut(void)
{
    DQ_Config(0x3u);
}

/* 输入上拉模式（ODR=1 选择上拉，和 GPIO_Init 行为一致） */
static void DQ_ModeIn(void)
{
    DQ_PORT->BSRR = DQ_PIN;
    DQ_Config(0x8u);
}

/* 读总线电平 */
static u8 DQ_Read(void)
{
    return (DQ_PORT->IDR & DQ_PIN) ? 1 : 0;
}

/* 复位：拉低 RST_LOW_MS 后释放总线 */
static void DQ_Reset(void)
{
    DQ_ModeOut();
    DQ_LOW();
    delay_ms(RST_LOW_MS);
    DQ_ModeIn();
}

/* 等待应答脉冲：返回 0=检测到传感器，1=无应答 */
static u8 DQ_Presence(void)
{
    u32 i = 0;

    while (!DQ_Read() && i < 2000) i++;     /* 等待总线释放为高 */
    i = 0;
    while (DQ_Read() && i < 2000) i++;      /* 等待传感器拉低（应答） */
    if (i >= 2000) return 1;

    i = 0;
    while (!DQ_Read() && i < 2000) i++;     /* 应答脉冲结束 */
    return 0;
}

/* 写一个字节（低位在前） */
static void DQ_WriteByte(u8 dat)
{
    u8 j, b;

    for (j = 0; j < 8; j++)
    {
        b = dat & 1;
        dat >>= 1;

        DQ_ModeOut();
        DQ_LOW();
        if (b)
        {
            DLY_US(2);
            DQ_ModeIn();                    /* 释放总线 -> 高 */
            DLY_US(60);
        }
        else
        {
            DLY_US(60);
            DQ_ModeIn();
            DLY_US(2);
        }
    }
}

/* 读一个位：时隙内采样两次，任一次为低即判 0 */
static u8 DQ_ReadBit(void)
{
    u8 v = 1;

    DQ_ModeOut();
    DQ_LOW();
    DLY_US(2);
    DQ_ModeIn();                            /* 释放，让传感器驱动数据 */

    DLY_US(8);  if (!DQ_Read()) v = 0;
    DLY_US(4);  if (!DQ_Read()) v = 0;
    DLY_US(46);                             /* 补满约 60µs 时隙 */
    return v;
}

/* 读一个字节（低位在前） */
static u8 DQ_ReadByte(void)
{
    u8 i, dat = 0;

    for (i = 0; i < 8; i++)
        dat |= (u8)(DQ_ReadBit() << i);
    return dat;
}

/* ---------------- 对外接口 ---------------- */

/* 初始化：配置引脚 + 检测传感器。返回 0=正常，1=未检测到 */
u8 DS18B20_Init(void)
{
    RCC_APB2PeriphClockCmd(DQ_CLK, ENABLE);

    DQ_ModeOut();
    DQ_HIGH();
    DQ_Reset();

    return DQ_Presence();
}

/* 进入/退出 1-Wire 临界区：
 * 只屏蔽会周期性打断位时序的 TIM2（心率）/TIM3（按键扫描）。
 * 不能关全局中断：否则串口指令到达时 USART1 接收中断被屏蔽，
 * 指令字节会丢失，上位机就会收到"未知指令"。
 * 代价：极少数情况下串口中断可能落在读位采样窗口里，
 * 造成单次温度读数异常（下一轮自动恢复），可接受。 */
static void DQ_CritEnter(void)
{
    NVIC_DisableIRQ(TIM2_IRQn);
    NVIC_DisableIRQ(TIM3_IRQn);
}

static void DQ_CritExit(void)
{
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM3_IRQn);
}

/* 读取温度：单位 0.1°C，返回 -1000 表示未检测到传感器
 *
 * 注意：本工程有 TIM2（心率 50Hz）和 TIM3（按键 100Hz）中断，
 * 1-Wire 的位时隙只有几十微秒，一旦被中断打断，读出值就会乱跳。
 * 因此位级通信放在临界区里；750ms 转换等待期间恢复正常中断，
 * 心率采样不受影响。
 */
short DS18B20_Get_Temp(void)
{
    u8 tl, th;
    s16 raw;

    /* 1. 启动一次温度转换（临界区） */
    DQ_CritEnter();

    DQ_Reset();
    if (DQ_Presence())
    {
        DQ_CritExit();
        return -1000;
    }
    DQ_WriteByte(0xCC);                     /* SKIP ROM */
    DQ_WriteByte(0x44);                     /* CONVERT T */

    DQ_CritExit();

    /* 2. 等待转换完成（此期间允许中断） */
    delay_ms(CONV_MS);

    /* 3. 读暂存器（临界区） */
    DQ_CritEnter();

    DQ_Reset();
    if (DQ_Presence())
    {
        DQ_CritExit();
        return -1000;
    }
    DQ_WriteByte(0xCC);                     /* SKIP ROM */
    DQ_WriteByte(0xBE);                     /* READ SCRATCHPAD */

    tl = DQ_ReadByte();                     /* 低字节 */
    th = DQ_ReadByte();                     /* 高字节 */

    DQ_CritExit();

    raw = (s16)(((u16)th << 8) | tl);       /* 16 位补码，1 LSB = 1/16°C */
    return (short)(((int)raw * 10) / 16);   /* 换算成 0.1°C */
}
