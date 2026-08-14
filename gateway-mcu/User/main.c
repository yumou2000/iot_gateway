#include "stm32f10x.h"                  // Device header
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
#include "Buzzer.h"
#include "TempSensor.h"
#include "RedSensor.h"
#include "AHT30.h"
#include "queue.h"
#include "semphr.h"
#include "ParseCmd.h"
#include "AHT30.h"
#include "RTC_Controller.h"
#include "ADC.h"
#include "LED.h"
#include "ZigBee.h"


uint8_t connectstate = 0;
uint8_t mqttState = 0;
QueueHandle_t Cmd_Queue;

SemaphoreHandle_t MQTT_Mutex;
TaskHandle_t BreathHandler;

TaskHandle_t connectWifiHandler;

TaskHandle_t MQTTHandler;

TaskHandle_t dianjiHandler;

TaskHandle_t LEDControlHandler;

TaskHandle_t HardwareHandler;

TaskHandle_t collectHandler;

QueueHandle_t Result_Queue;

TaskHandle_t MQTTReturnHandler;

TaskHandle_t ZigBeeHandler;

void connectWifiTask(void *param)
{
    uint8_t ok = CloseATEAndConnectWifi();

    if(ok)
    {
        uint32_t t = 0;

        // 尝试同步时间
        for(int i = 0; i < 3; i++)
        {
            t = SynchronizeTime();

            if(t > 0)
            {
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // 只有拿到有效时间，才设置RTC
        if(t > 0)
        {
            setCounter((int32_t)t);
        }
    }

    connectstate = ok;
    vTaskDelete(NULL);
}
void MQTTTask(void *param)
{
    uint8_t flag = 0;
	
    while(1)
    {
		if(connectstate == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
		
		
        if(flag == 0)
        {
            flag = connectMQTT(5000);

            if(flag)
            {
				mqttState = 1;
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(3000));
            }
        }
        else
        {
            Cmd_t cmd;

            cmd = ParseCmd(message);

            if(cmd.valid != 0)
            {
                if(xQueueSend(Cmd_Queue, &cmd, 0) == pdPASS)
                {
					memset(message, 0, BUFFER_SIZE);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
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

void HardwareTask(void *param)
{
    Cmd_t cmd;
	Result_t result;
    while(1)
    {
        if(xQueueReceive(Cmd_Queue, &cmd, portMAX_DELAY) == pdPASS)
        {
           if(cmd.led_on == 1){
			   TIM_SetCompare1(TIM3,cmd.led_br*100);

		   }else{
				 TIM_SetCompare1(TIM3,0);
		   }
		   if(cmd.motor_on == 1){
			   if(cmd.motor_dir == 1){
							
				GPIO_SetBits(GPIOA,GPIO_Pin_11);
				GPIO_ResetBits(GPIOA,GPIO_Pin_12);
				TIM_SetCompare2(TIM2,cmd.motor_sp*100);
			   }else{
			    GPIO_SetBits(GPIOA,GPIO_Pin_12);
				GPIO_ResetBits(GPIOA,GPIO_Pin_11);
				TIM_SetCompare2(TIM2,cmd.motor_sp*100);
			   }
		   }else{
				TIM_SetCompare2(TIM2,0);
		   }
		   if(cmd.buzzer == 1){
				GPIO_ResetBits(GPIOA,GPIO_Pin_7);
		   }else{
				GPIO_SetBits(GPIOA,GPIO_Pin_7);
		   }
		   
			result.result = 1;
            result.led_on = cmd.led_on;
            result.motor_on = cmd.motor_on;
            result.buzzer = cmd.buzzer;
            xQueueSend(Result_Queue, &result, 0);
        }
    }
}
AHT data = {0};


void publishMqtt(char *topic, char *json)
{
    char pubRaw[128];

    if(xSemaphoreTake(MQTT_Mutex, portMAX_DELAY) == pdTRUE)
    {
        sprintf(pubRaw,
                "AT+MQTTPUBRAW=0,%s,%d,1,0\r\n",
                topic,
                (int)strlen(json));

        if(!SendAT(pubRaw, ">", 5000))
        {
            /* 失败也必须释放 Mutex */
            xSemaphoreGive(MQTT_Mutex);
            return;
        }

        Send_String(json);

        if(!WaitAT("OK", 2000))
        {
        }

        /* 正常发送完成，释放 Mutex */
        xSemaphoreGive(MQTT_Mutex);
    }
}

void gettime(char ts[24]){
	/* 用RTC的真实时间生成时间戳（北京时间UTC+8） */
		
		uint32_t now = RTC_GetCounter();
		if(now > 0){
			UnixTimeToStr(now, 8, ts);
		}else{
			strcpy(ts, "1970-01-01 00:00:00");
		}
}


void MQTTReturnTask(void* param){
	Result_t result;
    char mqtt_msg[256];
	char ts[24];
    while(1)
    {
        if(xQueueReceive(Result_Queue, &result, portMAX_DELAY) == pdPASS)
        {
			
			gettime(ts);
             /* 生成状态 JSON */
            sprintf(mqtt_msg,
                    "{\"type\":\"status\","
                    "\"dev\":\"mcu01\","
                    "\"ts\":\"%s\","
                    "\"body\":{\"items\":["
                    "{\"name\":\"led\",\"state\":\"%s\",\"value\":%d},"
                    "{\"name\":\"motor\",\"state\":\"%s\",\"value\":%d},"
                    "{\"name\":\"buzzer\",\"state\":\"%s\",\"value\":%d}"
                    "]}}",
                    ts,

                    result.led_on ? "on" : "off",
                    TIM_GetCapture3(TIM2),

                    result.motor_on ? "on" : "off",
                    TIM_GetCapture2(TIM2),

                    result.buzzer ? "on" : "off",
                    result.buzzer
            );

            publishMqtt("\"dev/mcu01/report\"",mqtt_msg);
        }
    }

}




void collectTask(void* param){
	char json[200];
	char ts[24];
	float tem = 27.6f;
	float humi = 36.1f;
	int light = 320;
	int ir = 2500;
	while(1){
		if(mqttState != 1){
			vTaskDelay(pdMS_TO_TICKS(1000));
			continue;
		}
		AHT30_WriteData();
		getAHT_Data(&data);
		humi = (float)data.shidu/1048576*100;
		tem = (float)data.tem/1048576*200-50;
		
		int32_t temint = (int32_t)tem;
		int32_t temdec = (tem - temint)*10;
		
		int32_t shiduint = (int32_t)humi;
		int32_t shidudec = (humi - shiduint)*10;
		gettime(ts);

		/* 先生成 JSON */
		sprintf(json,
				"{\"type\":\"sensor\","
				"\"dev\":\"mcu01\","
				"\"ts\":\"%s\","
				"\"body\":{\"data\":{"
				"\"temp\":%.1f,"
				"\"humi\":%.1f,"
				"\"light\":%d,"
				"\"ir\":%d"
				"}}}",
				ts,
				tem,
				humi,
				4095-DATA[0],
				DATA[1]);

		publishMqtt("\"dev/mcu01/report\"",json);
		
		
		vTaskDelay(pdMS_TO_TICKS(1000));
		
	}
}

/* 解析一帧 ZigBee 收到的 JSON 指令，示例：
 * {"type":"chsw","dev":"mcu01","ts":"2026-08-14 15:48:08","body":{"transport":"zigbee"}} */
static void HandleZigbeeFrame(char *frame)
{
    cJSON *root = cJSON_Parse(frame);
    if(root == NULL)
    {
        printf("Zigbee CMD parse fail\r\n");
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if(cJSON_IsString(type) && strcmp(type->valuestring, "chsw") == 0)
    {
        cJSON *body = cJSON_GetObjectItem(root, "body");
        cJSON *transport = (body != NULL) ? cJSON_GetObjectItem(body, "transport") : NULL;
        printf("Zigbee chsw, transport=%s\r\n",
               cJSON_IsString(transport) ? transport->valuestring : "(none)");
    }
    else
    {
        printf("Zigbee CMD type=%s\r\n",
               cJSON_IsString(type) ? type->valuestring : "?");
    }

    cJSON_Delete(root);
}

void ZigBeeTask(void* param){
    /* 按 JSON 完整性拆帧：'{' 开始累积，'}' 配对闭合即一帧 */
    char frame[256];
    uint16_t len = 0;
    int depth = 0;
    uint8_t data;

    while(1)
    {
        if(xQueueReceive(Zigbee_Queue, &data, portMAX_DELAY) == pdPASS)
        {
            char c = (char)data;

            /* 帧外：等待 '{' 开始一帧 */
            if(depth == 0)
            {
                if(c == '{')
                {
                    depth = 1;
                    len = 0;
                    frame[len++] = c;
                }
                continue;
            }

            /* 帧内：累积字节并跟踪 {} 深度，深度回到 0 即一帧完整 */
            if(len < sizeof(frame) - 1)
            {
                frame[len++] = c;
            }

            if(c == '{')
            {
                depth++;
            }
            else if(c == '}')
            {
                depth--;
                if(depth == 0)
                {
                    frame[len] = '\0';
                    HandleZigbeeFrame(frame);
                    len = 0;
                }
            }

            /* 缓冲满仍未闭合：丢弃，重新等下一帧 */
            if(len >= sizeof(frame) - 1 && depth > 0)
            {
                depth = 0;
                len = 0;
            }
        }
    }
}
int main()
{
    //=====初始化=====
	USART1_Init();
	RTC_CTRL_Init();
	Zigbee_Init();
	BreathLEDInit();
	LED_PWM();
	dianjiInit();
	BuzzerInit();
	AHT30Init();
	ADCInit();
	
	Cmd_Queue = xQueueCreate(10, sizeof(Cmd_t));
	
	Result_Queue = xQueueCreate(5, sizeof(Result_t));
	MQTT_Mutex = xSemaphoreCreateMutex();
	
	
	xTaskCreate(BreathLEDTask,"BreathLED",128,NULL,2,&BreathHandler);
	xTaskCreate(ZigBeeTask,"ZIGBEE",512,NULL,3,&ZigBeeHandler);
	xTaskCreate(connectWifiTask,"WIFI",512,NULL,3,&connectWifiHandler);
	
	xTaskCreate(MQTTTask,"MQTT",512,NULL,4,&MQTTHandler);
	
	xTaskCreate(collectTask,"Collection",512,NULL,1,&collectHandler);
   
	
	xTaskCreate(HardwareTask,"Hardware",128,NULL,2,&HardwareHandler);
	xTaskCreate(MQTTReturnTask,"ReturnMQTT",512,NULL,2,&MQTTReturnHandler);
	
	
	
	
	// 启动调度器
    vTaskStartScheduler();
    
    while(1)
    {
        // 不会执行到这里
		
    }
}



