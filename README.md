# 人脸识别智能锁 — 网页控制面板

东华理工大学 邹宇航

## 项目结构

```
人脸识别项目网页端/
├── index.html          # 控制面板网页（前端）
├── server.js           # Node.js 服务器（HTTP代理 + MQTT + WebSocket + SQLite）
├── proxy-server.js     # 旧版纯 HTTP 代理（已弃用）
├── lock_data.db        # SQLite 数据库（自动生成）
├── package.json        # Node.js 依赖
├── node_modules/       # npm 包
└── README.md           # 本文档
```

---

## 快速启动

```bash
# 1. 安装依赖（只需一次）
npm install mqtt ws better-sqlite3

# 2. 启动服务器
node server.js

# 3. 浏览器打开 index.html
```

---

## 架构

```
┌─────────────┐     HTTP (localhost:3000)     ┌──────────────┐
│   浏览器     │ ──────────────────────────── │              │
│  index.html  │                              │  server.js   │
│              │ ◄── WebSocket (port:3001) ── │              │
└─────────────┘     实时推送属性变更          └──────┬───────┘
                                                     │
                                    ┌────────────────┼────────────────┐
                                    │ HTTP 代理       │ MQTT 订阅      │
                                    ▼                ▼                ▼
                              OneNET REST API   OneNET MQTT Broker
                              (设置属性/查询)   (实时接收设备上报)
                                    │                │
                                    └────────┬───────┘
                                             ▼
                                      ┌─────────────┐
                                      │  STM32 设备  │
                                      │  人脸识别锁  │
                                      └─────────────┘
```

**数据流**：
- **控制指令**：浏览器 → HTTP API → OneNET → 设备
- **实时状态**：设备 → MQTT → server.js → WebSocket → 浏览器
- **轮询兜底**：浏览器每 3 秒 HTTP 查询一次（MQTT 不通时）
- **数据存储**：server.js → SQLite（属性变更 + 操作日志）

---

## 配置参数

| 参数 | 说明 | 来源 |
|------|------|------|
| Access Token | 平台访问令牌 | OneNET 控制台 → 产品管理 → API 密钥 |
| Product ID | 产品标识 | OneNET 控制台 → 产品管理 → 产品信息 |
| Device Name | 设备名称 | OneNET 控制台 → 设备管理 |

配置保存在浏览器 localStorage（key: `onenet_lock_cfg`）。

---

## 服务端口

| 端口 | 协议 | 用途 |
|------|------|------|
| 3000 | HTTP | API 代理（浏览器 → OneNET） |
| 3001 | WebSocket | 实时推送（server → 浏览器） |

---

## API 接口

### 1. 查询设备详情（在线状态）

```
GET /device/detail?product_id={pid}&device_name={dn}
Authorization: {access_token}
```

返回 `status: 1` = 在线，`0` = 离线。

### 2. 查询设备属性

```
GET /thingmodel/query-device-property?product_id={pid}&device_name={dn}
Authorization: {access_token}
```

返回 `data` 为属性数组。

### 3. 设置设备属性

```
POST /thingmodel/set-device-property
Authorization: {access_token}
Content-Type: application/json

{
  "product_id": "xxx",
  "device_name": "xxx",
  "params": { "LOCK": false }
}
```

响应码：`0` = 成功，`10403` = Token 无效，`10411` = 设备超时（指令已到平台）

### 4. 查询历史数据

```
GET /api/history?limit=50&identifier=LOCK
```

---

## WebSocket 消息协议

### 浏览器 → 服务端

| type | 说明 | 参数 |
|------|------|------|
| `connect` | 连接 MQTT | productId, deviceName, token |
| `set_property` | 设置属性 | identifier, value |
| `get_property` | 查询属性 | - |
| `get_history` | 查询历史 | limit, identifier |
| `disconnect` | 断开 MQTT | - |

### 服务端 → 浏览器

| type | 说明 |
|------|------|
| `mqtt_status` | MQTT 连接状态变更 |
| `property_update` | 属性实时更新（设备上报） |
| `set_reply` | 设置属性响应 |
| `connect_result` | MQTT 连接结果 |
| `history` | 历史数据 |
| `error` | 错误信息 |

