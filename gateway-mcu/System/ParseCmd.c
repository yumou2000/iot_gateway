#include "ParseCmd.h"
#include <string.h>

/* 递归查找任意层级的 "cmd" 字符串字段，
 * 支持 {"data":{"cmd":"LED_ON"}} 这类嵌套结构 */
 


/* 解析完整控制命令，返回结构体 */
Cmd_t ParseCmd(char *msg)
{
    Cmd_t cmd = {0};

    char *start = strchr(msg, '{');
    char *end   = strrchr(msg, '}');

    if(start == NULL || end == NULL || end <= start)
    {
        return cmd;
    }

    char json[256];

    int len = (int)(end - start + 1);

    if(len >= sizeof(json))
    {
        len = sizeof(json) - 1;
    }

    strncpy(json, start, len);
    json[len] = '\0';

    cJSON *root = cJSON_Parse(json);

    if(root == NULL)
    {
        return cmd;
    }

    /* 检查 type */
    cJSON *type = cJSON_GetObjectItem(root, "type");

    if(!cJSON_IsString(type) ||
       strcmp(type->valuestring, "cmd") != 0)
    {
        cJSON_Delete(root);
        return cmd;
    }

    /* 获取 body */
    cJSON *body = cJSON_GetObjectItem(root, "body");

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


