# 人脸识别门禁系统 (Face Recognition Door Lock)

基于 STM32F103ZET6 + FM225 摄像头 + ESP8266 WiFi 的智能人脸识别门禁系统，支持本地控制和 OneNet 云平台远程管理。

## 硬件配置

| 模块 | 型号 | 接口 |
|------|------|------|
| 主控 | STM32F103ZET6 | — |
| 摄像头 | FM225 (THFaceCropper) | USART2 (PA2/PA3) |
| WiFi | ESP8266-01S | USART3 (PB10/PB11) |
| LCD | ST7796 320×480 | FSMC (SRAM模式) |
| 电磁锁 | 12V 继电器模块 | PC3 |
| 蜂鸣器 | 有源蜂鸣器 | PB8 |
| LED | 状态指示灯 | PE5 |

## 引脚占用

| 外设 | 引脚 | 说明 |
|------|------|------|
| USART2 TX/RX | PA2/PA3 | FM225 摄像头 |
| USART3 TX/RX | PB10/PB11 | ESP8266 WiFi |
| FSMC LCD | PD0-PD15, PE7-PE15 | LCD 并口 |
| LCD BL | PB0 | 背光控制 |
| 电磁锁 | PC3 | 高电平开锁 |
| LED | PE5 | 低电平亮 |
| 蜂鸣器 | PB8 | 高电平响 |
| 按键 PA0 | PA0 | 删除所有用户 |
| 按键 PE2 | PE2 | 人脸验证 |
| 按键 PE3 | PE3 | 人脸录入 |

## 功能特性

### 本地控制
- **人脸录入**: PE3 按键触发，多角度采集（上/下/左/右/中）
- **人脸验证**: PE2 按键触发，识别成功自动开锁
- **删除用户**: PA0 按键清空所有已录入用户
- **陌生人告警**: 验证失败触发蜂鸣器报警

### 云端控制 (OneNet MQTT)
- **LOCK**: 远程开锁/关锁
- **OPEN_JC**: 远程触发人脸验证
- **OPEN_LR**: 远程触发人脸录入
- **Clear_user**: 远程删除所有用户
- **YC_BJ**: 远程报警
- **arr_id**: 用户 ID 数组同步

### 数据上报
| 属性 | 类型 | 说明 |
|------|------|------|
| LOCK | bool | 锁状态 (true=开/false=关) |
| EERNUM | bool | 识别结果 (true=成功/false=失败) |
| USER_NUM | int | 已录入用户数量 |
| arr_id | string | 用户 ID 列表 (逗号分隔) |

## OneNet 云平台配置

- **服务器**: `mqtts.heclouds.com:1883`
- **产品ID**: `u9gXgMhYR1`
- **设备名**: `onenet_one`
- **认证**: MD5 Token 方式

### MQTT Topic
```
发布: $sys/u9gXgMhYR1/onenet_one/thing/property/post
订阅: $sys/u9gXgMhYR1/onenet_one/thing/property/post/reply
订阅: $sys/u9gXgMhYR1/onenet_one/thing/property/set
回复: $sys/u9gXgMhYR1/onenet_one/thing/property/set_reply
```

## 固件结构

```
Core/
├── Inc/
│   ├── main.h
│   ├── fm22x.h          # FM225 摄像头驱动
│   ├── esp8266.h        # ESP8266 WiFi/MQTT
│   ├── lcd.h            # LCD 驱动 (FSMC)
│   ├── ui.h             # UI 界面
│   ├── usart.h          # 串口配置
│   ├── gpio.h           # GPIO 配置
│   └── fsmc.h           # FSMC 配置
└── Src/
    ├── main.c           # 主程序
    ├── fm22x.c          # 摄像头协议解析
    ├── esp8266.c        # WiFi AT 指令 / MQTT
    ├── lcd.c            # LCD 底层驱动
    ├── ui.c             # UI 绘制
    ├── usart.c          # 串口初始化
    ├── gpio.c           # GPIO 初始化
    └── fsmc.c           # FSMC 初始化
```

## FM225 摄像头协议

### 帧格式
```
EF AA [MSGID] [SIZE_H] [SIZE_L] [DATA...] [PARITY]
```

### 主要指令
| 指令 | MSGID | 说明 |
|------|-------|------|
| RESET | 0x10 | 复位模块 |
| VERIFY | 0x12 | 1:N 人脸验证 |
| ENROLL | 0x13 | 交互式录入 |
| ENROLL_SINGLE | 0x1D | 单帧录入 |
| DELALL | 0x21 | 删除全部用户 |
| GET_ALL_USER | 0x24 | 查询用户数量及列表 |
| GET_VERSION | 0x30 | 获取固件版本 |

### 人脸状态 (NOTE)
| 值 | 状态 |
|----|------|
| 0 | 正常检测到人脸 |
| 1 | 无人脸 |
| 2-5 | 人脸偏移 (上/下/左/右) |
| 6-7 | 距离异常 (远/近) |
| 8-10 | 遮挡 (眉/眼/脸) |

## 编译与烧录

- **IDE**: Keil MDK-ARM V5
- **编译器**: ARM Compiler V5/V6
- **烧录工具**: ST-Link / J-Link (SWD)
- **工程文件**: `MDK-ARM/rlsb.uvprojx`

## UI 布局

```
┌──────────────────────────┐
│    Face Lock  System     │  标题栏 (深蓝)
├──────────────────────────┤
│ Users: 3        LOCKED   │  状态条 (浅蓝)
├──────────────────────────┤
│ Face Detection           │
│ Status : DETECTED        │  人脸检测区域
│ Yaw    : +5 deg          │  (白色背景)
│ Box L T: (50, 30)        │
│ Box R B: (180, 200)      │
│ Size   : 130 x 170       │
├──────────────────────────┤
│ Event Log                │
│ > Camera READY!          │  事件日志
│ > MQTT Connected!        │  (5行滚动)
│ > Verify SUCCESS!        │
├──────────────────────────┤
│ [PE3]Enroll [PE2]Verify [PA0]Delete │  按键/告警栏
├──────────────────────────┤
│          统计栏           │
└──────────────────────────┘
```

## 操作说明

1. 上电后自动初始化摄像头、WiFi、连接 OneNet
2. LCD 显示系统状态和操作提示
3. 按 **PE3** 录入人脸，按提示转动头部完成多角度采集
4. 按 **PE2** 进行人脸验证，识别成功自动开锁
5. 按 **PA0** 删除所有已录入用户
6. 云端可通过 OneNet 平台远程控制所有功能
7. 心跳每 30 秒上报一次锁状态和用户数量
8. 用户 ID 数组在录入/删除时实时同步到云端

## 版本

v2.0 — 2026-06-12
