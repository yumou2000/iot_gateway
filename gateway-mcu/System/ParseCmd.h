#ifndef __PARSECMD_H
#define __PARSECMD_H
#include "stm32f10x.h"                  // Device header
#include "cJSON.h"

/* 传感器数据上传通道(由下行 {"transport":"mqtt"|"zigbee"} 切换):
 * MQTT   -> publishMqtt 经 ESP8266 上报
 * ZIGBEE -> Zigbee_SendLine 经 USART2/ZigBee 链路上报，不依赖 MQTT 连接 */
#define TRANSPORT_MQTT   0
#define TRANSPORT_ZIGBEE 1

extern volatile uint8_t transportMode;

typedef struct
{
	uint8_t valid;
    uint8_t transport;   // 1 = body/顶层 transport == "zigbee"(通道切换标记)
    uint8_t led_on;
    uint8_t led_br;

    uint8_t motor_on;
    uint8_t motor_sp;
    uint8_t motor_dir;

    uint8_t buzzer;
} Cmd_t;


typedef struct
{
    uint8_t result;       // 1成功，0失败
    uint8_t led_on;
    uint8_t motor_on;
    uint8_t buzzer;
} Result_t;

Cmd_t ParseCmd(char* msg);

/* 从消息中提取完整 JSON（第一个 { 到最后一个 }），返回长度；找不到或超长返回 0 */
int ExtractJson(char *msg, char *out, int maxLen);


#endif