---

## 页面功能

### Tab 页

| Tab | 功能 |
|-----|------|
| 概览 | 统计卡片 + 锁控制 + 设备功能 + 识别记录 |
| 配置 | Token / Product ID / Device Name 配置 |
| 日志 | 操作日志（内存） |
| 历史数据 | 数据库中的属性变更历史 + 操作日志 + 实时数据流 |

### 实时更新机制

1. **主通道**：3 秒轮询 HTTP API（可靠）
2. **增强通道**：MQTT → WebSocket 实时推送（毫秒级，依赖 MQTT 订阅成功）
3. 两种通道并行，互不影响

---

## ⚠️ 容易出错的点

### 1. 锁逻辑是反的

**上锁发送 `false`，解锁发送 `true`**

```javascript
// snd() 函数
params: { [key]: val }
// 按钮: 上锁 → val=false，解锁 → val=true
```

### 2. 锁状态显示

**`false` = 已上锁 🔒，`true` = 已解锁 🔓**

### 3. 属性值是字符串

OneNET 返回的 value 是字符串 `"true"` / `"false"`，不是布尔。代码中做了类型兼容。

### 4. MQTT client_id 冲突

服务器连接 MQTT 时用 `{deviceName}_web` 作为 client_id，避免和设备 `{deviceName}` 冲突。如果用相同的 client_id，会互相踢下线。

### 5. code: 10411 不代表失败

`code: 10411` 是「设备响应超时」，指令已经发到平台了，设备可能已经执行了，只是没回复确认。

### 6. CORS 跨域

浏览器不能直接请求 `iot-api.heclouds.com`，必须通过 server.js 代理。

### 7. API 域名

数据接口是 `iot-api.heclouds.com`，不是 `open.iot.10086.cn`。

### 8. WebSocket 断开不影响控制

WebSocket 断开时，`conned` 状态不变。控制指令走 HTTP API，状态刷新靠 3 秒轮询。WebSocket 只用于实时推送增强。

---

## 常见问题

| 现象 | 原因 | 解决 |
|------|------|------|
| Failed to fetch | 服务没启动 | `node server.js` |
| Token 无效 | Token 错误 | 重新获取 |
| 设备离线 | 设备没连平台 | 检查设备电源和网络 |
| code 10411 | 设备超时 | 检查设备 MQTT，指令可能已到 |
| 属性值没变 | 设备没执行 | 检查 STM32 代码 |
| 上锁变解锁 | 逻辑反了 | 检查 `snd()` 和 `updatePropUI()` |
| 端口被占用 | 上次没关 | `taskkill /F /IM node.exe` |
| MQTT 订阅失败 | client_id 冲突 | 服务器用 `_web` 后缀，不影响 |
| 实时数据不更新 | MQTT 不通 | 轮询兜底，3 秒自动刷新 |

---

## 数据库

SQLite 文件：`lock_data.db`（自动生成）

### 表结构

**property_changes** — 属性变更记录

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 自增主键 |
| identifier | TEXT | 属性标识（LOCK, OPEN_JC 等） |
| value | TEXT | 属性值 |
| data_type | TEXT | 数据类型 |
| timestamp | DATETIME | 时间戳 |
| source | TEXT | 来源（device/user/poll） |

**operation_logs** — 操作日志

| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 自增主键 |
| action | TEXT | 操作类型 |
| detail | TEXT | 详情 |
| result | TEXT | 结果 |
| timestamp | DATETIME | 时间戳 |

---

## localStorage keys

| key | 内容 |
|-----|------|
| `onenet_lock_cfg` | 配置（accessToken, productId, deviceName） |
| `onenet_state` | 属性状态缓存 |

---

## 依赖

| 包 | 用途 |
|----|------|
| `mqtt` | MQTT 客户端（订阅设备上报） |
| `ws` | WebSocket 服务端（推送实时数据） |
| `better-sqlite3` | SQLite 数据库 |
