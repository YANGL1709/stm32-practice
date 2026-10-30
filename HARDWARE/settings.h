#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "stm32f10x.h"
#include "ds1302.h"

u8 DaysInMonth(u16 y, u8 m);
u8 CalcWeek(u16 year, u8 month, u8 day);

void EditAdd(s8 d);
void EditAlarmAdd(s8 d);
void EditHrAdd(s8 d);
void EditTempAdd(s8 d);
void EditGasAdd(s8 d);

void SaveThresholds(void);
void LoadThresholds(void);
void WriteRTC(struct TIMEData *t);
void SaveTime(void);
void SaveAlarm(void);
void Alarm_Load(void);

#endif
