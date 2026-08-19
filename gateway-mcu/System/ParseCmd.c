#include "ParseCmd.h"
#include <string.h>

volatile uint8_t transportMode = TRANSPORT_MQTT;

/* 从消息中提取完整 JSON（第一个 { 到最后一个 }），返回长度；找不到或超长返回 0。
 * 超长不截断：截断会丢掉末尾 '}'，产生坏帧。 */
int ExtractJson(char *msg, char *out, int maxLen)
{
    char *start = strchr(msg, '{');
    char *end   = strrchr(msg, '}');

    if(start == NULL || end == NULL || end <= start)
    {
        return 0;
    }

    int len = (int)(end - start + 1);

    if(len >= maxLen)
    {
        return 0;
    }

    strncpy(out, start, len);
    out[len] = '\0';

    return len;
}

/* 解析完整控制命令，返回结构体 */
Cmd_t ParseCmd(char *msg)
{
    Cmd_t cmd = {0};

    char json[256];

    if(ExtractJson(msg, json, sizeof(json)) == 0)
    {
        return cmd;
    }

    cJSON *root = cJSON_Parse(json);

    if(root == NULL)
    {
        return cmd;
    }

    /* 传输模式切换：不依赖 type，顶层 {"transport":"mqtt"|"zigbee"} 或
     * body.transport 均可触发（兼容网关下发的 chsw 信封与裸切换指令）。
     * 例如 {"type":"chsw","dev":"mcu01",...,"body":{"transport":"zigbee"}} */
    cJSON *transport = cJSON_GetObjectItem(root, "transport");
    cJSON *body = cJSON_GetObjectItem(root, "body");

    if(!cJSON_IsString(transport) && cJSON_IsObject(body))
    {
        transport = cJSON_GetObjectItem(body, "transport");
    }

    if(cJSON_IsString(transport))
    {
        if(strcmp(transport->valuestring, "zigbee") == 0)
        {
            transportMode = TRANSPORT_ZIGBEE;
            cmd.transport = 1;
        }
        else if(strcmp(transport->valuestring, "mqtt") == 0)
        {
            transportMode = TRANSPORT_MQTT;
        }
    }

    /* 检查 type */
    cJSON *type = cJSON_GetObjectItem(root, "type");

    if(!cJSON_IsString(type) ||
       strcmp(type->valuestring, "cmd") != 0)
    {
        cJSON_Delete(root);
        return cmd;
    }

    /* 获取 body（transport 解析时已取过，这里复用同一指针） */
    body = cJSON_GetObjectItem(root, "body");

    if(!cJSON_IsObject(body))
    {
        cJSON_Delete(root);
        return cmd;
    }

    /* LED */
    cJSON *led_on = cJSON_GetObjectItem(body, "led_on");
    if(cJSON_IsNumber(led_on))
    {
        cmd.led_on = led_on->valueint;
    }

    cJSON *led_br = cJSON_GetObjectItem(body, "led_br");
    if(cJSON_IsNumber(led_br))
    {
        cmd.led_br = led_br->valueint;
    }

    /* Motor */
    cJSON *motor_on = cJSON_GetObjectItem(body, "motor_on");
    if(cJSON_IsNumber(motor_on))
    {
        cmd.motor_on = motor_on->valueint;
    }

    cJSON *motor_sp = cJSON_GetObjectItem(body, "motor_sp");
    if(cJSON_IsNumber(motor_sp))
    {
        cmd.motor_sp = motor_sp->valueint;
    }

    cJSON *motor_dir = cJSON_GetObjectItem(body, "motor_dir");
    if(cJSON_IsNumber(motor_dir))
    {
        cmd.motor_dir = motor_dir->valueint;
    }

    /* Buzzer */
    cJSON *buzzer = cJSON_GetObjectItem(body, "buzzer");
    if(cJSON_IsNumber(buzzer))
    {
        cmd.buzzer = buzzer->valueint;
    }
	
	cmd.valid = 1;
    cJSON_Delete(root);

    return cmd;
}
 
 
static cJSON* FindCmdItem(cJSON* obj){
	if(obj == NULL) return NULL;
	if(cJSON_IsObject(obj)){
		cJSON* item = cJSON_GetObjectItem(obj, "body");
		if(item != NULL && cJSON_IsString(item)) return item;
		cJSON* child = obj->child;
		while(child != NULL){
			cJSON* found = FindCmdItem(child);
			if(found != NULL) return found;
			child = child->next;
		}
	}
	return NULL;
}


