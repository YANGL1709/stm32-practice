/*
 * pages.c -- OLED 页面绘制
 */

#include "stm32f10x.h"
#include <stdio.h>
#include "pages.h"
#include "app.h"
#include "event.h"
#include "settings.h"
#include "oled.h"
#include "HR.h"
#include "ds1302.h"

static void draw_digits(u8 x, u8 y, u16 v, u8 n, u8 visible)
{
    u8 i;

    if (!visible)                       /* 闪烁的隐藏相位：用空格擦除 */
    {
        for (i = 0; i < n; i++) st[i] = ' ';
        st[n] = 0;
    }
    else if (n == 4)
    {
        sprintf(st, "%04d", v);
    }
    else if (n == 3)
    {
        sprintf(st, "%03d", v);
    }
    else
    {
        sprintf(st, "%02d", v);
    }
    OLED_FastString(x, y, st);
}

static void DrawTimeFields(struct TIMEData *t, u8 edit, u8 field, u8 show)
{
    u8 wk;

    wk = edit ? CalcWeek(t->year, t->month, t->day) : t->week;

    /* 年 月 日 */
    draw_digits(8, 0, t->year, 4, (u8)(!edit || field != 0 || show));
    OLED_ShowChinese(40, 0, 9, 16, 1);
    draw_digits(56, 0, t->month, 2, (u8)(!edit || field != 1 || show));
    OLED_ShowChinese(72, 0, 10, 16, 1);
    draw_digits(88, 0, t->day, 2, (u8)(!edit || field != 2 || show));
    OLED_ShowChinese(104, 0, 11, 16, 1);

    /* 星期 + 时:分:秒 */
    OLED_ShowChinese(0, 16, 0, 16, 1);
    OLED_ShowChinese(16, 16, 1, 16, 1);
    if (wk >= 1 && wk <= 7)
        OLED_ShowChinese(32, 16, (u8)(wk + 1), 16, 1);

    draw_digits(56, 16, t->hour, 2, (u8)(!edit || field != 3 || show));
    OLED_FastString(72, 16, "-");
    draw_digits(80, 16, t->minute, 2, (u8)(!edit || field != 4 || show));
    OLED_FastString(96, 16, "-");
    draw_digits(104, 16, t->second, 2, (u8)(!edit || field != 5 || show));
}

void ShowTimePage(void)
{
    DrawTimeFields(&TimeData, 0, 0, 1);

    OLED_ShowCN(0, 32, CN_SHI);
    OLED_ShowCN(16, 32, CN_JIAN);

    OLED_FastString(0, 48, "K3:");
    OLED_ShowCN(24, 48, CN_SHE);
    OLED_ShowCN(40, 48, CN_ZHI);
}

void ShowHrPage(void)
{
    OLED_ShowCN(0, 0, CN_XIN);
    OLED_ShowCN(16, 0, CN_LV);

    if (HeartRate > 0) sprintf(st, "%3d", (int)HeartRate);
    else               sprintf(st, "---");
    OLED_FastString(0, 16, st);
    OLED_ShowCN(32, 16, CN_CI);          /* 次 */
    OLED_FastString(48, 16, "/");
    OLED_ShowCN(56, 16, CN_FEN);         /* 分 */

    if (g_hr_alarm == 1)      { OLED_ShowCN(0, 32, CN_GUO); OLED_ShowCN(16, 32, CN_GAO); }
    else if (g_hr_alarm == 2) { OLED_ShowCN(0, 32, CN_GUO); OLED_ShowCN(16, 32, CN_DI); }
    else                      { OLED_ShowCN(0, 32, CN_ZHENG); OLED_ShowCN(16, 32, CN_CHANG); }

    OLED_FastString(0, 48, "K0:");
    OLED_ShowCN(24, 48, CN_QIE);
    OLED_ShowCN(40, 48, CN_HUAN);
}

