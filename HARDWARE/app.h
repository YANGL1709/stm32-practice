#ifndef __APP_H
#define __APP_H

#include "stm32f10x.h"
#include "ds1302.h"

/* ==================== 页面 ==================== */
#define PAGE_TIME   0
#define PAGE_ALARM  1
#define PAGE_HR     2
#define PAGE_TEMP   3
#define PAGE_GAS    4
#define PAGE_EVENTS 5
#define PAGE_NUM    6

/* ==================== 设置模式 ==================== */
#define EDIT_NONE   0
#define EDIT_TIME   1
#define EDIT_ALARM  2
#define EDIT_HR     3
#define EDIT_TEMP   4
#define EDIT_GAS    5

/* ==================== 健康报警阈值 ==================== */
#define HR_HIGH_DEF    170
#define HR_LOW_DEF      60
#define HR_HYST         10
#define HR_LOW_HYST      5
#define TEMP_HIGH_DEF  373
#define TEMP_HYST        3
#define GAS_HIGH_DEF  2000
#define GAS_HYST       100

#define USE_STOP_MODE   0       /* 1 = 真 Stop（硬件）；0 = 软件低功耗（Proteus） */

/* ==================== 应用共享状态（定义在 main.c） ==================== */
extern char  st[50];
extern short g_temp;

extern u8 g_page;
extern u8 g_edit;
extern u8 g_field;
extern u8 g_blink;
extern u8 g_blink_div;
extern struct TIMEData EditTime;

extern u8 g_alarm_h;
extern u8 g_alarm_m;
extern u8 g_alarm_on;
extern u8 g_alarm_armed;

extern u8 g_hr_alarm;
extern u8 g_temp_alarm;
extern u8 g_fan_mode;

extern u16 g_hr_high;
extern u16 g_hr_low;
extern u16 g_temp_high;
extern u16 g_gas_high;

extern u16 g_gas_value;
extern u8  g_gas_alarm;
extern u8  g_sos;

#endif
