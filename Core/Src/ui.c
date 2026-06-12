#include "ui.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>

static char  log_buf[UI_LOG_LINES][UI_LOG_LEN];
static uint16_t log_color[UI_LOG_LINES];
static uint8_t  log_idx = 0;

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c) {
    LCD_FillRect(x, y, x + w - 1, y + h - 1, c);
}
static void draw_text(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg) {
    LCD_ShowString(x, y, (char*)s, fg, bg);
}
static void draw_dot(uint16_t x, uint16_t y, uint8_t r, uint16_t c) {
    fill_rect(x - r, y - r, r * 2 + 1, r * 2 + 1, c);
}
static void draw_hline(uint16_t y, uint16_t c) {
    fill_rect(0, y, 320, 1, c);
}

void UI_Init(void) {
    LCD_Clear(UI_WHITE);

    /* 标题栏 */
    fill_rect(0, 0, 320, UI_TOP_BAR_H, UI_TITLE_BG);
    {
        const char *t = "Face Lock  System";
        uint16_t tx = (320 - (uint16_t)strlen(t) * 8) / 2;
        draw_text(tx, 12, t, UI_WHITE, UI_TITLE_BG);
    }

    /* 状态条 */
    fill_rect(0, UI_TOP_BAR_H, 320, UI_STATUS_H, UI_BAR_BG);
    draw_hline(UI_TOP_BAR_H + UI_STATUS_H, UI_BORDER);

    /* 人脸区域 */
    draw_hline(UI_FACE_Y - 1, UI_BORDER);
    fill_rect(8, UI_FACE_Y, 304, UI_FACE_H - 2, UI_WHITE);
    draw_text(16, UI_FACE_Y + 4, "Face Detection", UI_GRAY, UI_WHITE);

    /* 日志区 */
    draw_hline(UI_LOG_Y - 1, UI_BORDER);
    fill_rect(0, UI_LOG_Y, 320, UI_LOG_H, UI_LOG_BG);
    draw_text(12, UI_LOG_Y + 4, "Event Log", UI_GRAY, UI_LOG_BG);
    draw_hline(UI_LOG_Y + 20, UI_BORDER);

    /* 按键栏 */
    draw_hline(UI_BTN_Y - 1, UI_BORDER);
    fill_rect(0, UI_BTN_Y, 320, UI_BTN_H, UI_TITLE_BG);
    {
        const char *b1 = "[PE3] Enroll";
        const char *b2 = "[PE2] Verify";
        const char *b3 = "[PA0] Delete";
        draw_text(14, UI_BTN_Y + 12, b1, UI_WHITE, UI_TITLE_BG);
        draw_text(120, UI_BTN_Y + 12, b2, UI_WHITE, UI_TITLE_BG);
        draw_text(228, UI_BTN_Y + 12, b3, UI_WHITE, UI_TITLE_BG);
    }

    /* 统计栏 */
    fill_rect(0, UI_STATS_Y, 320, UI_STATS_H, UI_BAR_BG);
    draw_hline(UI_STATS_Y, UI_BORDER);

    memset(log_buf, 0, sizeof(log_buf));
    memset(log_color, 0, sizeof(log_color));
    log_idx = 0;
}

/* ============ 状态条 (仅锁+用户) ============ */
void UI_UpdateStatus(uint8_t lock, uint8_t users) {
    uint16_t y = UI_TOP_BAR_H + 4;
    char buf[24];

    /* 清空 */
    fill_rect(0, UI_TOP_BAR_H, 320, UI_STATUS_H, UI_BAR_BG);

    /* 用户数 */
    sprintf(buf, "Users: %d", users);
    draw_text(20, y + 2, buf, UI_BLUE, UI_BAR_BG);

    /* 锁 */
    draw_dot(170, y + 12, 6, lock ? UI_GREEN : UI_RED);
    draw_text(182, y + 2, lock ? "UNLOCKED" : "LOCKED",
              lock ? UI_GREEN : UI_RED, UI_BAR_BG);

    /* 状态指示 */
    sprintf(buf, "%s", lock ? "O" : "X");
    draw_text(280, y + 2, buf, lock ? UI_GREEN : UI_RED, UI_BAR_BG);
}

/* ============ 人脸状态 ============ */
void UI_UpdateFace(int16_t state, int16_t yaw,
                   int16_t left, int16_t top,
                   int16_t right, int16_t bottom) {
    uint16_t y = UI_FACE_Y;
    char buf[48];

    fill_rect(8, y + 22, 304, UI_FACE_H - 24, UI_WHITE);

    /* 状态文字 */
    const char *ss;
    uint16_t sc;
    switch (state) {
        case 0:  ss = "DETECTED";  sc = UI_GREEN; break;
        case 1:  ss = "NO FACE";   sc = UI_RED;   break;
        case 6:  ss = "TOO FAR";   sc = UI_RED;   break;
        case 7:  ss = "TOO CLOSE"; sc = UI_RED;   break;
        default: ss = "ADJUST..."; sc = UI_RED;   break;
    }
    sprintf(buf, "Status : %s", ss);
    draw_text(16, y + 30, buf, sc, UI_WHITE);

    sprintf(buf, "Yaw    : %+d deg", yaw);
    draw_text(16, y + 52, buf, UI_BLACK, UI_WHITE);

    sprintf(buf, "Box L T: (%d, %d)", left, top);
    draw_text(16, y + 74, buf, UI_GRAY, UI_WHITE);

    sprintf(buf, "Box R B: (%d, %d)", right, bottom);
    draw_text(16, y + 96, buf, UI_GRAY, UI_WHITE);

    sprintf(buf, "Size   : %d x %d", right - left, bottom - top);
    draw_text(16, y + 118, buf, UI_GRAY, UI_WHITE);
}

/* ============ 日志 ============ */
void UI_Log(const char *msg, uint16_t color) {
    strncpy(log_buf[log_idx], msg, UI_LOG_LEN - 1);
    log_buf[log_idx][UI_LOG_LEN - 1] = '\0';
    log_color[log_idx] = color;
    log_idx = (log_idx + 1) % UI_LOG_LINES;

    uint16_t y = UI_LOG_Y + 22;
    fill_rect(4, y, 312, UI_LOG_H - 26, UI_LOG_BG);

    for (uint8_t i = 0; i < UI_LOG_LINES; i++) {
        uint8_t idx = (log_idx + i) % UI_LOG_LINES;
        if (strlen(log_buf[idx]) == 0) continue;
        draw_dot(16, y + i * 18 + 6, 3, log_color[idx]);
        draw_text(24, y + i * 18, log_buf[idx], log_color[idx], UI_LOG_BG);
    }
}

/* ============ 告警 ============ */
void UI_ShowAlarm(const char *msg) {
    fill_rect(0, UI_BTN_Y, 320, UI_BTN_H, UI_RED);
    char buf[40];
    sprintf(buf, "!! %s !!", msg);
    uint16_t tx = (320 - (uint16_t)strlen(buf) * 8) / 2;
    draw_text(tx, UI_BTN_Y + 12, buf, UI_YELLOW, UI_RED);
}

void UI_ClearAlarm(void) {
    fill_rect(0, UI_BTN_Y, 320, UI_BTN_H, UI_TITLE_BG);
    draw_text(14, UI_BTN_Y + 12, "[PE3] Enroll", UI_WHITE, UI_TITLE_BG);
    draw_text(120, UI_BTN_Y + 12, "[PE2] Verify", UI_WHITE, UI_TITLE_BG);
    draw_text(228, UI_BTN_Y + 12, "[PA0] Delete", UI_WHITE, UI_TITLE_BG);
}
