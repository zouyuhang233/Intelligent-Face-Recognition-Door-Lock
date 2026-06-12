#include "fm22x.h"
#include "usart.h"
#include <string.h>
#include "lcd.h"

uint8_t fm22x_rx_byte = 0;

/* 接收状态机 */
static uint8_t rx_state = 0;
static uint8_t rx_parity = 0;
static uint8_t rx_msgid = 0;
static uint16_t rx_size = 0;
static uint16_t rx_index = 0;
static uint16_t rx_data_idx = 0;
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_data_buffer[1024];

/* 状态标志 */
volatile bool module_ready = false;
volatile bool command_complete = false;
volatile uint8_t last_result = 0xFF;

/* 状态机状态 */
#define STATE_WAIT_SYNC0    0
#define STATE_WAIT_SYNC1    1
#define STATE_WAIT_MSGID    2
#define STATE_WAIT_SIZE_H   3
#define STATE_WAIT_SIZE_L   4
#define STATE_WAIT_DATA     5
#define STATE_WAIT_PARITY   6

/* ============ 校验计算 ============ */
uint8_t FM22x_CalculateParity(uint8_t* data, uint16_t len)
{
    uint8_t parity = 0;
    for(uint16_t i = 0; i < len; i++)
        parity ^= data[i];
    return parity;
}

/* ============ 发送命令 ============ */
void FM22x_SendCommand(uint8_t* data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, data, len, 100);
}

/* ============ 复位 ============ */
void FM22x_Reset(void)
{
    uint8_t cmd[] = {0xEF, 0xAA, MID_RESET, 0x00, 0x00, 0x00};
    cmd[5] = FM22x_CalculateParity(&cmd[2], 3);
    FM22x_SendCommand(cmd, sizeof(cmd));
}

/* ============ 获取状态 ============ */
void FM22x_GetStatus(void)
{
    uint8_t cmd[] = {0xEF, 0xAA, MID_GETSTATUS, 0x00, 0x00, 0x00};
    cmd[5] = FM22x_CalculateParity(&cmd[2], 3);
    FM22x_SendCommand(cmd, sizeof(cmd));
}

/* ============ 验证 ============ */
void FM22x_Verify(uint8_t pd_rightaway, uint8_t timeout)
{
    uint8_t cmd[] = {0xEF, 0xAA, MID_VERIFY, 0x00, 0x02, pd_rightaway, timeout, 0x00};
    cmd[7] = FM22x_CalculateParity(&cmd[2], 5);
    FM22x_SendCommand(cmd, sizeof(cmd));
}

/* ============ 单帧录入（与官方 FM225 完全一致） ============ */
void FM22x_EnrollSingle(uint8_t admin, uint8_t face_dir, uint8_t timeout)
{
    uint8_t cmd[6 + USER_NAME_SIZE + 2];  // 6 header + 32 name + 2 params = 40
    uint16_t idx = 0;

    cmd[idx++] = 0xEF;
    cmd[idx++] = 0xAA;
    cmd[idx++] = MID_ENROLL_SINGLE;
    cmd[idx++] = 0x00;
    cmd[idx++] = 0x00;

    uint16_t data_size = 1 + USER_NAME_SIZE + 2;  // 35
    cmd[3] = (data_size >> 8) & 0xFF;
    cmd[4] = data_size & 0xFF;

    cmd[idx++] = admin;

    for(uint8_t i = 0; i < USER_NAME_SIZE; i++)
        cmd[idx++] = 0x00;

    cmd[idx++] = face_dir;
    cmd[idx++] = timeout;

    uint8_t parity = FM22x_CalculateParity(&cmd[2], idx - 2);
    cmd[idx++] = parity;

    FM22x_SendCommand(cmd, idx);
}

/* ============ 交互式录入（标准方法，多角度采集） ============ */
void FM22x_Enroll(uint8_t admin, uint8_t face_dir, uint8_t timeout)
{
    uint8_t cmd[6 + USER_NAME_SIZE + 2];  // 40 bytes
    uint16_t idx = 0;

    cmd[idx++] = 0xEF;
    cmd[idx++] = 0xAA;
    cmd[idx++] = MID_ENROLL;
    cmd[idx++] = 0x00;
    cmd[idx++] = 0x00;

    uint16_t data_size = 1 + USER_NAME_SIZE + 2;  // 35
    cmd[3] = (data_size >> 8) & 0xFF;
    cmd[4] = data_size & 0xFF;

    cmd[idx++] = admin;
    for(uint8_t i = 0; i < USER_NAME_SIZE; i++)
        cmd[idx++] = 0x00;
    cmd[idx++] = face_dir;
    cmd[idx++] = timeout;

    uint8_t parity = FM22x_CalculateParity(&cmd[2], idx - 2);
    cmd[idx++] = parity;

    FM22x_SendCommand(cmd, idx);
}

