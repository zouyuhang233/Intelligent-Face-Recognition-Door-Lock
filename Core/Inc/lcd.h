#ifndef __LCD_H__
#define __LCD_H__

#include "main.h"

/* ============ LCD 硬件配置 ============ */
#define LCD_W_REG   (*((volatile uint16_t *)0x6C000000))  // 命令
#define LCD_W_DAT   (*((volatile uint16_t *)0x6C000800))  // 数据

#define LCD_WIDTH   320
#define LCD_HEIGHT  480

/* LCD 背光控制 */
#define LCD_BL_GPIO_Port  GPIOB
#define LCD_BL_Pin        GPIO_PIN_0

/* ============ 颜色定义 (RGB565) ============ */
#define WHITE       0xFFFF
#define BLACK       0x0000
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define YELLOW      0xFFE0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define GRAY        0x8430

/* ============ 函数声明 ============ */
void LCD_Init(void);
void LCD_Clear(uint16_t color);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void LCD_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bgcolor);
void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bgcolor);

#endif
