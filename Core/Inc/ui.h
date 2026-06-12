#ifndef __UI_H__
#define __UI_H__

#include "main.h"
#include "lcd.h"

/* 颜色定义 */
#define UI_WHITE      0xFFFF
#define UI_BLACK      0x0000
#define UI_RED        0xF800
#define UI_GREEN      0x07E0
#define UI_BLUE       0x001F
#define UI_YELLOW     0xFFE0
#define UI_CYAN       0x07FF
#define UI_GRAY       0x8430
#define UI_LIGHTGRAY  0xC618
#define UI_TITLE_BG   0x02B0   /* 深蓝标题栏 */
#define UI_BAR_BG     0xAED8   /* 浅蓝状态条 */
#define UI_LOG_BG     0xEF7D   /* 浅灰日志底 */
#define UI_BORDER     0x632C   /* 分割线色 */

/* 布局定义 */
#define UI_TOP_BAR_H   44
#define UI_STATUS_H    28
#define UI_FACE_Y      75
#define UI_FACE_H      195
#define UI_LOG_Y       275
#define UI_LOG_H       110
#define UI_BTN_Y       390
#define UI_BTN_H       40
#define UI_STATS_Y     434
#define UI_STATS_H     46

/* 日志缓冲 */
#define UI_LOG_LINES   5
#define UI_LOG_LEN     40

/* 函数 */
void UI_Init(void);
void UI_UpdateStatus(uint8_t lock, uint8_t users);
void UI_UpdateFace(int16_t state, int16_t yaw, int16_t left, int16_t top, int16_t right, int16_t bottom);
void UI_Log(const char *msg, uint16_t color);
void UI_ShowAlarm(const char *msg);
void UI_ClearAlarm(void);

#endif
