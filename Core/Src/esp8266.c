#include "esp8266.h"
#include "usart.h"
#include "lcd.h"
#include "fm22x.h"
#include <stdio.h>
#include <string.h>

/* ============ 接收缓冲区 ============ */
char rx_buf[512];
static volatile uint16_t rx_idx = 0;
static uint8_t rx_byte;

/* ============ 摄像头接收缓冲 ============ */
uint8_t cam_rx_byte;
volatile uint32_t raw_byte_count = 0;

/* ============ 接收回调 ============ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  // USART3: ESP8266 WiFi
  if (huart->Instance == USART3)
  {
    if (rx_idx < sizeof(rx_buf) - 1)
    {
      rx_buf[rx_idx++] = rx_byte;
      rx_buf[rx_idx] = '\0';
    }
    HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
  }
  // USART2: FM22x 摄像头模组（与官方 FM225 回调一致）
  if (huart->Instance == USART2)
  {
    raw_byte_count++;
    FM22x_UART_Callback(cam_rx_byte);
    HAL_UART_Receive_IT(&huart2, &cam_rx_byte, 1);
  }
}

/* ============ ESP8266 初始化 ============ */
void ESP8266_Init(void)
{
  MX_USART3_UART_Init();
  rx_idx = 0;
  memset(rx_buf, 0, sizeof(rx_buf));
  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
  HAL_Delay(3000);  /* 等待 ESP8266 启动 */

  /* 尝试多次 AT 测试 */
  for (int i = 0; i < 3; i++) {
    if (ESP8266_SendCmd("AT", "OK", 2000)) {
      LCD_ShowString(10, 80, "[0] ESP8266 Ready    ", GREEN, WHITE);
      break;
    }
    HAL_Delay(1000);
  }
}

/* ============ 清空接收缓冲 ============ */
static void ClearRxBuf(void)
{
  rx_idx = 0;
  memset(rx_buf, 0, sizeof(rx_buf));
}

/* ============ 发送 AT 指令并等待期望回复 ============ */
uint8_t ESP8266_SendCmd(const char *cmd, const char *expected, uint16_t timeout)
{
  ClearRxBuf();
  HAL_UART_Transmit(&huart3, (uint8_t *)cmd, strlen(cmd), 500);
  HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, 50);

  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout)
  {
    if (strstr(rx_buf, expected) != NULL)
      return 1;
    if (strstr(rx_buf, "ERROR") != NULL)
      return 0;
    if (strstr(rx_buf, "FAIL") != NULL)
      return 0;
    HAL_Delay(10);
  }
  return 0;
}

/* ============ 发送原始数据 ============ */
void ESP8266_SendData(const char *data)
{
  HAL_UART_Transmit(&huart3, (uint8_t *)data, strlen(data), 500);
}

/* ============ 连接 WiFi ============ */
uint8_t ESP8266_ConnectWiFi(const char *ssid, const char *password)
{
  char cmd[128];
  sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
  return ESP8266_SendCmd(cmd, "WIFI GOT IP", 20000);
}

/* ============ MQTT 配置 ============ */
uint8_t ESP8266_MQTTConfig(const char *client_id, const char *user, const char *pass)
{
  char cmd[512];
  strcpy(cmd, "AT+MQTTUSERCFG=0,1,\"");
  strcat(cmd, client_id);
  strcat(cmd, "\",\"");
  strcat(cmd, user);
  strcat(cmd, "\",\"");
  strcat(cmd, pass);
  strcat(cmd, "\",0,0,\"\"");
  return ESP8266_SendCmd(cmd, "OK", 10000);
}

/* ============ MQTT 连接 ============ */
uint8_t ESP8266_MQTTConnect(const char *host, uint16_t port)
{
  char cmd[128];
  sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,1", host, port);
  return ESP8266_SendCmd(cmd, "MQTTCONNECTED", 15000);
}

/* ============ MQTT 订阅 ============ */
uint8_t ESP8266_MQTTSubscribe(const char *topic)
{
  char cmd[256];
  sprintf(cmd, "AT+MQTTSUB=0,\"%s\",0", topic);
  return ESP8266_SendCmd(cmd, "OK", 5000);
}

/* ============ MQTT 发布 ============ */
uint8_t ESP8266_MQTTPublish(const char *topic, const char *payload)
{
  char cmd[1024];
  sprintf(cmd, "AT+MQTTPUB=0,\"%s\",\"%s\",0,0", topic, payload);
  return ESP8266_SendCmd(cmd, "OK", 8000);
}

