#ifndef __DS18B20_H
#define __DS18B20_H

#include "sys.h"

/* DS18B20 对外接口（移植时只需拷贝 ds18b20.c + ds18b20.h） */
u8    DS18B20_Init(void);        /* 0 = 正常, 1 = 未检测到传感器 */
short DS18B20_Get_Temp(void);    /* 单位 0.1°C, -1000 表示未检测到传感器 */

#endif
