/*
 * settings.c -- 时间/闹钟/阈值设置与 DS1302 持久化
 */

#include "stm32f10x.h"
#include "settings.h"
#include "app.h"
#include "ds1302.h"
#include "oled.h"

#define BCD(v)      ((u8)((((v) / 10) << 4) | ((v) % 10)))

u8 DaysInMonth(u16 y, u8 m)
{
    static const u8 dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (m < 1 || m > 12) return 31;
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) return 29;
    return dm[m - 1];
}

u8 CalcWeek(u16 year, u8 month, u8 day)
{
    int y = year;
    int m = month;
    int K, J, h;

    if (m < 3) { m += 12; y -= 1; }
    K = y % 100;
    J = y / 100;
    h = (day + (13 * (m + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
    return (u8)(((h + 5) % 7) + 1);
}

void EditAdd(s8 d)
{
    u8 dim;

    switch (g_field)
    {
        case 0:     /* 年 2000~2099 */
            EditTime.year = (u16)((int)EditTime.year + d);
            if (EditTime.year > 2099) EditTime.year = 2000;
            if (EditTime.year < 2000) EditTime.year = 2099;
            break;

        case 1:     /* 月 1~12 */
            if (d > 0) { EditTime.month++; if (EditTime.month > 12) EditTime.month = 1; }
            else       { if (EditTime.month <= 1) EditTime.month = 12; else EditTime.month--; }
            break;

        case 2:     /* 日 1~当月天数 */
            dim = DaysInMonth(EditTime.year, EditTime.month);
            if (d > 0) { EditTime.day++; if (EditTime.day > dim) EditTime.day = 1; }
            else       { if (EditTime.day <= 1) EditTime.day = dim; else EditTime.day--; }
            break;

        case 3:     /* 时 0~23 */
            if (d > 0) { EditTime.hour++; if (EditTime.hour > 23) EditTime.hour = 0; }
            else       { if (EditTime.hour == 0) EditTime.hour = 23; else EditTime.hour--; }
            break;

        case 4:     /* 分 0~59 */
            if (d > 0) { EditTime.minute++; if (EditTime.minute > 59) EditTime.minute = 0; }
            else       { if (EditTime.minute == 0) EditTime.minute = 59; else EditTime.minute--; }
            break;

        case 5:     /* 秒 0~59 */
            if (d > 0) { EditTime.second++; if (EditTime.second > 59) EditTime.second = 0; }
            else       { if (EditTime.second == 0) EditTime.second = 59; else EditTime.second--; }
            break;

        default: break;
    }

    dim = DaysInMonth(EditTime.year, EditTime.month);
    if (EditTime.day < 1)  EditTime.day = 1;
    if (EditTime.day > dim) EditTime.day = dim;
}

void EditAlarmAdd(s8 d)
{
    if (g_field == 0)
    {
        if (d > 0) { g_alarm_h++; if (g_alarm_h > 23) g_alarm_h = 0; }
        else       { if (g_alarm_h == 0) g_alarm_h = 23; else g_alarm_h--; }
    }
    else
    {
        if (d > 0) { g_alarm_m++; if (g_alarm_m > 59) g_alarm_m = 0; }
        else       { if (g_alarm_m == 0) g_alarm_m = 59; else g_alarm_m--; }
    }
}

void EditHrAdd(s8 d)
{
    int v;

    if (g_field == 0)                       /* 过高阈值：[过低+20, 200] */
    {
        v = (int)g_hr_high + d;
        if (v < (int)g_hr_low + 20) v = (int)g_hr_low + 20;
        if (v > 200) v = 200;
        g_hr_high = (u16)v;
    }
    else                                    /* 过低阈值：[40, 过高-20] */
    {
        v = (int)g_hr_low + d;
        if (v < 40) v = 40;
        if (v > (int)g_hr_high - 20) v = (int)g_hr_high - 20;
        g_hr_low = (u16)v;
    }
}

void EditTempAdd(s8 d)
{
    int v = (int)g_temp_high + d;
    if (v < 360) v = 360;
    if (v > 420) v = 420;
    g_temp_high = (u16)v;
}

void EditGasAdd(s8 d)
{
    int v = (int)g_gas_high + (int)d * 50;
    if (v < 500) v = 500;
    if (v > 4000) v = 4000;
    g_gas_high = (u16)v;
}

void SaveThresholds(void)
{
    DS1302_wirte_rig(0x8E, 0x00);               /* 关写保护 */
    DS1302_wirte_rig(0xD2, (u8)g_hr_high);
    DS1302_wirte_rig(0xD4, (u8)g_hr_low);
    DS1302_wirte_rig(0xD6, (u8)(g_temp_high & 0xFF));
    DS1302_wirte_rig(0xD8, (u8)(g_temp_high >> 8));
    DS1302_wirte_rig(0xDC, (u8)(g_gas_high & 0xFF));
    DS1302_wirte_rig(0xDE, (u8)(g_gas_high >> 8));
    DS1302_wirte_rig(0xDA, 0x5A);               /* 有效标记 */
    DS1302_wirte_rig(0x8E, 0x80);               /* 恢复写保护 */

    g_edit = EDIT_NONE;
    OLED_Clear();
}

void LoadThresholds(void)
{
    u8 ht, lt, tl, th;
    u16 t;

    if (DS1302_read_rig(0xDB) != 0x5A) return;  /* 未保存过，用默认值 */

    ht = DS1302_read_rig(0xD3);
    lt = DS1302_read_rig(0xD5);
    tl = DS1302_read_rig(0xD7);
    th = DS1302_read_rig(0xD9);

    if (ht >= 100 && ht <= 200 && lt >= 40 && lt <= 80 && ht >= lt + 20)
    {
        g_hr_high = ht;
        g_hr_low  = lt;
    }

    t = (u16)((u16)tl | ((u16)th << 8));
    if (t >= 360 && t <= 420) g_temp_high = t;

    tl = DS1302_read_rig(0xDD);
    th = DS1302_read_rig(0xDF);
    t = (u16)((u16)tl | ((u16)th << 8));
    if (t >= 500 && t <= 4000) g_gas_high = t;
}

void WriteRTC(struct TIMEData *t)
{
    t->week = CalcWeek(t->year, t->month, t->day);

    DS1302_wirte_rig(0x8E, 0x00);                       /* 关闭写保护 */
    DS1302_wirte_rig(0x80, BCD(t->second));
    DS1302_wirte_rig(0x82, BCD(t->minute));
    DS1302_wirte_rig(0x84, BCD(t->hour));
    DS1302_wirte_rig(0x86, BCD(t->day));
    DS1302_wirte_rig(0x88, BCD(t->month));
    DS1302_wirte_rig(0x8A, t->week);
    DS1302_wirte_rig(0x8C, BCD((u8)(t->year - 2000)));
    DS1302_wirte_rig(0x8E, 0x80);                       /* 恢复写保护 */

    DS1302_read_realTime();
}

void SaveTime(void)
{
    WriteRTC(&EditTime);

    g_edit = EDIT_NONE;
    OLED_Clear();
}

void SaveAlarm(void)
{
    DS1302_wirte_rig(0x8E, 0x00);
    DS1302_wirte_rig(0xCC, g_alarm_h);
    DS1302_wirte_rig(0xCE, g_alarm_m);
    DS1302_wirte_rig(0xD0, 0xA5);
    DS1302_wirte_rig(0x8E, 0x80);

    g_alarm_armed = 0;              /* 本分钟不再触发，避免设置后立刻响 */
    g_edit = EDIT_NONE;
    OLED_Clear();
}

void Alarm_Load(void)
{
    u8 h, m;

    if (DS1302_read_rig(0xD1) == 0xA5)
    {
        h = DS1302_read_rig(0xCD);
        m = DS1302_read_rig(0xCF);
        if (h <= 23 && m <= 59)
        {
            g_alarm_h = h;
            g_alarm_m = m;
        }
    }
}
