#ifndef __ZIGBEE_H
#define __ZIGBEE_H
#include "stm32f10x.h"                  // Device header
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

extern QueueHandle_t Zigbee_Queue;
extern SemaphoreHandle_t Zigbee_TX_Mutex;

void Zigbee_Init(void);
void Zigbee_SendByte(uint8_t data);
void Zigbee_SendString(char *str);
void Zigbee_SendLine(char *str);

#endif
