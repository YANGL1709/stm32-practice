/*
 * uart_cmd.c -- USART1 指令协议与事件上报
 *
 * 指令：TIME=... / DATA? / FAN=ON / FAN=OFF / LOG?
 * 9600 8N1，回车换行结束，大小写不敏感。
 */

#include "stm32f10x.h"
#include <stdio.h>
#include "uart_cmd.h"
#include "app.h"
#include "event.h"
#include "settings.h"
#include "HR.h"
#include "ds1302.h"
#include "delay.h"

/* 串口接收缓冲（中断写入，主循环解析） */
static char rxbuf[64];
static volatile u8 rxlen = 0;
static char cmdbuf[64];
static volatile u8 cmd_ready = 0;

static void uart_send_str(const char *s)
{
    while (*s)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, (u8)*s++);
    }
}

static void uart_burst_start(void)
{
    uart_send_str("        \r\n");      /* 8 个空格 + 换行 */
}


/* 开机通过串口打印功能菜单 */
void Uart_PrintMenu(void)
{
    uart_burst_start();
    uart_send_str(CN_SYS "\r\n");
    uart_send_str("----------------------------------------\r\n");
    uart_send_str("  DATA?                        " CN_QUERY CN_DATA "\r\n");
    uart_send_str("  LOG?                         " CN_VIEW CN_LOG "\r\n");
    uart_send_str("  FAN=ON                       " CN_ON CN_FAN "\r\n");
    uart_send_str("  FAN=OFF                      " CN_OFF CN_FAN "\r\n");
    uart_send_str("  TIME=2026-10-19 21:30:00     " CN_SET CN_TIME "\r\n");
    uart_send_str("----------------------------------------\r\n");
}

void USART1_IRQHandler(void)
{
    char c;
    u8 i;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) == RESET) return;

    c = (char)USART_ReceiveData(USART1);
    USART_ClearITPendingBit(USART1, USART_IT_RXNE);

    if (c == '\r' || c == '\n')
    {
        if (rxlen > 0 && !cmd_ready)
        {
            for (i = 0; i <= rxlen; i++)            /* 含结尾的 0 */
                cmdbuf[i] = rxbuf[i];
            cmd_ready = 1;
        }
        rxlen = 0;
    }
    else if (rxlen < sizeof(rxbuf) - 1)
    {
        rxbuf[rxlen++] = c;
        rxbuf[rxlen] = 0;
    }
}

void UartRx_Init(void)
{
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART1_IRQn, 2);
    NVIC_EnableIRQ(USART1_IRQn);
}

