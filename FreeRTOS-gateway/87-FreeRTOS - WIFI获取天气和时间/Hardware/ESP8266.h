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

void CloseATEAndConnectWifi(void);

uint32_t SynchronizeTime(void);

uint8_t parseJSON(char *rxJSON,uint32_t timeOut);

uint8_t connectMQTT(uint32_t timeout);


void SynchronizeTianqi(void);
#endif
