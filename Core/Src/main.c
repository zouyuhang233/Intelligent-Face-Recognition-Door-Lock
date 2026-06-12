/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : FM225 摄像头人脸识别测试程序
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"
#include "fm22x.h"
#include "esp8266.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern uint8_t fm22x_rx_byte;
extern volatile bool module_ready;
extern volatile bool command_complete;
extern volatile uint8_t last_result;
extern volatile int16_t g_face_left, g_face_top, g_face_right, g_face_bottom;
extern volatile int16_t g_face_yaw, g_face_pitch, g_face_roll;
extern volatile int16_t g_face_state;
extern volatile bool g_face_updated;
extern volatile uint32_t g_face_count;
extern volatile uint8_t g_user_count;

uint8_t  enrolling   = 0;
uint8_t  verifying   = 0;
uint32_t key_tick = 0;
uint8_t  pa0_last = 0;
uint8_t  pe3_last = 1;
uint8_t  pe2_last = 1;

uint32_t verify_ok_count = 0;
uint32_t verify_fail_count = 0;
uint32_t enroll_ok_count = 0;
uint32_t enroll_fail_count = 0;

/* WiFi 和锁状态 */
uint8_t wifi_connected = 0;
uint8_t mqtt_connected = 0;
uint32_t heartbeat_tick = 0;
uint32_t alarm_tick = 0;
uint8_t alarm_state = 0;
uint8_t lock_state = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void led_blink(uint8_t n, uint8_t fast) {
    uint32_t d = fast ? 100 : 200;
    for (uint8_t i = 0; i < n; i++) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_RESET); HAL_Delay(d);
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_SET);   HAL_Delay(d);
    }
}

static void update_status_bar(void) {
    UI_UpdateStatus(lock_state, g_user_count);
}

/* 开锁 */
static void do_unlock(void) {
    lock_state = 1;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_RESET);
    update_status_bar();
    UI_Log("Lock OPEN", UI_GREEN);
    if (mqtt_connected) {
        OneNet_ReportLock(1);
        OneNet_ReportResult(1);
    }
}

/* 关锁 */
static void do_lock(void) {
    lock_state = 0;
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_SET);
    update_status_bar();
    UI_Log("Lock CLOSE", UI_RED);
    if (mqtt_connected) OneNet_ReportLock(0);
}

/* 告警 */
static void do_alarm(const char *reason) {
    alarm_state = 1;
    alarm_tick = HAL_GetTick();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    UI_ShowAlarm(reason);
    UI_Log("ALARM!", UI_RED);
    HAL_Delay(300);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    alarm_state = 0;
}

