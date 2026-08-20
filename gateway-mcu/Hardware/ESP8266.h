#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"                  // Device header
#include "USART_Model.h"
#include "Delay.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//返回1，成功，返回0，失败
uint8_t SendAT(char *cmd , char *match , uint32_t timeOut);

//等待串口返回匹配字符串，但不清空接收缓冲区（用于 MQTTPUBRAW 发送数据后等待 OK）
uint8_t WaitAT(char *match , uint32_t timeOut);

uint8_t CloseATEAndConnectWifi(void);

uint32_t SynchronizeTime(void);

uint8_t parseJSON(char *rxJSON,uint32_t timeOut);

uint8_t connectMQTT(uint32_t timeout);


void SynchronizeTianqi(void);

/* 向 ESP8266 补发填充字节：当 AT+MQTTPUBRAW 已发出且模块已进入"等待 payload"
 * 状态但 STM32 侧未能完成发送时（如匹配超时/误判），模块会一直吞掉后续
 * 所有 AT 指令。补发 len 字节填充使其凑满长度退出等待态，避免 AT 状态机卡死。 */
void ESP_FlushMqttPayload(int len);

/* 软复位 ESP8266（AT+RST）：模块卡死/无响应时调用。
 * 若模块正卡在 MQTTPUBRAW 等待数据状态，先灌填充字节解卡再执行 RST。 */
void ESP_Restart(void);

/* 确保 WiFi 已连接（AT+CWJAP? 确认，未连则 AT+CWJAP 重连）。用于 ESP_Restart
 * 之后（软复位会断开 WiFi）。return：1，已连接；0，未连上 */
uint8_t ESP_EnsureWifi(void);

/* ============ ESP8266 硬件复位（RST 引脚接 PB14） ============
 * 注意：PB14 在 Key.c 里原为按键（Key2），但 main 从未调用 Key_Init，
 * 代码层面无冲突；硬件上不要同时在 PB14 接按键。 */
void ESP8266_RST_Init(void);       /* 上电初始化：拉低≥200ms 再释放，复位 ESP8266 */
void ESP8266_RST_HardReset(void);  /* 任务上下文硬件复位（软复位失败时兜底） */
#endif
