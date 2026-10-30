#ifndef __OLED_H
#define __OLED_H 
#include "sys.h"
#include "stdlib.h"	

//----------------OLED端口定义----------------- 

#define OLED_SCL_PROT  			GPIOB
#define OLED_SCL_PIN				GPIO_Pin_8
#define OLED_SCL_GPIO_CLK   RCC_APB2Periph_GPIOB
#define OLED_SDA_PROT  			GPIOB
#define OLED_SDA_PIN				GPIO_Pin_9
#define OLED_SDA_GPIO_CLK   RCC_APB2Periph_GPIOB
/*********************END**********************/

#define OLED_SCL_Clr() GPIO_ResetBits(OLED_SCL_PROT,OLED_SCL_PIN)//SCL
#define OLED_SCL_Set() GPIO_SetBits(OLED_SCL_PROT,OLED_SCL_PIN)

#define OLED_SDA_Clr() GPIO_ResetBits(OLED_SDA_PROT,OLED_SDA_PIN)//DIN
#define OLED_SDA_Set() GPIO_SetBits(OLED_SDA_PROT,OLED_SDA_PIN)

#define OLED_CMD  0	//写命令
#define OLED_DATA 1	//写数据

void OLED_ClearPoint(u8 x,u8 y);
void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);
void OLED_I2C_Start(void);
void OLED_I2C_Stop(void);
void OLED_I2C_WaitAck(void);
void OLED_Send_Byte(u8 dat);
void OLED_WR_Byte(u8 dat,u8 mode);
void OLED_DisPlay_On(void);
void OLED_DisPlay_Off(void);
void OLED_Refresh(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y,u8 t);
void OLED_DrawLine(u8 x1,u8 y1,u8 x2,u8 y2,u8 mode);
void OLED_DrawCircle(u8 x,u8 y,u8 r);
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 size1,u8 mode);
void OLED_ShowChar6x8(u8 x,u8 y,u8 chr,u8 mode);
void OLED_ShowString(u8 x,u8 y,u8 *chr,u8 size1,u8 mode);
void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size1,u8 mode);
void OLED_ShowChinese(u8 x,u8 y,u8 num,u8 size1,u8 mode);
void OLED_ScrollDisplay(u8 num,u8 space,u8 mode);
void OLED_ShowPicture(u8 x,u8 y,u8 sizex,u8 sizey,u8 BMP[],u8 mode);
void OLED_Init(void);

void OLED_FastChar(u8 x,u8 y,u8 ch);
void OLED_FastString(u8 x,u8 y,const char *s);

/* 画一个 16x16 汉字（字模见 oledfont.h 的 Hzk1） */
void OLED_ShowCN(u8 x, u8 y, u8 idx);

/* 汉字字模索引（Hzk1：0~11 为原有字，12 起为新增字） */
#define CN_SHI      12      /* 时 */
#define CN_JIAN     13      /* 间 */
#define CN_XIN      14      /* 心 */
#define CN_LV       15      /* 率 */
#define CN_TI       16      /* 体 */
#define CN_WEN      17      /* 温 */
#define CN_NAO      18      /* 闹 */
#define CN_ZHONG    19      /* 钟 */
#define CN_SHE      20      /* 设 */
#define CN_ZHI      21      /* 置 */
#define CN_ZHENG    22      /* 正 */
#define CN_CHANG    23      /* 常 */
#define CN_GUO      24      /* 过 */
#define CN_GAO      25      /* 高 */
#define CN_DI       26      /* 低 */
#define CN_FEN      27      /* 分 */
#define CN_MIAO     28      /* 秒 */
#define CN_CI       29      /* 次 */
#define CN_XUAN     30      /* 选 */
#define CN_ZE       31      /* 择 */
#define CN_BAO      32      /* 保 */
#define CN_CUN      33      /* 存 */
#define CN_QIE      34      /* 切 */
#define CN_HUAN     35      /* 换 */
#define CN_DU       36      /* 度 */
#define CN_YI       37      /* 异 */
#define CN_JI       38      /* 记 */
#define CN_LU       39      /* 录 */
#define CN_WU       40      /* 无 */
#define CN_ZI       41      /* 自 */
#define CN_DONG     42      /* 动 */
#define CN_SHOU     43      /* 手 */
#define CN_YAN      44      /* 烟 */
#define CN_WU2      45      /* 雾 */
#define CN_HUAN2    46      /* 环 */
#define CN_JING     47      /* 境 */
#define CN_JIN      48      /* 紧 */
#define CN_JI2      49      /* 急 */
#define CN_HU       50      /* 呼 */
#define CN_JIU      51      /* 救 */
#define CN_REN      52      /* 任 */
#define CN_YI2      53      /* 意 */
#define CN_JIAN2    54      /* 键 */
#define CN_QU       55      /* 取 */
#define CN_XIAO     56      /* 消 */
#define CN_JIAN3    57      /* 检 */
#define CN_JIA      58      /* 家 */
#define CN_JU       59      /* 居 */
#define CN_AN       60      /* 安 */
#define CN_QUAN     61      /* 全 */

#endif

