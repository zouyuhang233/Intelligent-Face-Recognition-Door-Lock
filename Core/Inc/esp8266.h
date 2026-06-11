#ifndef __ESP8266_H__
#define __ESP8266_H__

#include "main.h"
#include <string.h>

extern UART_HandleTypeDef huart3;
extern char rx_buf[512];

/* ============ 初始化 ============ */
void ESP8266_Init(void);

/* ============ AT 指令基础 ============ */
uint8_t ESP8266_SendCmd(const char *cmd, const char *expected, uint16_t timeout);
void    ESP8266_SendData(const char *data);

/* ============ WiFi 连接 ============ */
uint8_t ESP8266_ConnectWiFi(const char *ssid, const char *password);

/* ============ MQTT 操作 ============ */
uint8_t ESP8266_MQTTConfig(const char *client_id, const char *user, const char *pass);
uint8_t ESP8266_MQTTConnect(const char *host, uint16_t port);
uint8_t ESP8266_MQTTSubscribe(const char *topic);
uint8_t ESP8266_MQTTPublish(const char *topic, const char *payload);

/* ============ OneNet 专用 ============ */
uint8_t OneNet_Connect(void);
uint8_t OneNet_ReportData(const char *lock_state, const char *led_state);
uint8_t OneNet_ReportLock(uint8_t is_open);
uint8_t OneNet_ReportResult(uint8_t is_success);  // 上报识别结果 (EERNUM + YC_BJ)
uint8_t OneNet_ReportUserNum(uint8_t num);  // 上报用户数量 (USER_NUM)
int8_t OneNet_CheckCommand(char *cmd_id);  // 检查云平台下发指令
uint8_t OneNet_SendSetReply(const char *cmd_id);  // 回复 set_reply

#endif