/* ============ 清除录入状态 ============ */
void FM22x_FaceReset(void)
{
    uint8_t cmd[] = {0xEF, 0xAA, 0x23, 0x00, 0x00, 0x00};
    cmd[5] = FM22x_CalculateParity(&cmd[2], 3);
    FM22x_SendCommand(cmd, sizeof(cmd));
}

/* ============ 获取所有已注册用户 ============ */
void FM22x_GetAllUser(void)
{
    uint8_t cmd[] = {0xEF, 0xAA, 0x24, 0x00, 0x00, 0x00};
    cmd[5] = FM22x_CalculateParity(&cmd[2], 3);
    FM22x_SendCommand(cmd, sizeof(cmd));
}

/* ============ 删除所有用户 ============ */
void FM22x_DeleteAll(void)
{
    uint8_t cmd[] = {0xEF, 0xAA, MID_DELALL, 0x00, 0x00, 0x00};
    cmd[5] = FM22x_CalculateParity(&cmd[2], 3);
    FM22x_SendCommand(cmd, sizeof(cmd));
}

/* ============ 获取版本 ============ */
void FM22x_GetVersion(void)
{
    uint8_t cmd[] = {0xEF, 0xAA, MID_GET_VERSION, 0x00, 0x00, 0x00};
    cmd[5] = FM22x_CalculateParity(&cmd[2], 3);
    FM22x_SendCommand(cmd, sizeof(cmd));
}

/* 最新人脸状态（供 main 循环 LCD 显示，须在 ParseFaceState 前定义） */
volatile int16_t g_face_left, g_face_top, g_face_right, g_face_bottom;
volatile int16_t g_face_yaw, g_face_pitch, g_face_roll;
volatile int16_t g_face_state;
volatile bool g_face_updated = false;
volatile uint32_t g_face_count = 0;
volatile uint16_t g_last_uid = 0;
volatile uint8_t g_user_count = 0;
volatile uint16_t g_user_ids[50] = {0};
volatile uint8_t  g_user_id_count = 0;

/* ============ 解析人脸状态 ============ */
static void ParseFaceState(uint8_t* data, uint16_t size)
{
    if(size < 17) return;

    /* 实际固件的字段顺序（与官方样例代码一致，已验证数据自洽）：
       left(int16) top(int16) right(int16) bottom(int16)
       yaw(int16) pitch(int16) roll(int16) state(uint8) spare(uint8)
       全部小端序 */
    int16_t left   = (int16_t)(data[1] | (data[2] << 8));
    int16_t top    = (int16_t)(data[3] | (data[4] << 8));
    int16_t right  = (int16_t)(data[5] | (data[6] << 8));
    int16_t bottom = (int16_t)(data[7] | (data[8] << 8));
    int16_t yaw    = (int16_t)(data[9] | (data[10] << 8));
    int16_t pitch  = (int16_t)(data[11] | (data[12] << 8));
    int16_t roll   = (int16_t)(data[13] | (data[14] << 8));
    int16_t state  = data[15];  /* 实际是 1 字节 */

    /* 存储最新人脸状态，供 main 循环 LCD 显示 */
    g_face_left   = left;
    g_face_top    = top;
    g_face_right  = right;
    g_face_bottom = bottom;
    g_face_yaw    = yaw;
    g_face_pitch  = pitch;
    g_face_roll   = roll;
    g_face_state  = state;
    g_face_updated = true;
    g_face_count++;
}

extern uint8_t lock_state;
uint8_t beep_state = 0;

/* ============ 解析结果码 ============ */
static void ParseResult(uint8_t result)
{
    if (result == MR_SUCCESS)
        lock_state = 1;
    else if (result == MR_FAILED4_TIMEOUT)
        beep_state = 1;
}

uint8_t lock_beep_state = 0;

