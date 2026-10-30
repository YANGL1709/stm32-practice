/*
 * main.c -- 健康监护手表主程序
 *
 * 模块划分：
 *   pages.c      页面绘制
 *   settings.c   时间/闹钟/阈值设置与 DS1302 持久化
 *   event.c      异常事件记录与 Flash 持久化
 *   uart_cmd.c   串口指令协议与事件上报
 *   lowpower.c   低功耗与按键唤醒
 *   mq2.c        烟雾传感器
 *   beep.c       喇叭与风扇执行器
 */

#include "stm32f10x.h"
#include <stdio.h>
#include "beep.h"
#include "usart.h"
#include "delay.h"
#include "oled.h"
#include "ds1302.h"
#include "AD.h"
#include "HR.h"
#include "ds18b20.h"
#include "Key.h"
#include "mq2.h"
#include "app.h"
#include "event.h"
#include "settings.h"
#include "pages.h"
#include "uart_cmd.h"
#include "lowpower.h"

char  st[50];
short g_temp = -1000;

u8 g_page  = PAGE_TIME;
u8 g_edit  = EDIT_NONE;
u8 g_field = 0;
u8 g_blink = 1;
u8 g_blink_div = 0;

struct TIMEData EditTime;

/* 闹钟 */
u8 g_alarm_h    = 7;
u8 g_alarm_m    = 30;
u8 g_alarm_on   = 0;
u8 g_alarm_armed = 1;

/* 健康报警状态 */
u8 g_hr_alarm   = 0;
u8 g_temp_alarm = 0;
static u8 g_temp_div = 0;
static u8 g_beep_div = 0;
u8 g_fan_mode = 0;

/* 可调报警阈值（上电从 DS1302 RAM 恢复） */
u16 g_hr_high   = HR_HIGH_DEF;
u16 g_hr_low    = HR_LOW_DEF;
u16 g_temp_high = TEMP_HIGH_DEF;
u16 g_gas_high  = GAS_HIGH_DEF;

/* 烟雾/可燃气体 */
u16 g_gas_value = 0;
u8  g_gas_alarm = 0;
static u8 evt_prev_gas = 0;
u8  g_sos = 0;

static u8 evt_prev_hr = 0;
static u8 evt_prev_temp = 0;
static u8 evt_prev_alarm = 0;