void ShowTempPage(void)
{
    OLED_ShowCN(0, 0, CN_WEN);
    OLED_ShowCN(16, 0, CN_DU);

    /* g_temp 由主循环统一采样（温度页每轮，其它页约 2 轮一次）
     * 850 = 85.0°C 是 DS18B20 上电默认值，显示为 -- */
    if (g_temp == -1000 || g_temp == 850) sprintf(st, "--.-C   ");
    else if (g_temp < 0)      sprintf(st, "-%d.%dC  ", (-g_temp) / 10, (-g_temp) % 10);
    else                      sprintf(st, "%d.%dC   ", g_temp / 10, g_temp % 10);
    OLED_FastString(0, 16, st);

    OLED_ShowCN(0, 32, CN_WEN);
    OLED_ShowCN(16, 32, CN_DU);
    if (g_temp_alarm)
    {
        OLED_ShowCN(32, 32, CN_GUO);
        OLED_ShowCN(48, 32, CN_GAO);
    }
    else
    {
        OLED_ShowCN(32, 32, CN_ZHENG);
        OLED_ShowCN(48, 32, CN_CHANG);
    }

    OLED_FastString(0, 48, "K0:");
    OLED_ShowCN(24, 48, CN_QIE);
    OLED_ShowCN(40, 48, CN_HUAN);

    /* 风扇模式：自动 / 手动 */
    if (g_fan_mode == 0)
    {
        OLED_ShowCN(64, 48, CN_ZI);
        OLED_ShowCN(80, 48, CN_DONG);
    }
    else
    {
        OLED_ShowCN(64, 48, CN_SHOU);
        OLED_ShowCN(80, 48, CN_DONG);
    }
}

void ShowHrEditPage(void)
{
    OLED_ShowCN(0, 0, CN_XIN);
    OLED_ShowCN(16, 0, CN_LV);

    OLED_ShowCN(0, 16, CN_GUO);
    OLED_ShowCN(16, 16, CN_GAO);
    OLED_FastString(32, 16, ":");
    draw_digits(40, 16, g_hr_high, 3, (u8)(g_field != 0 || g_blink));

    OLED_ShowCN(0, 32, CN_GUO);
    OLED_ShowCN(16, 32, CN_DI);
    OLED_FastString(32, 32, ":");
    draw_digits(40, 32, g_hr_low, 3, (u8)(g_field != 1 || g_blink));

    OLED_FastString(0, 48, "K0:");
    OLED_ShowCN(24, 48, CN_QIE);
    OLED_ShowCN(40, 48, CN_HUAN);
    OLED_FastString(56, 48, " K3:");
    OLED_ShowCN(88, 48, CN_BAO);
    OLED_ShowCN(104, 48, CN_CUN);
}

void ShowTempEditPage(void)
{
    OLED_ShowCN(0, 0, CN_WEN);
    OLED_ShowCN(16, 0, CN_DU);

    OLED_ShowCN(0, 16, CN_WEN);
    OLED_ShowCN(16, 16, CN_DU);
    OLED_ShowCN(32, 16, CN_GUO);
    OLED_ShowCN(48, 16, CN_GAO);
    OLED_FastString(64, 16, ":");

    if (g_blink)
    {
        sprintf(st, "%d.%d", g_temp_high / 10, g_temp_high % 10);
        OLED_FastString(72, 16, st);
    }
    else
    {
        OLED_FastString(72, 16, "    ");
    }

    OLED_FastString(0, 48, "K3:");
    OLED_ShowCN(24, 48, CN_BAO);
    OLED_ShowCN(40, 48, CN_CUN);
}

void ShowGasPage(void)
{
    OLED_ShowCN(0, 0, CN_JIA);
    OLED_ShowCN(16, 0, CN_JU);
    OLED_ShowCN(32, 0, CN_AN);
    OLED_ShowCN(48, 0, CN_QUAN);

    OLED_ShowCN(0, 16, CN_YAN);
    OLED_ShowCN(16, 16, CN_WU2);
    OLED_FastString(32, 16, ":");
    sprintf(st, "%4d", (int)g_gas_value);
    OLED_FastString(40, 16, st);

    OLED_ShowCN(0, 32, CN_YAN);
    OLED_ShowCN(16, 32, CN_WU2);
    if (g_gas_alarm)
    {
        OLED_ShowCN(32, 32, CN_GUO);
        OLED_ShowCN(48, 32, CN_GAO);
    }
    else
    {
        OLED_ShowCN(32, 32, CN_ZHENG);
        OLED_ShowCN(48, 32, CN_CHANG);
    }

    OLED_FastString(0, 48, "K3:");
    OLED_ShowCN(24, 48, CN_SHE);
    OLED_ShowCN(40, 48, CN_ZHI);
}

