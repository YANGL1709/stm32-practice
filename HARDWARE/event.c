/*
 * event.c -- 异常事件记录与 Flash 持久化
 *
 * RAM：32 条环形缓冲，供 OLED 翻页查看
 * Flash：最后一页 0x0800FC00（1KB），最多 102 条，每条 10 字节
 * 写入策略：仅在状态跳变时调用，同类型 60 秒内只记一次
 */

#include "stm32f10x.h"
#include <stdio.h>
#include "event.h"
#include "app.h"
#include "ds1302.h"
#include "uart_cmd.h"

/* ==================== 事件记录 Flash 持久化 ====================
 * 使用内部 Flash 最后一页（1KB）：0x0800FC00 ~ 0x0800FFFF
 * 页格式：[0..3] 魔数 "EVT1"   [4..] 每条记录 10 字节
 *   byte0: 类型  byte1: 保留(0xFF)  byte2-3: 数值(小端)
 *   byte4..9: 年(-2000) 月 日 时 分 秒
 */
#define FLASH_LOG_ADDR   0x0800FC00u
#define FLASH_LOG_MAGIC  0x31545645u        /* 'E','V','T','1'（小端） */
#define FLASH_LOG_MAX    102                /* (1024-4)/10 */

Event g_evt[EVT_MAX];
u8    g_evt_cnt = 0;
u8    g_evt_sel = 0;

static u8 g_flash_cnt = 0;

static void Flash_Wait(void)
{
    while (FLASH->SR & FLASH_SR_BSY);
}

static void Flash_Unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123u;
        FLASH->KEYR = 0xCDEF89ABu;
    }
}

static void Flash_Lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static void Flash_ErasePage(u32 addr)
{
    Flash_Wait();
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = addr;
    FLASH->CR |= FLASH_CR_STRT;
    Flash_Wait();
    FLASH->CR &= ~FLASH_CR_PER;
}

static void Flash_WriteHalf(u32 addr, u16 data)
{
    Flash_Wait();
    FLASH->CR |= FLASH_CR_PG;
    *(volatile u16 *)addr = data;
    Flash_Wait();
    FLASH->CR &= ~FLASH_CR_PG;
}

static void Flash_InitPage(void)
{
    Flash_ErasePage(FLASH_LOG_ADDR);
    Flash_WriteHalf(FLASH_LOG_ADDR,     0x5645);   /* 'E','V' */
    Flash_WriteHalf(FLASH_LOG_ADDR + 2, 0x3154);   /* 'T','1' */
    g_flash_cnt = 0;
}

static void FlashLog_Add(const Event *e)
{
    u32 addr;
    u16 hw[5];
    u8 i;

    Flash_Unlock();

    if (*(volatile u32 *)FLASH_LOG_ADDR != FLASH_LOG_MAGIC ||
        g_flash_cnt >= FLASH_LOG_MAX)
    {
        Flash_InitPage();                       /* 未初始化或页满：擦除重来 */
    }

    hw[0] = (u16)(e->type | 0xFF00u);           /* 高字节保留 0xFF */
    hw[1] = e->value;
    hw[2] = (u16)((u16)e->year | ((u16)e->month << 8));
    hw[3] = (u16)((u16)e->day  | ((u16)e->hour  << 8));
    hw[4] = (u16)((u16)e->minute | ((u16)e->second << 8));

    addr = FLASH_LOG_ADDR + 4 + (u32)g_flash_cnt * 10;
    for (i = 0; i < 5; i++)
        Flash_WriteHalf(addr + i * 2, hw[i]);

    g_flash_cnt++;

    Flash_Lock();
}

/* 上电时从 Flash 加载记录到 RAM（只取最后 EVT_MAX 条） */
void Event_Init(void)
{
    u8 total = 0;
    u8 i, start;

    g_flash_cnt = 0;
    g_evt_cnt = 0;
    g_evt_sel = 0;

    if (*(volatile u32 *)FLASH_LOG_ADDR != FLASH_LOG_MAGIC)
        return;                                 /* 页未初始化 */

    while (total < FLASH_LOG_MAX)
    {
        const u8 *r = (const u8 *)(FLASH_LOG_ADDR + 4 + (u32)total * 10);
        if (r[0] < 1 || r[0] > 4) break;
        total++;
    }
    g_flash_cnt = total;

    start = (u8)((total > EVT_MAX) ? (total - EVT_MAX) : 0);
    for (i = start; i < total; i++)
    {
        const u8 *r = (const u8 *)(FLASH_LOG_ADDR + 4 + (u32)i * 10);
        Event *e = &g_evt[g_evt_cnt++];

        e->type   = r[0];
        e->value  = (u16)((u16)r[2] | ((u16)r[3] << 8));
        e->year   = r[4];
        e->month  = r[5];
        e->day    = r[6];
        e->hour   = r[7];
        e->minute = r[8];
        e->second = r[9];
    }
    if (g_evt_cnt) g_evt_sel = (u8)(g_evt_cnt - 1);
}

/* 清空 Flash 中的记录 */
void Event_Clear(void)
{
    Flash_Unlock();
    Flash_ErasePage(FLASH_LOG_ADDR);
    g_flash_cnt = 0;
    Flash_Lock();
}

/* 添加一条异常事件（同类型 60 秒内只记一次），并同步上报串口 */
void Event_Add(u8 type, u16 value)
{
    Event *e;
    u32 now_s, last_s;

    now_s = (u32)TimeData.hour * 3600 + TimeData.minute * 60 + TimeData.second;

    if (g_evt_cnt > 0)                      /* 防刷屏：同类型 60 秒内只记一次 */
    {
        e = &g_evt[g_evt_cnt - 1];
        last_s = (u32)e->hour * 3600 + e->minute * 60 + e->second;
        if (e->type == type && e->day == TimeData.day &&
            now_s >= last_s && (now_s - last_s) < 60)
            return;
    }

    if (g_evt_cnt >= EVT_MAX)               /* 缓冲区满：整体前移，丢弃最旧一条 */
    {
        u8 i;
        for (i = 1; i < EVT_MAX; i++)
            g_evt[i - 1] = g_evt[i];
        g_evt_cnt = EVT_MAX - 1;
    }

    e = &g_evt[g_evt_cnt++];
    e->type   = type;
    e->value  = value;
    e->year   = (u8)(TimeData.year - 2000);
    e->month  = TimeData.month;
    e->day    = TimeData.day;
    e->hour   = TimeData.hour;
    e->minute = TimeData.minute;
    e->second = TimeData.second;

    g_evt_sel = (u8)(g_evt_cnt - 1);        /* 默认查看最新一条 */

    FlashLog_Add(e);                        /* 同步写入内部 Flash（掉电保留） */

    Uart_ReportEvent(type, value);          /* 串口同步上报 */
}
