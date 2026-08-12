#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "USART_Model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "RTC_Controller.h"
#include "ESP8266.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "BreathLED.h"
#include "dianji.h"
#include "LED.h"
TaskHandle_t BreathHandler;

TaskHandle_t connectWifiHandler;

TaskHandle_t MQTTHandler;

TaskHandle_t dianjiHandler;

TaskHandle_t LEDControlHandler;

void LEDControlTask(void* param){
	while(1){
		LED1_Toggle();
		vTaskDelay(2000);
	}
}

void connectWifiTask(void* param){
	 //=====连接WIFI=====
	CloseATEAndConnectWifi();
	vTaskDelete(NULL);
}

void MQTTTask(void* param){
	connectMQTT(5000);
	while(1)
    {
        // MQTT接收、发布等
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


void BreathLEDTask(void* param){
	
	uint8_t flag = 0;
	for(int i = 0;i<100;){
		if(i ==  99){
			flag = !flag;
			i = 0;
		}
		if(!flag){
			TIM_SetCompare1(TIM2,100*(i++));
			vTaskDelay(10);
		}
		if(flag){
			TIM_SetCompare1(TIM2,10000-100*(i++));
			vTaskDelay(10);
		}
	}
}


void dianjiTask(void* param){
	while(1){
	 vTaskDelay(pdMS_TO_TICKS(10));
	}	
}




int main()
{
    //=====初始化=====
	
    USART1_Init();
	BreathLEDInit();
	dianjiInit();
	LED_Init();
	
	xTaskCreate(BreathLEDTask,"BreathLED",128,NULL,1,&BreathHandler);
	
	xTaskCreate(connectWifiTask,"WIFI",256,NULL,2,&connectWifiHandler);
	
	xTaskCreate(MQTTTask,"MQTT",512,NULL,3,&MQTTHandler);
	
	xTaskCreate(dianjiTask,"dianji",128,NULL,1,&dianjiHandler);
   
	xTaskCreate(LEDControlTask,"LEDControl",128,NULL,2,&LEDControlHandler);
    
	// 启动调度器
    vTaskStartScheduler();
    
    while(1)
    {
        // 不会执行到这里
		
    }
}



