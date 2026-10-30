#ifndef __EVENT_H
#define __EVENT_H

#include "stm32f10x.h"

#define EVT_MAX     32

#define EVT_HR_HIGH 1
#define EVT_HR_LOW  2
#define EVT_TEMP    3
#define EVT_ALARM   4
#define EVT_GAS     5
#define EVT_SOS     6

typedef struct
{
    u8  type;
    u16 value;
    u8  year;
    u8  month, day, hour, minute, second;
} Event;

extern Event g_evt[EVT_MAX];
extern u8    g_evt_cnt;
extern u8    g_evt_sel;

void Event_Init(void);
void Event_Add(u8 type, u16 value);
void Event_Clear(void);

#endif
