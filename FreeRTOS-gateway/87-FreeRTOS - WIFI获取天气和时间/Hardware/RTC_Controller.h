#ifndef __RTC_CONTROLLER_H
#define __RTC_CONTROLLER_H

#include "stm32f10x.h"                  // Device header

void RTC_CTRL_Init(void);


void setAlarm(uint32_t alarmValue);

void RTC_InterruptInit(void);

void setCounter(int32_t counter);
#endif
