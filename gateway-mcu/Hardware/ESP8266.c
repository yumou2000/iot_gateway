#include "stm32f10x.h"                  // Device header
#include "ESP8266.h"
#include "OLED.h"
#include "FreeRTOS.h"
#include "task.h"
//帮我们发送指令
//参数一：指令
//参数二：发送指令后返回要匹配的字符串
//参数三：超时时间 ,比如500Ms
//return：1，成功；0，失败
/* message 是 USART1 IDLE 中断（processNewData）与 AT 状态机共享的全局缓冲。
 * 任务侧所有读写必须进临界区（关闭全部中断），否则与中断写入竞争，
 * 轻则丢命令，重则使 SendAT 匹配到被污染的中间态。 */
static void AT_ClearMessage(void)
{
	taskENTER_CRITICAL();
	memset(message, 0x00, BUFFER_SIZE);
	taskEXIT_CRITICAL();
}

/* 行级边界匹配：命中子串且其前一字符是行首（串首/\r/\n/空格）。
 * 避免 ESP8266 异步上行帧（+MQTTSUBRECV 下行的 JSON、+MQTTDISCONNECTED 等）
 * 内容里的 "OK"/"ERROR"/">" 等子串误触发 SendAT 匹配，
 * 同时兼容 HTTP 响应里的 "200 OK"（OK 前是空格）。 */
static uint8_t AT_HasText(const char *s)
{
	uint8_t found = 0;
	taskENTER_CRITICAL();
	const char *p = message;
	while((p = strstr(p, s)) != NULL)
	{
		if(p == message || p[-1] == '\r' || p[-1] == '\n' || p[-1] == ' ')
		{
			found = 1;
			break;
		}
		p++;
	}
	taskEXIT_CRITICAL();
	return found;
}

uint8_t SendAT(char *cmd , char *match , uint32_t timeOut){
	
	AT_ClearMessage();
	//1.发送指令到串口
	Send_String(cmd);
	
	uint32_t time = 0;
	while(time < timeOut){
		//2.如果串口收到了数据，包含了是实际要返回的值
		if(AT_HasText(match)){
			return 1;
		}
		//3.如果收到busy,error,fail,就返回0
		if(AT_HasText("busy")
			|| AT_HasText("ERROR")
			|| AT_HasText("FAIL")
			|| AT_HasText("FALL"))
		{
			return 0;
		}
		//4.如果上面的都没匹配到，延时5ms，继续循环等待
		time+=5;
		vTaskDelay(5);
	}
	//超过超时时间，还没匹配到想要的字符串，返回0
	return 0;
}

//连接WIFI的函数，连接之前关闭回显
/*
AT   测试指令
AT+CWMODE=1   设置成连接WIFI的模式 
AT+CWLAP   查询附近的可连接WIFI
AT+CWJAP="Mate","12356789"   连接WIFI
AT+CWJAP? 查询当前连接上的网络情况
AT+CIFSR      查询是否有IP地址
*/
uint8_t CloseATEAndConnectWifi(void){
	/* 0. 探测 ESP8266 是否在线：复位 STM32 时 ESP8266 常未断电，若上次
	 *    MQTT PUBRAW 发送中断导致模块卡在"等待 payload"状态，所有 AT 指令
	 *    都会被吞掉。探测失败先解卡 + 软复位，否则会永远连不上 WiFi。 */
	if(!SendAT("AT\r\n", "OK", 500)){
		ESP_Restart();
	}
	//1.关闭回显
	SendAT("ATE0\r\n","OK",500);
	
	//2.连接wifi
	//此固件已连接时返回 +CWJAP:"ssid",...（+CWJAP: 后直接是引号），
	//匹配 +CWJAP:" 表示已连接；未连接时返回 +CWJAP:0 或 ERROR 不会命中
	if(SendAT("AT+CWJAP?\r\n","+CWJAP:\"",500)){
		return 1;   // 已连接
	}
	else{//如果WIFI之前没连接上，需要不断重连
		while(1){
			SendAT("AT+CWMODE=1\r\n","OK",500);
			SendAT("AT+CWJAP=\"jang\",\"12345678\"\r\n","OK",20000);
			
			if(SendAT("AT+CWJAP?\r\n","+CWJAP:\"",500)){
				return 1;
			}
			
			vTaskDelay(pdMS_TO_TICKS(3000));
		}
	}
}

//发送完HTTP请求后，等待message接收到的信息，
//解并且从信息中析JSON
uint8_t parseJSON(char *rxJSON,uint32_t timeOut){
	//1.判断延时时间
	uint32_t time = 0;
	while(time < timeOut){
		//2.从message中获取 JSON中的两个'{' , '}'
		char *start = strchr(message,'{');
		char *end = strrchr(message,'}');
		
		//3.如果没有{}，表示还没收到返回的JSON，或者数据有问题
		if(!start || !end){
			time+=5;
			vTaskDelay(5);
		}
		else{
			int len = end - start + 1;
			strncpy(rxJSON,start,len);//从start开始位置拷贝，把拷贝进去的发到rxJSON中
			rxJSON[len] = '\0';
			return 1;
		}	
	}
	return 0;
}