void ShowGasEditPage(void)
{
    OLED_ShowCN(0, 0, CN_JIA);
    OLED_ShowCN(16, 0, CN_JU);
    OLED_ShowCN(32, 0, CN_AN);
    OLED_ShowCN(48, 0, CN_QUAN);

    OLED_ShowCN(0, 16, CN_YAN);
    OLED_ShowCN(16, 16, CN_WU2);
    OLED_FastString(32, 16, ":");

    if (g_blink)
    {
        sprintf(st, "%4d", (int)g_gas_high);
        OLED_FastString(40, 16, st);
    }
    else
    {
        OLED_FastString(40, 16, "    ");
    }

    OLED_FastString(0, 48, "K3:");
    OLED_ShowCN(24, 48, CN_BAO);
    OLED_ShowCN(40, 48, CN_CUN);
}

void ShowAlarmPage(void)
{
    u8 vis_h = (u8)(g_edit != EDIT_ALARM || g_field != 0 || g_blink);
    u8 vis_m = (u8)(g_edit != EDIT_ALARM || g_field != 1 || g_blink);

    OLED_ShowCN(0, 0, CN_NAO);
    OLED_ShowCN(16, 0, CN_ZHONG);

    OLED_ShowCN(0, 16, CN_NAO);
    OLED_ShowCN(16, 16, CN_ZHONG);
    OLED_FastString(32, 16, ":");
    draw_digits(40, 16, g_alarm_h, 2, vis_h);
    OLED_FastString(56, 16, ":");
    draw_digits(64, 16, g_alarm_m, 2, vis_m);

    if (g_edit == EDIT_ALARM)
    {
        OLED_ShowCN(0, 32, CN_SHE);
        OLED_ShowCN(16, 32, CN_ZHI);
        OLED_FastString(32, 32, ":");
        OLED_ShowCN(40, 32, (g_field == 0) ? CN_SHI : CN_FEN);

        OLED_FastString(0, 48, "K0:");
        OLED_ShowCN(24, 48, CN_XUAN);
        OLED_ShowCN(40, 48, CN_ZE);
        OLED_FastString(56, 48, " K3:");
        OLED_ShowCN(88, 48, CN_BAO);
        OLED_ShowCN(104, 48, CN_CUN);
    }
    else
    {
        OLED_ShowCN(0, 32, CN_NAO);
        OLED_ShowCN(16, 32, CN_ZHONG);
        OLED_ShowCN(32, 32, CN_SHI);
        OLED_ShowCN(48, 32, CN_JIAN);

        OLED_FastString(0, 48, "K3:");
        OLED_ShowCN(24, 48, CN_SHE);
        OLED_ShowCN(40, 48, CN_ZHI);
    }
}