/* ============ 处理接收到的完整帧（在中断回调中调用） ============ */
static void ProcessReceivedFrame(uint8_t msgid, uint16_t size, uint8_t* data)
{
    switch(msgid)
    {
        case MID_REPLY:
            if(size >= 2)
            {
                uint8_t cmd = data[0];
                uint8_t result = data[1];
                last_result = result;
                command_complete = true;

                if(cmd == MID_VERIFY)
                    lock_beep_state = 1;

                ParseResult(result);

                /* 存储 user ID */
                if((cmd == MID_ENROLL || cmd == MID_ENROLL_SINGLE || cmd == MID_VERIFY)
                    && result == MR_SUCCESS && size >= 4)
                {
                    g_last_uid = ((uint16_t)data[2] << 8) | data[3];
                    if (cmd == MID_ENROLL || cmd == MID_ENROLL_SINGLE) {
                        uint8_t found = 0;
                        for (uint8_t i = 0; i < g_user_id_count; i++) {
                            if (g_user_ids[i] == g_last_uid) { found = 1; break; }
                        }
                        if (!found && g_user_id_count < 50) {
                            g_user_ids[g_user_id_count++] = g_last_uid;
                        }
                    }
                }

                /* 获取用户数量 + 列表 */
                if(cmd == 0x24 && result == MR_SUCCESS && size >= 3)
                {
                    g_user_count = data[2];
                    if (size >= (uint16_t)(3 + g_user_count * 2)) {
                        g_user_id_count = 0;
                        for (uint8_t i = 0; i < g_user_count && i < 50; i++) {
                            g_user_ids[i] = ((uint16_t)data[3 + i*2] << 8)
                                          | data[4 + i*2];
                            g_user_id_count++;
                        }
                    }
                }

                /* 删除全部：清空数组 */
                if(cmd == MID_DELALL && result == MR_SUCCESS)
                {
                    g_user_id_count = 0;
                    memset((void*)g_user_ids, 0, sizeof(g_user_ids));
                }
            }
            break;

        case MID_NOTE:
            if(size >= 1)
            {
                uint8_t nid = data[0];
                if(nid == 0x00)
                    module_ready = true;
                else if(nid == 0x01)
                    ParseFaceState(data, size);
            }
            break;

        default:
            break;
    }
}

/* ============ 初始化 ============ */
void FM22x_Init(void)
{
    module_ready = false;
    command_complete = false;
    last_result = 0xFF;
    rx_state = STATE_WAIT_SYNC0;
    rx_index = 0;
    rx_data_idx = 0;
    rx_size = 0;

    /* Init done, waiting for module_ready flag */
}

/* ============ UART 接收回调（状态机，在中断中调用） ============ */
void FM22x_UART_Callback(uint8_t byte)
{
    switch(rx_state)
    {
        case STATE_WAIT_SYNC0:
            if(byte == 0xEF)
            {
                rx_state = STATE_WAIT_SYNC1;
                rx_parity = 0;
                rx_index = 0;
                rx_data_idx = 0;
                rx_buffer[rx_index++] = byte;
            }
            break;

        case STATE_WAIT_SYNC1:
            if(byte == 0xAA)
            {
                rx_state = STATE_WAIT_MSGID;
                rx_buffer[rx_index++] = byte;
            }
            else
            {
                rx_state = STATE_WAIT_SYNC0;
                rx_parity = 0;
                rx_index = 0;
                rx_data_idx = 0;
            }
            break;

        case STATE_WAIT_MSGID:
            rx_msgid = byte;
            rx_parity ^= byte;
            rx_buffer[rx_index++] = byte;
            rx_state = STATE_WAIT_SIZE_H;
            break;

        case STATE_WAIT_SIZE_H:
            rx_size = ((uint16_t)byte << 8);
            rx_parity ^= byte;
            rx_buffer[rx_index++] = byte;
            rx_state = STATE_WAIT_SIZE_L;
            break;

        case STATE_WAIT_SIZE_L:
            rx_size |= byte;
            rx_parity ^= byte;
            rx_buffer[rx_index++] = byte;

            if(rx_size == 0)
                rx_state = STATE_WAIT_PARITY;
            else
            {
                rx_state = STATE_WAIT_DATA;
                rx_data_idx = 0;
            }
            break;

        case STATE_WAIT_DATA:
            rx_data_buffer[rx_data_idx++] = byte;
            rx_parity ^= byte;
            rx_buffer[rx_index++] = byte;

            if(rx_data_idx >= rx_size)
                rx_state = STATE_WAIT_PARITY;
            break;

        case STATE_WAIT_PARITY:
            rx_buffer[rx_index++] = byte;

            if(byte == rx_parity)
            {
                ProcessReceivedFrame(rx_msgid, rx_size, rx_data_buffer);
            }

            rx_state = STATE_WAIT_SYNC0;
            break;

        default:
            rx_state = STATE_WAIT_SYNC0;
            break;
    }
}