/*
AT+CIPMUX=0             单连接

AT+CIPSTART="TCP","api.pinduoduo.com",80
// 等待返回 CONNECT OK

AT+CIPSEND=77
// 等待返回 >

// 立即发送以下内容（最后有两个\r\n\r\n）：
GET /api/server/_stm HTTP/1.1
Host: api.pinduoduo.com
Connection: close
*/

//通过网络同步时间，返回Unix秒（北京时间同UTC秒数）
uint32_t SynchronizeTime(void){
	
	//先清理可能残留的连接（失败忽略，继续尝试）：
	//复位STM32时ESP8266常未断电，残留的TCP/MQTT连接会占用LinkID，
	//导致后面的AT+CIPSTART失败，时间同步失败
	SendAT("AT+CIPCLOSE\r\n","OK",1000);
	SendAT("AT+MQTTCLEAN=0\r\n","OK",1000);
	
	//拼多多时间接口：返回 {"server_time":毫秒时间戳}
	char *httpReq = "GET /api/server/_stm HTTP/1.1\r\n"
					"Host: api.pinduoduo.com\r\n"
					"Connection: close\r\n\r\n";
	
	if(!SendAT("AT+CIPMUX=0\r\n","OK",1000)){
		return 0;
	}
	
	if(!SendAT("AT+CIPSTART=\"TCP\",\"api.pinduoduo.com\",80\r\n","OK",5000)){
		return 0;
	}
	
	char cmd[20];
	snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%d\r\n",strlen(httpReq));
	if(!SendAT(cmd , ">" , 5000)){
		return 0;
	}
	//发送HTTP请求，并等待ESP8266确认（参考项目写法：等"OK"，
	//匹配HTTP响应头里的"200 OK"，确保服务器响应真正到达再解析）
	if(!SendAT(httpReq , "OK" , 5000)){
		return 0;
	}
	
	//从message的服务器返回的所有的数据中，解析出JSON字符串
	char rxJSON[500] = {0};
	
	if(parseJSON(rxJSON,10000)){
		//使cJOSN,解析
		cJSON *root = cJSON_Parse(rxJSON);
		cJSON *serverTime = cJSON_GetObjectItem(root ,"server_time");
		if(serverTime != NULL){
			//注意：server_time 是毫秒时间戳（如1786623815689），
			//超出uint32_t范围，必须先除以1000变成秒，再转uint32_t
			uint32_t pddTimeLong = (uint32_t)(serverTime->valuedouble / 1000);
			cJSON_Delete(root);
			return pddTimeLong;
		}
		cJSON_Delete(root);
	}
	return 0;
	
}


//同步天气
void SynchronizeTianqi(void){
	
	char *httpReq = "GET /v3/weather/now.json?key=St63SF7GwsF4p5Dtc&location=beijing&language=en&unit=c HTTP/1.1\r\n"
					"Host: api.seniverse.com\r\n"
					"Connection: close\r\n\r\n";
	
	if(!SendAT("AT+CIPMUX=0\r\n","OK",1000)){
		OLED_ShowString(3,1,"Weather ERROR");
		
	}
	
	if(!SendAT("AT+CIPSTART=\"TCP\",\"api.seniverse.com\",80\r\n","OK",5000)){
		OLED_ShowString(3,1,"Weather ERROR");
		
	}
	
	char cmd[20];
	snprintf(cmd,sizeof(cmd),"AT+CIPSEND=%d\r\n",strlen(httpReq));
	if(!SendAT(cmd , ">" , 5000)){
		OLED_ShowString(3,1,"Weather ERROR");
	}
	//发送HTTP请求，获取返回的Json
	Send_String(httpReq);
	
	//从message的服务器返回的所有的数据中，解析出JSON字符串
	char rxJSON[1024];
	
	if(parseJSON(rxJSON,5000)){
		//使cJOSN,解析
		cJSON *root = cJSON_Parse(rxJSON);
		cJSON *results = cJSON_GetObjectItem(root ,"results");
		cJSON *result = cJSON_GetArrayItem(results,0);
		cJSON *now = cJSON_GetObjectItem(result ,"now");
		cJSON *text = cJSON_GetObjectItem(now ,"text");
		cJSON *temperature = cJSON_GetObjectItem(now ,"temperature");
		
		OLED_ShowString(3,1,text->valuestring);
		OLED_ShowString(3,10,temperature->valuestring);
		
		cJSON_Delete(root);
		
	}
	else
	{
		OLED_ShowString(3,1,"Weather Error");
	}
	
}

/* 等待串口返回匹配字符串，但不清空接收缓冲区。
 * 用于 MQTTPUBRAW 发送数据后等待 "OK"，避免 SendAT 开头的清空操作
 * 把模组刚返回的数据丢掉，导致永远等不到结果。
 * return：1，成功；0，失败 */
