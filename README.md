# STM32F103 多传感器监测终端（练习项目）

基于 STM32F103C8T6 + 标准外设库（SPL）+ Keil MDK 的练习项目。

## 功能
- DS1302 实时时钟，4 界面 OLED 显示，按键切换
- 心率采集（TIM2 中断采样）
- 温度（DS18B20 单总线）
- 烟雾浓度（MQ2 + ADC）
- 闹钟：存 DS1302 内部 RAM，掉电不丢
- PWM 无源蜂鸣器报警

## 说明
本项目代码由 AI 辅助生成；作者负责需求定义、硬件设计与调试验证。

## 目录
- `CORE/` 内核与启动文件
- `STM32F10x_FWLib/` 标准外设库
- `HARDWARE/` 传感器与显示驱动
- `SYSTEM/` 延时、串口、定时器
- `USER/` 主程序与 Keil 工程（`STM32_CGMCU.uvprojx`）

## 编译
Keil MDK 打开 `USER/STM32_CGMCU.uvprojx`，按 F7 编译。