/* ============ OneNet 连接 ============ */
uint8_t OneNet_Connect(void)
{
  /* 清除 WiFi 状态显示区域 */
  LCD_FillRect(10, 80, 310, 260, WHITE);

  // Step 1: 设置 WiFi 模式
  LCD_ShowString(10, 80,  "[1] Set mode...", BLACK, WHITE);
  if (!ESP8266_SendCmd("AT+CWMODE=1", "OK", 3000))
  {
    LCD_ShowString(10, 80,  "[1] Mode FAIL        ", RED, WHITE);
    return 0;
  }
  LCD_ShowString(10, 80,  "[1] Mode OK          ", GREEN, WHITE);

  // Step 2: 连接 WiFi
  LCD_ShowString(10, 110, "[2] WiFi connecting..", BLACK, WHITE);
  if (!ESP8266_ConnectWiFi("onenet", "123456789"))
  {
    LCD_ShowString(10, 110, "[2] WiFi FAIL        ", RED, WHITE);
    return 0;
  }
  LCD_ShowString(10, 110, "[2] WiFi OK          ", GREEN, WHITE);
  HAL_Delay(2000);

  // Step 3: MQTT Config
  LCD_ShowString(10, 140, "[3] MQTT config...", BLACK, WHITE);
  if (!ESP8266_MQTTConfig(
      "onenet_one",
      "u9gXgMhYR1",
      "version=2018-10-31&res=products%2Fu9gXgMhYR1%2Fdevices%2Fonenet_one&et=1812503220&method=md5&sign=ARYnb3UiXlTGHWqufrpm6A%3D%3D"))
  {
    LCD_ShowString(10, 140, "[3] Config FAIL      ", RED, WHITE);
    return 0;
  }
  LCD_ShowString(10, 140, "[3] Config OK        ", GREEN, WHITE);

  // Step 4: MQTT Connect
  LCD_ShowString(10, 170, "[4] MQTT connect...", BLACK, WHITE);
  if (!ESP8266_MQTTConnect("mqtts.heclouds.com", 1883))
  {
    LCD_ShowString(10, 170, "[4] MQTT FAIL        ", RED, WHITE);
    return 0;
  }
  LCD_ShowString(10, 170, "[4] MQTT OK          ", GREEN, WHITE);

  // Step 5: Subscribe
  LCD_ShowString(10, 200, "[5] Subscribe...", BLACK, WHITE);
  ESP8266_MQTTSubscribe("$sys/u9gXgMhYR1/onenet_one/thing/property/post/reply");
  ESP8266_MQTTSubscribe("$sys/u9gXgMhYR1/onenet_one/thing/property/set");
  LCD_ShowString(10, 200, "[5] Subscribed       ", GREEN, WHITE);

  return 1;
}

/* ============ 上报锁状态 ============ */
uint8_t OneNet_ReportLock(uint8_t is_open)
{
  char cmd[512];
  sprintf(cmd, "AT+MQTTPUB=0,\"$sys/u9gXgMhYR1/onenet_one/thing/property/post\","
    "\"{\\\"id\\\":\\\"123\\\"\\,\\\"version\\\":\\\"1.0\\\"\\,\\\"params\\\":"
    "{\\\"LOCK\\\":{\\\"value\\\":%s}}}\",0,0",
    is_open ? "true" : "false");
  return ESP8266_SendCmd(cmd, "OK", 8000);
}

/* ============ 上报警报数据 (EERNUM) ============ */
uint8_t OneNet_ReportResult(uint8_t is_success)
{
  char cmd[512];
  sprintf(cmd, "AT+MQTTPUB=0,\"$sys/u9gXgMhYR1/onenet_one/thing/property/post\","
    "\"{\\\"id\\\":\\\"123\\\"\\,\\\"version\\\":\\\"1.0\\\"\\,\\\"params\\\":"
    "{\\\"EERNUM\\\":{\\\"value\\\":%s}}}\",0,0",
    is_success ? "true" : "false");
  return ESP8266_SendCmd(cmd, "OK", 8000);
}

/* ============ 上报用户数量 ============ */
uint8_t OneNet_ReportUserNum(uint8_t num)
{
  char cmd[512];
  sprintf(cmd, "AT+MQTTPUB=0,\"$sys/u9gXgMhYR1/onenet_one/thing/property/post\","
    "\"{\\\"id\\\":\\\"123\\\"\\,\\\"version\\\":\\\"1.0\\\"\\,\\\"params\\\":"
    "{\\\"USER_NUM\\\":{\\\"value\\\":%d}}}\",0,0",
    num);
  return ESP8266_SendCmd(cmd, "OK", 8000);
}