uint8_t WaitAT(char *match , uint32_t timeOut)
{
	uint32_t time = 0;
	while(time < timeOut){
		if(AT_HasText(match)){
			return 1;
		}
		if(AT_HasText("busy")
			|| AT_HasText("ERROR")
			|| AT_HasText("FAIL"))
		{
			return 0;
		}
		time+=5;
		/* 必须用 vTaskDelay：Delay_ms 会改写 SysTick 寄存器，与 FreeRTOS
		 * tick 时钟冲突（见 Delay.c 说明），会把整个系统拖死 */
		vTaskDelay(pdMS_TO_TICKS(5));
	}
	return 0;
}

/* 向 ESP8266 补发填充字节，使其退出 MQTTPUBRAW 等待 payload 状态 */
void ESP_FlushMqttPayload(int len)
{
	char fill[64];
	while(len > 0)
	{
		int n = (len > (int)sizeof(fill) - 1) ? (int)sizeof(fill) - 1 : len;
		memset(fill, ' ', n);
		fill[n] = '\0';
		Send_String(fill);
		len -= n;
	}
	vTaskDelay(pdMS_TO_TICKS(100));   /* 等模块处理完（回 OK/ERROR） */
	AT_ClearMessage();
}

void ESP_Restart(void)
{
	ESP_FlushMqttPayload(512);   /* 覆盖任何 PUBRAW 长度，先解卡 */
	Send_String("AT+RST\r\n");
	/* 等待重启完成（回 "ready"），最长 5s */
	uint32_t time = 0;
	while(time < 5000){
		if(AT_HasText("ready")){
			break;
		}
		time += 100;
		vTaskDelay(pdMS_TO_TICKS(100));
	}
	if(time >= 5000){
		/* 软复位无响应（AT 指令流已被完全吞掉）：RST 引脚硬件复位兜底 */
		ESP8266_RST_HardReset();
	}
	else
	{
		vTaskDelay(pdMS_TO_TICKS(1000));   /* 再等模块稳定 */
	}
	AT_ClearMessage();
}

/* ============ ESP8266 硬件复位（RST 引脚接 PB14） ============ */

/* 上电初始化：复位 STM32 时 ESP8266 不断电，旧卡死状态会残留，
 * 这里主动拉低 RST ≥200ms 再释放，让 ESP8266 每次随 STM32 干净重启。
 * 在调度器启动前（main 初始化）调用，用 DWT 版 Delay_ms 无冲突。 */
void ESP8266_RST_Init(void)
{
	GPIO_InitTypeDef GPIO_Struct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_Struct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Struct.GPIO_Pin = GPIO_Pin_14;
	GPIO_Struct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_Struct);

	GPIO_ResetBits(GPIOB, GPIO_Pin_14);   /* 拉低：复位 ESP8266 */
	Delay_ms(200);
	GPIO_SetBits(GPIOB, GPIO_Pin_14);     /* 释放复位 */
	Delay_ms(500);                        /* 等 AT 固件启动完成 */
}

/* 任务上下文用的硬件复位（调度器已启动，必须用 vTaskDelay） */
void ESP8266_RST_HardReset(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_14);
	vTaskDelay(pdMS_TO_TICKS(200));
	GPIO_SetBits(GPIOB, GPIO_Pin_14);
	vTaskDelay(pdMS_TO_TICKS(1000));   /* 等 AT 固件启动 */
	AT_ClearMessage();
}

uint8_t ESP_EnsureWifi(void)
{
	if(SendAT("AT+CWJAP?\r\n", "+CWJAP:\"", 500)){
		return 1;
	}
	SendAT("AT+CWMODE=1\r\n", "OK", 500);
	return SendAT("AT+CWJAP=\"jang\",\"12345678\"\r\n", "OK", 20000);
}

uint8_t connectMQTT(uint32_t timeout)
{
	uint32_t time = 0;
	uint8_t flag1 = 1;
	uint8_t flag2 = 1;
	uint8_t flag3 = 1;
	uint8_t flag4 = 1;

	/* 清理可能残留的 MQTT 连接（断线重连场景），失败忽略 */
	SendAT("AT+MQTTDISC=0\r\n", "OK", 500);
	
	while(time<timeout){
		if(flag1 && !SendAT("AT+MQTTUSERCFG=0,1,\"ESP8266_yu\",\"yu\",\"123456789\",0,0,\"\"\r\n","OK",500)){
			time += 5;
			vTaskDelay(5);
			continue;
		}
		flag1 = 0;
		if(flag2 && !SendAT("AT+MQTTCONN=0,\"192.168.137.198\",1883,0\r\n","+MQTTCONNECTED",5000))
		{
			time += 5;
			vTaskDelay(5);
			continue;
		}
		flag2 = 0;

		if(flag3 && !SendAT("AT+MQTTSUB=0,\"dev/mcu01/cmd\",1\r\n","OK",5000))
		{
			time += 5;
			vTaskDelay(5);
			return 0;
		}
		flag3 = 0;
			///发送数据
		return 1;
		
	}
	return 0;
}