/* 停止告警 */
static void stop_alarm(void) {
    alarm_state = 0;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
    UI_ClearAlarm();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  SystemClock_Config();
  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_FSMC_Init();
  /* USER CODE BEGIN 2 */
  LCD_Init();
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(300);

  /* 初始化 UI */
  UI_Init();

  pa0_last = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
  pe3_last = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3);
  pe2_last = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2);

  /* 初始化摄像头 */
  UI_Log("Init camera...", UI_GRAY);
  FM22x_Init();
  HAL_UART_Receive_IT(&huart2, &fm22x_rx_byte, 1);

  {
      uint32_t t0 = HAL_GetTick();
      while ((HAL_GetTick() - t0) < 10000) {
          if (module_ready) {
              UI_Log("Camera READY!", UI_GREEN);
              led_blink(2, 1);
              break;
          }
          HAL_Delay(10);
      }
  }

  if (!module_ready) {
      UI_Log("Camera FAIL!", UI_RED);
  }

  /* 初始化 ESP8266 + 连接 OneNet */
  UI_Log("Init WiFi...", UI_GRAY);
  ESP8266_Init();

  UI_Log("Connect MQTT...", UI_GRAY);
  if (OneNet_Connect()) {
      mqtt_connected = 1;
      HAL_Delay(1000);
      UI_Log("MQTT Connected!", UI_GREEN);
  } else {
      UI_Log("MQTT Failed!", UI_RED);
  }

  /* 获取用户列表 */
  FM22x_GetAllUser();
  HAL_Delay(500);
  update_status_bar();

  /* 开机上报 arr_id */
  heartbeat_tick = HAL_GetTick();
  if (mqtt_connected) {
      if (OneNet_ReportUserIDArray((uint16_t*)g_user_ids, g_user_id_count)) {
          UI_Log("arr_id synced", UI_GREEN);
      } else {
          UI_Log("arr_id fail!", UI_RED);
      }
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ---- 人脸状态显示 ---- */
    if (g_face_updated) {
        g_face_updated = false;
        int16_t yaw_deg = g_face_yaw / 10;
        UI_UpdateFace(g_face_state, yaw_deg,
            g_face_left, g_face_top, g_face_right, g_face_bottom);
    }

    /* ---- 命令结果处理 ---- */
    if (command_complete) {
        command_complete = false;
        uint8_t r = last_result;

        if (enrolling) {
            enrolling = 0;
            if (r == 0) {
                UI_Log("Enroll SUCCESS!", UI_GREEN);
                led_blink(3, 0);
                enroll_ok_count++;
                FM22x_GetAllUser();
                HAL_Delay(300);
                update_status_bar();
                if (mqtt_connected) OneNet_ReportUserIDArray((uint16_t*)g_user_ids, g_user_id_count);
            } else {
                char b[40];
                sprintf(b, "Enroll FAIL: %d", r);
                UI_Log(b, UI_RED);
                led_blink(1, 1);
                enroll_fail_count++;
            }
        }
        else if (verifying) {
            verifying = 0;
            if (r == 0) {
                UI_Log("Verify SUCCESS!", UI_GREEN);
                led_blink(3, 0);
                verify_ok_count++;
                do_unlock();
            } else {
                char b[40];
                sprintf(b, "Verify FAIL: %d", r);
                UI_Log(b, UI_RED);
                led_blink(1, 1);
                verify_fail_count++;
                do_alarm("Stranger!");
                if (mqtt_connected) OneNet_ReportResult(0);
            }
        }
    }

    /* ---- 自动停止告警 (2秒) ---- */
    if (alarm_state && (HAL_GetTick() - alarm_tick > 2000)) {
        stop_alarm();
    }

    /* ---- 检查云端下发指令 ---- */
    if (mqtt_connected) {
        char cmd_id[32] = {0};
        int8_t cloud_cmd = OneNet_CheckCommand(cmd_id);

        if (cloud_cmd == 0 && lock_state == 1) {
            do_lock();
            if (cmd_id[0]) OneNet_SendSetReply(cmd_id);
        } else if (cloud_cmd == 1 && lock_state == 0) {
            do_unlock();
            if (cmd_id[0]) OneNet_SendSetReply(cmd_id);
        } else if (cloud_cmd == 2) {
            UI_Log("Cloud: Verify...", UI_CYAN);
            verifying = 1;
            FM22x_Verify(1, 10);
            if (cmd_id[0]) OneNet_SendSetReply(cmd_id);
        } else if (cloud_cmd == 3) {
            UI_Log("Cloud: Enroll...", UI_CYAN);
            enrolling = 1;
            FM22x_Enroll(0, 0x1F, 30);
            if (cmd_id[0]) OneNet_SendSetReply(cmd_id);
        } else if (cloud_cmd == 4) {
            UI_Log("Cloud: Delete All", UI_RED);
            FM22x_DeleteAll();
            HAL_Delay(500);
            FM22x_GetAllUser();
            HAL_Delay(300);
            update_status_bar();
            if (mqtt_connected) OneNet_ReportUserIDArray((uint16_t*)g_user_ids, g_user_id_count);
            if (cmd_id[0]) OneNet_SendSetReply(cmd_id);
        } else if (cloud_cmd == 6) {
            UI_Log("Cloud: arr_id clear", UI_RED);
            FM22x_DeleteAll();
            HAL_Delay(500);
            FM22x_GetAllUser();
            HAL_Delay(300);
            update_status_bar();
            if (mqtt_connected) OneNet_ReportUserIDArray((uint16_t*)g_user_ids, g_user_id_count);
            if (cmd_id[0]) OneNet_SendSetReply(cmd_id);
        } else if (cloud_cmd == 5) {
            UI_Log("Cloud: ALARM!", UI_RED);
            for (int i = 0; i < 3; i++) {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
                HAL_Delay(500);
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
                HAL_Delay(200);
            }
            if (cmd_id[0]) OneNet_SendSetReply(cmd_id);
        }
    }

    /* ---- 心跳上报 (30秒) ---- */
    if (mqtt_connected && (HAL_GetTick() - heartbeat_tick > 30000)) {
        heartbeat_tick = HAL_GetTick();
        OneNet_ReportLock(lock_state);
        OneNet_ReportUserNum(g_user_count);
    }

    /* ---- 按键处理 ---- */
    uint8_t key = 0;
    uint8_t pa0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    if (pa0 == 1 && pa0_last == 0) key = 1; pa0_last = pa0;
    uint8_t pe3 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3);
    if (pe3 == 0 && pe3_last == 1) key = 2; pe3_last = pe3;
    uint8_t pe2 = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2);
    if (pe2 == 0 && pe2_last == 1) key = 3; pe2_last = pe2;

    if (key && (HAL_GetTick() - key_tick > 300)) {
        key_tick = HAL_GetTick();

        if (key == 1) {
            UI_Log("Delete All Users...", UI_RED);
            FM22x_DeleteAll();
            HAL_Delay(500);
            FM22x_GetAllUser();
            HAL_Delay(300);
            update_status_bar();
            if (mqtt_connected) OneNet_ReportUserIDArray((uint16_t*)g_user_ids, g_user_id_count);
        }
        else if (key == 2) {
            enrolling = 1;
            UI_Log("Enrolling...", UI_CYAN);
            FM22x_Enroll(0, 0x1F, 30);
        }
        else if (key == 3) {
            verifying = 1;
            UI_Log("Verifying...", UI_CYAN);
            FM22x_Verify(1, 10);
        }
    }

    HAL_Delay(100);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
