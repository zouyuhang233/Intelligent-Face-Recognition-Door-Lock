#ifndef __FM22X_H__
#define __FM22X_H__

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* 消息ID定义 */
#define MID_REPLY                   0x00
#define MID_NOTE                    0x01
#define MID_IMAGE                   0x02
#define MID_RESET                   0x10
#define MID_GETSTATUS               0x11
#define MID_VERIFY                  0x12
#define MID_ENROLL                  0x13
#define MID_DELUSER                 0x20
#define MID_DELALL                  0x21
#define MID_GETUSERINFO             0x22
#define MID_ENROLL_SINGLE           0x1D
#define MID_GET_VERSION             0x30

/* 人脸方向定义 */
#define FACE_DIRECTION_UP           0x10
#define FACE_DIRECTION_DOWN         0x08
#define FACE_DIRECTION_LEFT         0x04
#define FACE_DIRECTION_RIGHT        0x02
#define FACE_DIRECTION_MIDDLE       0x01

/* 人脸状态定义 */
#define FACE_STATE_NORMAL           0
#define FACE_STATE_NOFACE           1
#define FACE_STATE_TOOUP            2
#define FACE_STATE_TOODOWN          3
#define FACE_STATE_TOOLEFT          4
#define FACE_STATE_TOORIGHT         5
#define FACE_STATE_FAR              6
#define FACE_STATE_CLOSE            7
#define FACE_STATE_EYEBROW_OCCLUSION 8
#define FACE_STATE_EYE_OCCLUSION    9
#define FACE_STATE_FACE_OCCLUSION   10
#define FACE_STATE_DIRECTION_ERROR  11

/* 结果码定义 */
#define MR_SUCCESS                  0x00
#define MR_REJECTED                 0x01
#define MR_ABORTED                  0x02
#define MR_FAILED4_CAMERA           0x04
#define MR_FAILED4_UNKNOWNREASON    0x05
#define MR_FAILED4_INVALIDPARAM     0x06
#define MR_FAILED4_NOMEMORY         0x07
#define MR_FAILED4_UNKNOWNUSER      0x08
#define MR_FAILED4_MAXUSER          0x09
#define MR_FAILED4_FACEENROLLED     0x0A
#define MR_FAILED4_LIVENESSCHECK    0x0C
#define MR_FAILED4_TIMEOUT          0x0D

/* 缓冲区大小 */
#define RX_BUFFER_SIZE              512
#define USER_NAME_SIZE              32

/* 函数声明 */
void FM22x_Init(void);
void FM22x_SendCommand(uint8_t* data, uint16_t len);
uint8_t FM22x_CalculateParity(uint8_t* data, uint16_t len);
void FM22x_Reset(void);
void FM22x_GetStatus(void);
void FM22x_Verify(uint8_t pd_rightaway, uint8_t timeout);
void FM22x_EnrollSingle(uint8_t admin, uint8_t face_dir, uint8_t timeout);
void FM22x_Enroll(uint8_t admin, uint8_t face_dir, uint8_t timeout);
void FM22x_FaceReset(void);
void FM22x_GetAllUser(void);
void FM22x_DeleteAll(void);
void FM22x_GetVersion(void);
void FM22x_UART_Callback(uint8_t byte);

/* 状态标志 */
extern volatile bool module_ready;
extern volatile bool command_complete;
extern volatile uint8_t last_result;
extern uint8_t fm22x_rx_byte;
extern uint8_t lock_state;
extern uint8_t beep_state;
extern uint8_t lock_beep_state;

#endif