void ShowEventsPage(void)
{
    Event *e;

    /* 先清掉各行残留，避免上一条/下一条内容重叠 */
    OLED_FastString(88, 0, "     ");                    /* 计数区 */
    OLED_FastString(0, 16, "                ");         /* 日期行 */
    OLED_FastString(0, 32, "                ");         /* 时间行 */
    OLED_FastString(0, 48, "                ");         /* 类型/数值行 */

    OLED_ShowCN(0, 0, CN_YI);
    OLED_ShowCN(16, 0, CN_CHANG);
    OLED_ShowCN(32, 0, CN_JI);
    OLED_ShowCN(48, 0, CN_LU);

    if (g_evt_cnt == 0)
    {
        OLED_ShowCN(0, 16, CN_WU);
        OLED_ShowCN(16, 16, CN_JI);
        OLED_ShowCN(32, 16, CN_LU);
        OLED_FastString(0, 48, "K0:NEXT");
        return;
    }

    if (g_evt_sel >= g_evt_cnt) g_evt_sel = (u8)(g_evt_cnt - 1);
    e = &g_evt[g_evt_sel];

    sprintf(st, "%d/%d", g_evt_sel + 1, g_evt_cnt);
    OLED_FastString(88, 0, st);

    sprintf(st, "%04d-%02d-%02d", 2000 + e->year, e->month, e->day);
    OLED_FastString(0, 16, st);

    sprintf(st, "%02d:%02d:%02d", e->hour, e->minute, e->second);
    OLED_FastString(0, 32, st);

    /* 事件类型 */
    if (e->type == EVT_HR_HIGH)
    {
        OLED_ShowCN(0, 48, CN_XIN); OLED_ShowCN(16, 48, CN_LV);
        OLED_ShowCN(32, 48, CN_GUO); OLED_ShowCN(48, 48, CN_GAO);
    }
    else if (e->type == EVT_HR_LOW)
    {
        OLED_ShowCN(0, 48, CN_XIN); OLED_ShowCN(16, 48, CN_LV);
        OLED_ShowCN(32, 48, CN_GUO); OLED_ShowCN(48, 48, CN_DI);
    }
    else if (e->type == EVT_TEMP)
    {
        OLED_ShowCN(0, 48, CN_WEN); OLED_ShowCN(16, 48, CN_DU);
        OLED_ShowCN(32, 48, CN_GUO); OLED_ShowCN(48, 48, CN_GAO);
    }
    else if (e->type == EVT_GAS)
    {
        OLED_ShowCN(0, 48, CN_YAN); OLED_ShowCN(16, 48, CN_WU2);
        OLED_ShowCN(32, 48, CN_GUO); OLED_ShowCN(48, 48, CN_GAO);
    }
    else if (e->type == EVT_SOS)
    {
        OLED_ShowCN(0, 48, CN_JIN); OLED_ShowCN(16, 48, CN_JI2);
        OLED_ShowCN(32, 48, CN_HU); OLED_ShowCN(48, 48, CN_JIU);
    }
    else
    {
        OLED_ShowCN(0, 48, CN_NAO); OLED_ShowCN(16, 48, CN_ZHONG);
    }

    /* 事件数值 */
    st[0] = 0;
    if (e->type == EVT_TEMP)
        sprintf(st, "%d.%d", e->value / 10, e->value % 10);
    else if (e->type == EVT_HR_HIGH || e->type == EVT_HR_LOW || e->type == EVT_GAS)
        sprintf(st, "%d", e->value);
    if (st[0]) OLED_FastString(72, 48, st);
}

void ShowSosPage(void)
{
    if (g_blink)
    {
        OLED_ShowCN(32, 0, CN_JIN);
        OLED_ShowCN(48, 0, CN_JI2);
        OLED_ShowCN(64, 0, CN_HU);
        OLED_ShowCN(80, 0, CN_JIU);
    }
    else
    {
        OLED_FastString(0, 0, "                ");   /* 闪烁 */
    }

    OLED_FastString(48, 16, "SOS!");

    OLED_ShowCN(24, 32, CN_REN);
    OLED_ShowCN(40, 32, CN_YI2);
    OLED_ShowCN(56, 32, CN_JIAN2);
    OLED_ShowCN(72, 32, CN_QU);
    OLED_ShowCN(88, 32, CN_XIAO);
}

void ShowEditPage(void)
{
    u8 idx;

    DrawTimeFields(&EditTime, 1, g_field, g_blink);

    switch (g_field)
    {
        case 0:  idx = 9;       break;   /* 年 */
        case 1:  idx = 10;      break;   /* 月 */
        case 2:  idx = 11;      break;   /* 日 */
        case 3:  idx = CN_SHI;  break;   /* 时 */
        case 4:  idx = CN_FEN;  break;   /* 分 */
        default: idx = CN_MIAO; break;   /* 秒 */
    }

    OLED_ShowCN(0, 32, CN_SHE);
    OLED_ShowCN(16, 32, CN_ZHI);
    OLED_FastString(32, 32, ":");
    OLED_ShowCN(40, 32, idx);

    OLED_FastString(0, 48, "K0:");
    OLED_ShowCN(24, 48, CN_XUAN);
    OLED_ShowCN(40, 48, CN_ZE);
    OLED_FastString(56, 48, " K3:");
    OLED_ShowCN(88, 48, CN_BAO);
    OLED_ShowCN(104, 48, CN_CUN);
}
