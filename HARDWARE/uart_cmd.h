#ifndef __UART_CMD_H
#define __UART_CMD_H

#include "stm32f10x.h"

/* 串口中文标签：源文件是 UTF-8，而串口助手通常按 GBK 解码，
 * 所以这里用 GBK 字节转义，终端按 GBK/ANSI 显示即为正常中文。 */
#define CN_DATE     "\xC8\xD5\xC6\xDA"      /* 日期 */
#define CN_WEEK     "\xD0\xC7\xC6\xDA"      /* 星期 */
#define CN_TIME     "\xCA\xB1\xBC\xE4"      /* 时间 */
#define CN_HR       "\xD0\xC4\xC2\xCA"      /* 心率 */
#define CN_TEMP     "\xCE\xC2\xB6\xC8"      /* 温度 */
#define CN_ALARM    "\xC4\xD6\xD6\xD3"      /* 闹钟 */
#define CN_NORMAL   "\xD5\xFD\xB3\xA3"      /* 正常 */
#define CN_HIGH     "\xB9\xFD\xB8\xDF"      /* 过高 */
#define CN_LOW      "\xB9\xFD\xB5\xCD"      /* 过低 */
#define CN_SMOKE    "\xD1\xCC\xCE\xED"      /* 烟雾 */
#define CN_SOS      "\xBD\xF4\xBC\xB1\xBA\xF4\xBE\xC8"  /* 紧急呼救 */
#define CN_TEST     "\xD7\xD4\xBC\xEC"      /* 自检 */
#define CN_DATA     "\xCA\xFD\xBE\xDD"      /* 数据 */
#define CN_FAN      "\xB7\xE7\xC9\xC8"      /* 风扇 */
#define CN_ON       "\xBF\xAA"              /* 开 */
#define CN_OFF      "\xB9\xD8"              /* 关 */
#define CN_LOG      "\xBC\xC7\xC2\xBC"      /* 记录 */
#define CN_COUNT    "\xCC\xF5"              /* 条 */
#define CN_UNKNOWN  "\xCE\xB4\xD6\xAA"      /* 未知 */
#define CN_SYS      "\xBD\xA1\xBF\xB5\xBC\xE0\xBB\xA4\xCA\xD6\xB1\xED"  /* 健康监护手表 */
#define CN_QUERY    "\xB2\xE9\xD1\xAF"      /* 查询 */
#define CN_VIEW     "\xB2\xE9\xBF\xB4"      /* 查看 */
#define CN_SET      "\xC9\xE8\xD6\xC3"      /* 设置 */

void Uart_PrintMenu(void);
void UartRx_Init(void);
void Uart_Poll(void);
u8   Uart_IsBusy(void);
void Uart_ReportEvent(u8 type, u16 value);

#endif