int main(void)
{
    u8 k;

    /* 本工程的启动文件没有调用 SystemInit，必须在这里手动配置时钟，
     * 否则芯片一直跑在 HSI 8MHz：晶振不起振、串口波特率不对、
     * DS18B20 时序也不对。 */
    SystemInit();

    delay_init(72);
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    Fan_Init();                 /* 继电器/风扇驱动输出初始化（默认关闭） */

    USART1_Config();            /* 串口 9600，PA9=TX PA10=RX */
    UartRx_Init();              /* 使能串口接收中断（上位机指令） */

    AD_Init();
    MQ2_Init();                 /* MQ-2 烟雾传感器（PA0，与心率共用 ADC1） */
    HR_Init();

    DS1302_GPIO_Init();
    DS1302_Init();
    DS1302_read_realTime();
    Alarm_Load();
    LoadThresholds();       /* 从 DS1302 RAM 恢复报警阈值 */

    OLED_Init();
    OLED_Clear();

    DS18B20_Init();
    Key_Init();
    Key_EXTI_Init();        /* 低功耗唤醒用 */
    Beep_Init();
    Event_Init();           /* 从 Flash 恢复历史异常记录 */

    Uart_PrintMenu();       /* 开机打印串口功能菜单 */

    while (1)
    {
        Uart_Poll();                        /* 处理上位机指令 */

        k = Key_GetNum();

        if (k == 9)                     /* K3 长按 2 秒：进入低功耗 */
        {
            EnterStop();                /* 任意键唤醒 */
            k = 0;
        }

        if (k != 0 && g_sos)            /* SOS 呼救中：任意键取消 */
        {
            g_sos = 0;
            Beep_Off();
            OLED_Clear();
            k = 0;
        }
        else if (k == 10)               /* K0 长按 2 秒：触发 SOS 呼救 */
        {
            g_sos  = 1;
            g_edit = EDIT_NONE;
            OLED_Clear();
            Event_Add(EVT_SOS, 0);
            k = 0;
        }

        /* 闹钟响时，任意键只负责关闭蜂鸣器 */
        if (k != 0 && g_alarm_on)
        {
            g_alarm_on = 0;
            Beep_Off();
            k = 0;
        }

        if (g_edit == EDIT_TIME)
        {
            if (k == 1)                         /* K0：切换字段 */
            {
                g_field++;
                if (g_field >= 6) g_field = 0;
            }
            else if (k == 2) EditAdd(1);        /* K1：加 */
            else if (k == 3) EditAdd(-1);       /* K2：减 */
            else if (k == 4) SaveTime();        /* K3：保存并退出 */
        }
        else if (g_edit == EDIT_ALARM)
        {
            if (k == 1)                         /* K0：时/分切换 */
            {
                g_field ^= 1;
            }
            else if (k == 2) EditAlarmAdd(1);
            else if (k == 3) EditAlarmAdd(-1);
            else if (k == 4) SaveAlarm();
        }
        else if (g_edit == EDIT_HR)
        {
            if (k == 1)                         /* K0：过高/过低切换 */
            {
                g_field ^= 1;
            }
            else if (k == 2) EditHrAdd(1);
            else if (k == 3) EditHrAdd(-1);
            else if (k == 4) SaveThresholds();
        }
        else if (g_edit == EDIT_TEMP)
        {
            if (k == 2) EditTempAdd(1);
            else if (k == 3) EditTempAdd(-1);
            else if (k == 4) SaveThresholds();
        }
        else if (g_edit == EDIT_GAS)
        {
            if (k == 2) EditGasAdd(1);
            else if (k == 3) EditGasAdd(-1);
            else if (k == 4) SaveThresholds();
        }
        else
        {
            if (k == 1)                         /* K0：切换页面 */
            {
                g_page++;
                if (g_page >= PAGE_NUM) g_page = 0;
                OLED_Clear();
                if (g_page == PAGE_EVENTS && g_evt_cnt)
                    g_evt_sel = (u8)(g_evt_cnt - 1);
            }
            else if (k == 4)                    /* K3：进入设置 */
            {
                if (g_page == PAGE_TIME)
                {
                    g_edit  = EDIT_TIME;
                    g_field = 0;
                    g_blink = 1;
                    EditTime = TimeData;
                    OLED_Clear();
                }
                else if (g_page == PAGE_ALARM)
                {
                    g_edit  = EDIT_ALARM;
                    g_field = 0;
                    g_blink = 1;
                    OLED_Clear();
                }
                else if (g_page == PAGE_EVENTS)     /* K3：清空记录 */
                {
                    g_evt_cnt = 0;
                    g_evt_sel = 0;
                    Event_Clear();                  /* 同时清除 Flash 里的记录 */
                }
                else if (g_page == PAGE_HR)         /* K3：设置心率阈值 */
                {
                    g_edit  = EDIT_HR;
                    g_field = 0;
                    g_blink = 1;
                    OLED_Clear();
                }
                else if (g_page == PAGE_TEMP)       /* K3：设置温度阈值 */
                {
                    g_edit  = EDIT_TEMP;
                    g_field = 0;
                    g_blink = 1;
                    OLED_Clear();
                }
                else if (g_page == PAGE_GAS)        /* K3：设置烟雾阈值 */
                {
                    g_edit  = EDIT_GAS;
                    g_field = 0;
                    g_blink = 1;
                    OLED_Clear();
                }
            }
            else if (g_page == PAGE_EVENTS && (k == 2 || k == 3))   /* K1/K2：翻历史记录 */
            {
                if (k == 2) { if (g_evt_sel > 0) g_evt_sel--; }
                else        { if (g_evt_sel + 1 < g_evt_cnt) g_evt_sel++; }
            }
            else if (k == 5)                        /* K4：切换风扇模式 */
            {
                g_fan_mode++;
                if (g_fan_mode > 2) g_fan_mode = 0;
            }
        }

        if (g_edit == EDIT_NONE)
            DS1302_read_realTime();

        /* 闹钟判断（精确到分钟，每分钟只触发一次） */
        if (g_edit == EDIT_NONE)
        {
            if (g_alarm_armed && !g_alarm_on &&
                TimeData.hour == g_alarm_h && TimeData.minute == g_alarm_m)
            {
                g_alarm_on = 1;
                Beep_On();
                g_alarm_armed = 0;
            }
            if (TimeData.hour != g_alarm_h || TimeData.minute != g_alarm_m)
                g_alarm_armed = 1;              /* 离开该分钟，重新允许触发 */
        }

        /* 闹钟触发 → 记录事件 */
        if (g_alarm_on && !evt_prev_alarm) Event_Add(EVT_ALARM, 0);
        evt_prev_alarm = g_alarm_on;

        /* ---- 温度采样：温度页每轮读；其它页约 2 秒读一次（设置或收指令时暂停） ---- */
        if (g_edit == EDIT_NONE && !Uart_IsBusy())
        {
            if (g_page == PAGE_TEMP)
            {
                g_temp = DS18B20_Get_Temp();
            }
            else if (++g_temp_div >= 2)         /* 其它页约 2 轮读一次，加快报警响应 */
            {
                g_temp_div = 0;
                g_temp = DS18B20_Get_Temp();
            }
        }

        /* ---- 心率报警（高/低均报警，阈值可调，带回差防抖） ---- */
        if (HeartRate == 0)
        {
            g_hr_alarm = 0;                     /* 无有效信号不报警 */
        }
        else if (g_hr_alarm == 2)               /* 当前处于"过低"状态 */
        {
            if (HeartRate >= g_hr_low + HR_LOW_HYST)
                g_hr_alarm = (HeartRate > g_hr_high) ? 1 : 0;
        }
        else if (g_hr_alarm == 1)               /* 当前处于"过高"状态 */
        {
            if (HeartRate <= g_hr_high - HR_HYST)
                g_hr_alarm = (HeartRate < g_hr_low) ? 2 : 0;
        }
        else                                    /* 正常，检查是否越界 */
        {
            if (HeartRate > g_hr_high)     g_hr_alarm = 1;
            else if (HeartRate < g_hr_low) g_hr_alarm = 2;
        }

        /* 心率报警状态跳变 → 记录事件 */
        if (g_hr_alarm != evt_prev_hr)
        {
            if (g_hr_alarm == 1)      Event_Add(EVT_HR_HIGH, HeartRate);
            else if (g_hr_alarm == 2) Event_Add(EVT_HR_LOW, HeartRate);
            evt_prev_hr = g_hr_alarm;
        }

        /* ---- 温度报警执行器（风扇，带回差） ----
         * 850 = 85.0°C 是 DS18B20 上电默认值，不是真实测量值，忽略 */
        if (g_temp != -1000 && g_temp != 850)
        {
            if (g_temp >= g_temp_high)                   g_temp_alarm = 1;
            else if (g_temp <= g_temp_high - TEMP_HYST)  g_temp_alarm = 0;
        }
        if (g_gas_alarm)              Fan_On();       /* 烟雾报警：强制排风（安全优先） */
        else if (g_fan_mode == 1)     Fan_On();       /* 手动：强制开 */
        else if (g_fan_mode == 2)     Fan_Off();      /* 手动：强制关 */
        else if (g_temp_alarm)        Fan_On();       /* 自动：温度过高开风扇 */
        else                          Fan_Off();

        /* 温度报警状态跳变 → 记录事件 */
        if (g_temp_alarm != evt_prev_temp)
        {
            if (g_temp_alarm && g_temp != -1000 && g_temp != 850)
                Event_Add(EVT_TEMP, (u16)g_temp);
            evt_prev_temp = g_temp_alarm;
        }

        /* ---- 烟雾/可燃气体采样（ADC2）与报警（带回差） ---- */
        g_gas_value = MQ2_Read();
        if (g_gas_alarm == 0)
        {
            if (g_gas_value >= g_gas_high) g_gas_alarm = 1;
        }
        else
        {
            if (g_gas_value <= g_gas_high - GAS_HYST) g_gas_alarm = 0;
        }
        if (g_gas_alarm != evt_prev_gas)            /* 状态跳变 → 记录事件 */
        {
            if (g_gas_alarm) Event_Add(EVT_GAS, g_gas_value);
            evt_prev_gas = g_gas_alarm;
        }

        /* ---- 喇叭：间歇鸣叫（响 1 轮、停 3 轮）
         * 持续输出 PWM 会明显拖慢 Proteus 仿真，间歇输出更接近真实报警器 */
        if (g_alarm_on || g_hr_alarm || g_gas_alarm || g_sos)
        {
            g_beep_div = (u8)((g_beep_div + 1) & 0x03);
            if (g_beep_div == 0) Beep_On();
            else                 Beep_Off();
        }
        else
        {
            g_beep_div = 0;
            Beep_Off();
        }

        /* 显示 */
        if (g_sos)                      /* SOS 画面覆盖其它页面 */
        {
            ShowSosPage();
        }
        else if (g_edit == EDIT_TIME)
        {
            ShowEditPage();
        }
        else if (g_edit == EDIT_ALARM)
        {
            ShowAlarmPage();
        }
        else if (g_edit == EDIT_HR)
        {
            ShowHrEditPage();
        }
        else if (g_edit == EDIT_TEMP)
        {
            ShowTempEditPage();
        }
        else if (g_edit == EDIT_GAS)
        {
            ShowGasEditPage();
        }
        else
        {
            switch (g_page)
            {
                case PAGE_TIME:  ShowTimePage();  break;
                case PAGE_HR:    ShowHrPage();    break;
                case PAGE_TEMP:  ShowTempPage();  break;
                case PAGE_ALARM:  ShowAlarmPage();  break;
                case PAGE_EVENTS: ShowEventsPage(); break;
                case PAGE_GAS:    ShowGasPage();    break;
                default: break;
            }

            sprintf(st, "%d/%d", g_page + 1, PAGE_NUM);
            OLED_FastString(104, 48, st);
        }

        OLED_Refresh();

        /* 设置模式下主循环更快（100ms），便于响应长按连发；
         * 闪烁用分频保持约 200ms 变化一次 */
        g_blink_div++;
        if (g_blink_div >= 2)
        {
            g_blink_div = 0;
            g_blink = !g_blink;
        }

        delay_ms(g_edit ? 100 : 200);
    }
}