static char upc(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

static u8 cmd_is(const char *a, const char *b)      /* 整串不区分大小写比较 */
{
    while (*a && *b)
    {
        if (upc(*a) != upc(*b)) return 0;
        a++;
        b++;
    }
    return (*a == 0 && *b == 0);
}

static u8 cmd_head(const char *a, const char *b)    /* 前缀比较 */
{
    while (*b)
    {
        if (upc(*a) != upc(*b)) return 0;
        a++;
        b++;
    }
    return 1;
}

static u8 isdig(char c)
{
    return (c >= '0' && c <= '9');
}

static u8 ParseTimeCmd(const char *p, struct TIMEData *t)
{
    static const u8 pos[14] = {0,1,2,3,5,6,8,9,11,12,14,15,17,18};
    u8 i;

    for (i = 0; i < 14; i++)
        if (!isdig(p[pos[i]])) return 0;

    if (p[4] != '-' || p[7] != '-' || p[10] != ' ' || p[13] != ':' || p[16] != ':')
        return 0;

    t->year   = (u16)((p[0]-'0')*1000 + (p[1]-'0')*100 + (p[2]-'0')*10 + (p[3]-'0'));
    t->month  = (u8)((p[5]-'0')*10 + (p[6]-'0'));
    t->day    = (u8)((p[8]-'0')*10 + (p[9]-'0'));
    t->hour   = (u8)((p[11]-'0')*10 + (p[12]-'0'));
    t->minute = (u8)((p[14]-'0')*10 + (p[15]-'0'));
    t->second = (u8)((p[17]-'0')*10 + (p[18]-'0'));

    if (t->year < 2000 || t->year > 2099) return 0;
    if (t->month < 1 || t->month > 12)    return 0;
    if (t->day < 1 || t->day > 31)        return 0;
    if (t->hour > 23 || t->minute > 59 || t->second > 59) return 0;
    return 1;
}

static const char *EvtName(u8 type)
{
    switch (type)
    {
        case EVT_HR_HIGH: return CN_HR CN_HIGH;
        case EVT_HR_LOW:  return CN_HR CN_LOW;
        case EVT_TEMP:    return CN_TEMP CN_HIGH;
        case EVT_GAS:     return CN_SMOKE CN_HIGH;
        case EVT_SOS:     return CN_SOS;
        default:          return CN_ALARM;
    }
}

static void Uart_ProcessCommand(char *s)
{
    static char tx[160];
    char vs[12];
    u8 i;

    while (*s == ' ' || *s == '\t') s++;

    /* 收到指令后稍等再回复：给 Proteus COMPIM 留出收/发方向切换的时间 */
    delay_ms(100);
    uart_burst_start();

    if (cmd_head(s, "TIME="))
    {
        struct TIMEData t;

        if (ParseTimeCmd(s + 5, &t))
        {
            WriteRTC(&t);
            sprintf(tx, CN_TIME ":%04d-%02d-%02d %02d:%02d:%02d\r\n",
                    TimeData.year, TimeData.month, TimeData.day,
                    TimeData.hour, TimeData.minute, TimeData.second);
            uart_send_str(tx);
        }
        else
        {
            uart_send_str(CN_UNKNOWN ":TIME=2026-10-19 13:14:59\r\n");
        }
    }
    else if (cmd_is(s, "DATA?"))
    {
        if (HeartRate > 0) sprintf(vs, "%d", (int)HeartRate);
        else               sprintf(vs, "---");

        if (g_temp == -1000 || g_temp == 850)
            sprintf(tx, CN_DATA ":" CN_HR "=%s " CN_TEMP "=--.- " CN_SMOKE "=%d\r\n",
                    vs, (int)g_gas_value);
        else if (g_temp < 0)
            sprintf(tx, CN_DATA ":" CN_HR "=%s " CN_TEMP "=-%d.%d " CN_SMOKE "=%d\r\n",
                    vs, (-g_temp) / 10, (-g_temp) % 10, (int)g_gas_value);
        else
            sprintf(tx, CN_DATA ":" CN_HR "=%s " CN_TEMP "=%d.%d " CN_SMOKE "=%d\r\n",
                    vs, g_temp / 10, g_temp % 10, (int)g_gas_value);
        uart_send_str(tx);
    }
    else if (cmd_is(s, "FAN=ON"))
    {
        g_fan_mode = 1;
        uart_send_str(CN_FAN ":" CN_ON "\r\n");
    }
    else if (cmd_is(s, "FAN=OFF"))
    {
        g_fan_mode = 2;
        uart_send_str(CN_FAN ":" CN_OFF "\r\n");
    }
    else if (cmd_is(s, "LOG?"))
    {
        sprintf(tx, CN_LOG ":%d" CN_COUNT "\r\n", g_evt_cnt);
        uart_send_str(tx);

        for (i = 0; i < g_evt_cnt; i++)
        {
            Event *e = &g_evt[i];

            if (e->type == EVT_TEMP)
                sprintf(vs, "%d.%d", e->value / 10, e->value % 10);
            else if (e->type == EVT_HR_HIGH || e->type == EVT_HR_LOW || e->type == EVT_GAS)
                sprintf(vs, "%d", e->value);
            else
                vs[0] = 0;

            if (vs[0])
                sprintf(tx, "%d %s %s %04d-%02d-%02d %02d:%02d:%02d\r\n", i + 1,
                        EvtName(e->type), vs, 2000 + e->year, e->month, e->day,
                        e->hour, e->minute, e->second);
            else
                sprintf(tx, "%d %s %04d-%02d-%02d %02d:%02d:%02d\r\n", i + 1,
                        EvtName(e->type), 2000 + e->year, e->month, e->day,
                        e->hour, e->minute, e->second);
            uart_send_str(tx);
        }
    }
    else
    {
        uart_send_str(CN_UNKNOWN "\r\n");
    }
}

/* 主循环轮询：有完整指令就处理 */
void Uart_Poll(void)
{
    if (cmd_ready)
    {
        Uart_ProcessCommand(cmdbuf);
        cmd_ready = 0;
    }
}

/* 是否正在接收指令（主循环据此暂停耗时的温度转换） */
u8 Uart_IsBusy(void)
{
    return (u8)((rxlen != 0) || cmd_ready);
}

void Uart_ReportEvent(u8 type, u16 value)
{
    const char *name;
    static char tx[80];

    switch (type)
    {
        case EVT_HR_HIGH: name = CN_HR CN_HIGH;   break;
        case EVT_HR_LOW:  name = CN_HR CN_LOW;    break;
        case EVT_TEMP:    name = CN_TEMP CN_HIGH; break;
        case EVT_GAS:     name = CN_SMOKE CN_HIGH; break;
        case EVT_SOS:     name = CN_SOS;          break;
        default:          name = CN_ALARM;        break;
    }

    if (type == EVT_TEMP)
        sprintf(tx, "\xCA\xC2\xBC\xFE:%s %d.%d %04d-%02d-%02d %02d:%02d:%02d\r\n",
                name, value / 10, value % 10,
                TimeData.year, TimeData.month, TimeData.day,
                TimeData.hour, TimeData.minute, TimeData.second);
    else if (type == EVT_HR_HIGH || type == EVT_HR_LOW || type == EVT_GAS)
        sprintf(tx, "\xCA\xC2\xBC\xFE:%s %d %04d-%02d-%02d %02d:%02d:%02d\r\n",
                name, (int)value,
                TimeData.year, TimeData.month, TimeData.day,
                TimeData.hour, TimeData.minute, TimeData.second);
    else
        sprintf(tx, "\xCA\xC2\xBC\xFE:%s %04d-%02d-%02d %02d:%02d:%02d\r\n",
                name,
                TimeData.year, TimeData.month, TimeData.day,
                TimeData.hour, TimeData.minute, TimeData.second);

    uart_burst_start();
    uart_send_str(tx);
}