/* ============ 检查云平台下发指令 ============ */
// 返回值: -1=无指令, 0=关锁, 1=开锁, 2=人脸验证, 3=人脸录入, 4=清除用户, 5=远程报警, 6=arr_id清空
int8_t OneNet_CheckCommand(char *cmd_id)
{
  // ESP8266 收到 MQTT 消息格式: +MQTTSUBRECV:0,"topic",len,payload
  char *p = strstr(rx_buf, "+MQTTSUBRECV:");
  if (p == NULL)
    return -1;

  // 复制一份处理（避免修改原缓冲）
  char buf[512];
  strncpy(buf, p, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  // 提取 payload（最后一个逗号后面）
  char *payload = strrchr(buf, ',');
  if (payload == NULL) return -1;
  payload++;

  // 提取指令ID: {"id":"xx",...}
  cmd_id[0] = '\0';
  char *id_start = strstr(payload, "\"id\":\"");
  if (id_start)
  {
    id_start += 6;
    char *id_end = strchr(id_start, '"');
    if (id_end && (id_end - id_start) < 32)
    {
      strncpy(cmd_id, id_start, id_end - id_start);
      cmd_id[id_end - id_start] = '\0';
    }
  }

  // 检查 OPEN_JC (人脸验证)
  if (strstr(payload, "OPEN_JC") != NULL)
  {
    if (strstr(payload, "true") != NULL)
    {
      ClearRxBuf();
      return 2;
    }
  }

  // 检查 OPEN_LR (人脸录入)
  if (strstr(payload, "OPEN_LR") != NULL)
  {
    if (strstr(payload, "true") != NULL)
    {
      ClearRxBuf();
      return 3;
    }
  }

  // 检查 Clear_user (清除用户)
  if (strstr(payload, "Clear_user") != NULL)
  {
    if (strstr(payload, "true") != NULL)
    {
      ClearRxBuf();
      return 4;
    }
  }

  // 检查 YC_BJ (远程报警)
  if (strstr(payload, "YC_BJ") != NULL)
  {
    if (strstr(payload, "true") != NULL)
    {
      ClearRxBuf();
      return 5;
    }
  }

  // 检查 arr_id 指令（云平台下发用户数组）
  if (strstr(payload, "arr_id") != NULL)
  {
    /* arr_id = [] 表示清空用户 */
    if (strstr(payload, "[]") != NULL)
    {
      ClearRxBuf();
      return 6;  // 删除全部用户
    }
    /* arr_id 有值则忽略（设备端管理） */
    ClearRxBuf();
    return -1;
  }

  // 检查 LOCK 指令
  if (strstr(payload, "LOCK") != NULL)
  {
    if (strstr(payload, "true") != NULL)
    {
      ClearRxBuf();
      return 1;
    }
    if (strstr(payload, "false") != NULL)
    {
      ClearRxBuf();
      return 0;
    }
  }

  ClearRxBuf();
  return -1;
}

/* ============ 回复 set_reply ============ */
uint8_t OneNet_SendSetReply(const char *cmd_id)
{
  char cmd[512];
  sprintf(cmd, "AT+MQTTPUB=0,\"$sys/u9gXgMhYR1/onenet_one/thing/property/set_reply\",\"{\\\"id\\\":\\\"%s\\\"\\,\\\"code\\\":200\\,\\\"msg\\\":\\\"success\\\"}\",0,0",
    cmd_id);
  return ESP8266_SendCmd(cmd, "OK", 5000);
}

/* ============ 上报用户ID数组 (arr_id) ============ */
uint8_t OneNet_ReportUserIDArray(uint16_t *ids, uint8_t count)
{
  char val[256];
  int p = 0;
  if (count == 0) {
    p += sprintf(val + p, "");
  } else {
    for (uint8_t i = 0; i < count; i++) {
      p += sprintf(val + p, "%d", ids[i]);
      if (i < count - 1) p += sprintf(val + p, ",");
    }
  }

  char cmd[768];
  sprintf(cmd, "AT+MQTTPUB=0,\"$sys/u9gXgMhYR1/onenet_one/thing/property/post\","
    "\"{\\\"id\\\":\\\"123\\\"\\,\\\"version\\\":\\\"1.0\\\"\\,\\\"params\\\":"
    "{\\\"arr_id\\\":{\\\"value\\\":\\\"%s\\\"}}}\",0,0", val);
  return ESP8266_SendCmd(cmd, "OK", 8000);
}
