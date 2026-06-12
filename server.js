/**
 * 人脸识别智能锁 - 实时服务器
 * 功能：HTTP代理 + MQTT订阅 + WebSocket推送 + SQLite存储
 */
const http = require('http');
const https = require('https');
const mqtt = require('mqtt');
const { WebSocketServer } = require('ws');
const Database = require('better-sqlite3');
const path = require('path');

const PORT = 3010;
const WS_PORT = 3011;

// ========== SQLite 数据库 ==========
const db = new Database(path.join(__dirname, 'lock_data.db'));
db.pragma('journal_mode = WAL');

// 创建表
db.exec(`
  CREATE TABLE IF NOT EXISTS property_changes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    identifier TEXT NOT NULL,
    value TEXT,
    data_type TEXT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    source TEXT DEFAULT 'device'
  );
  CREATE TABLE IF NOT EXISTS operation_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    action TEXT NOT NULL,
    detail TEXT,
    result TEXT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
  );
  CREATE INDEX IF NOT EXISTS idx_prop_time ON property_changes(timestamp);
  CREATE INDEX IF NOT EXISTS idx_log_time ON operation_logs(timestamp);
`);

const insertProp = db.prepare('INSERT INTO property_changes (identifier, value, data_type, source) VALUES (?, ?, ?, ?)');
const insertLog = db.prepare('INSERT INTO operation_logs (action, detail, result) VALUES (?, ?, ?)');
const getRecentProps = db.prepare('SELECT * FROM property_changes ORDER BY id DESC LIMIT ?');
const getRecentLogs = db.prepare('SELECT * FROM operation_logs ORDER BY id DESC LIMIT ?');
const getPropsByIdentifier = db.prepare('SELECT * FROM property_changes WHERE identifier = ? ORDER BY id DESC LIMIT ?');

// 路径规范化：去掉 /rlsb 前缀（兼容本地开发和 nginx 代理两种场景）
function normalizePath(url) {
  return url.replace(/^\/rlsb/, '');
}

// ========== MQTT ==========
let mqttClient = null;
let mqttConnected = false;
let mqttConfig = { productId: '', deviceName: '', token: '' };
const wsClients = new Set();

function connectMQTT(productId, deviceName, token) {
  return new Promise((resolve, reject) => {
    if (mqttClient && mqttConnected &&
        mqttConfig.productId === productId &&
        mqttConfig.deviceName === deviceName) {
      resolve({ success: true, message: '已连接' });
      return;
    }

    if (mqttClient) {
      mqttClient.end(true);
      mqttClient = null;
      mqttConnected = false;
    }

    mqttConfig = { productId, deviceName, token };

    // 用不同 client_id 避免和设备冲突
    const serverClientId = deviceName + '_web';
    console.log(`[MQTT] 连接 mqtts.heclouds.com:1883 (TLS), client_id=${serverClientId}`);

    mqttClient = mqtt.connect('mqtts://mqtts.heclouds.com:1883', {
      clientId: serverClientId,
      username: productId,
      password: token,
      connectTimeout: 10000,
      keepalive: 60,
      clean: true,
      rejectUnauthorized: false  // OneNET 使用自签名证书
    });

    mqttClient.on('connect', () => {
      mqttConnected = true;
      console.log('[MQTT] 连接成功');

      // 订阅属性上报
      const postTopic = `$sys/${productId}/${deviceName}/thing/property/post`;
      mqttClient.subscribe(postTopic, { qos: 1 }, (err) => {
        if (err) console.error('[MQTT] 订阅失败:', err.message);
        else console.log(`[MQTT] 已订阅: ${postTopic}`);
      });

      // 订阅属性设置响应
      const setReplyTopic = `$sys/${productId}/${deviceName}/thing/property/set_reply`;
      mqttClient.subscribe(setReplyTopic, { qos: 1 }, (err) => {
        if (err) console.error('[MQTT] 订阅失败:', err.message);
        else console.log(`[MQTT] 已订阅: ${setReplyTopic}`);
      });

      // 订阅属性获取响应
      const getReplyTopic = `$sys/${productId}/${deviceName}/thing/property/get_reply`;
      mqttClient.subscribe(getReplyTopic, { qos: 1 }, (err) => {
        if (err) console.error('[MQTT] 订阅失败:', err.message);
        else console.log(`[MQTT] 已订阅: ${getReplyTopic}`);
      });

      resolve({ success: true, message: 'MQTT 连接成功' });
    });

    mqttClient.on('message', (topic, message) => {
      const msg = message.toString();
      console.log(`[MQTT] 收到: ${topic} -> ${msg.substring(0, 200)}`);

      try {
        const data = JSON.parse(msg);
        handleMQTTMessage(topic, data);
      } catch (e) {
        console.error('[MQTT] 解析失败:', e.message);
      }
    });

    mqttClient.on('error', (err) => {
      console.error('[MQTT] 错误:', err.message);
      mqttConnected = false;
    });

    mqttClient.on('close', () => {
      console.log('[MQTT] 连接关闭');
      mqttConnected = false;
    });

    mqttClient.on('offline', () => {
      console.log('[MQTT] 离线');
      mqttConnected = false;
    });

    setTimeout(() => {
      if (!mqttConnected) {
        reject(new Error('MQTT 连接超时'));
        if (mqttClient) mqttClient.end(true);
      }
    }, 10000);
  });
}

function handleMQTTMessage(topic, data) {
  // 属性上报：设备主动上报属性值
  if (topic.includes('/thing/property/post')) {
    if (data.params) {
      Object.entries(data.params).forEach(([identifier, propData]) => {
        const value = typeof propData.value === 'boolean' ? String(propData.value) : String(propData.value);
        const dataType = typeof propData.value === 'boolean' ? 'bool' : typeof propData.value;

        // 存入数据库
        insertProp.run(identifier, value, dataType, 'device');

        // 广播给所有 WebSocket 客户端
        broadcast({
          type: 'property_update',
          identifier,
          value: propData.value,
          dataType,
          time: Date.now(),
          source: 'device'
        });

        console.log(`[DB] 保存: ${identifier} = ${value}`);
      });
    }
  }

  // 属性设置响应
  if (topic.includes('/thing/property/set_reply')) {
    const success = data.code === 200;
    insertLog.run('set_property', JSON.stringify(data), success ? 'success' : 'failed');

    broadcast({
      type: 'set_reply',
      code: data.code,
      success,
      data
    });
  }

  // 属性获取响应
  if (topic.includes('/thing/property/get_reply')) {
    if (data.data && data.data.properties) {
      Object.entries(data.data.properties).forEach(([identifier, propData]) => {
        const value = String(propData.value);
        insertProp.run(identifier, value, typeof propData.value === 'boolean' ? 'bool' : 'unknown', 'query');
      });
    }

    broadcast({
      type: 'get_reply',
      data
    });
  }
}

// 通过 MQTT 设置属性
function setPropertyViaMQTT(productId, deviceName, propertyId, value) {
  return new Promise((resolve, reject) => {
    if (!mqttClient || !mqttConnected) {
      reject(new Error('MQTT 未连接'));
      return;
    }

    const topic = `$sys/${productId}/${deviceName}/thing/property/set`;
    const msgId = Date.now().toString();
    const message = JSON.stringify({
      id: msgId,
      version: "1.0",
      params: {
        [propertyId]: { value }
      }
    });

    console.log(`[MQTT] 发布: ${topic} -> ${message}`);

    mqttClient.publish(topic, message, { qos: 1 }, (err) => {
      if (err) {
        console.error('[MQTT] 发布失败:', err.message);
        reject(err);
      } else {
        insertLog.run('set_property', `${propertyId}=${value}`, 'sent');
        console.log('[MQTT] 发布成功');
        resolve({ success: true, id: msgId });
      }
    });
  });
}

// 通过 MQTT 获取属性
function getPropertyViaMQTT(productId, deviceName) {
  return new Promise((resolve, reject) => {
    if (!mqttClient || !mqttConnected) {
      reject(new Error('MQTT 未连接'));
      return;
    }

    const topic = `$sys/${productId}/${deviceName}/thing/property/get`;
    const msgId = Date.now().toString();
    const message = JSON.stringify({
      id: msgId,
      version: "1.0"
    });

    console.log(`[MQTT] 获取属性: ${topic}`);

    mqttClient.publish(topic, message, { qos: 1 }, (err) => {
      if (err) reject(err);
      else resolve({ success: true, id: msgId });
    });
  });
}

function disconnectMQTT() {
  if (mqttClient) {
    mqttClient.end(true);
    mqttClient = null;
    mqttConnected = false;
    mqttConfig = { productId: '', deviceName: '', token: '' };
    console.log('[MQTT] 已断开');
  }
}

// ========== WebSocket ==========
const wss = new WebSocketServer({ port: WS_PORT });

wss.on('connection', (ws) => {
  console.log('[WS] 新客户端连接');
  wsClients.add(ws);

  // 发送当前 MQTT 状态
  ws.send(JSON.stringify({
    type: 'mqtt_status',
    connected: mqttConnected,
    config: mqttConfig
  }));

  // 发送最近的历史数据
  const recentProps = getRecentProps.all(20);
  ws.send(JSON.stringify({
    type: 'history',
    properties: recentProps
  }));

  ws.on('message', (message) => {
    try {
      const data = JSON.parse(message.toString());
      handleWSMessage(ws, data);
    } catch (e) {
      console.error('[WS] 解析失败:', e.message);
    }
  });

  ws.on('close', () => {
    console.log('[WS] 客户端断开');
    wsClients.delete(ws);
  });
});

function handleWSMessage(ws, data) {
  switch (data.type) {
    case 'connect':
      connectMQTT(data.productId, data.deviceName, data.token)
        .then(result => {
          ws.send(JSON.stringify({ type: 'connect_result', ...result }));
          broadcast({ type: 'mqtt_status', connected: true, config: mqttConfig });
        })
        .catch(err => {
          ws.send(JSON.stringify({ type: 'connect_result', success: false, message: err.message }));
        });
      break;

    case 'set_property':
      // 始终持久化到本地数据库并广播
      insertProp.run(data.identifier, String(data.value), 'bool', 'user');
      insertLog.run('set_property', `${data.identifier}=${data.value}`, 'sent');
      broadcast({
        type: 'property_update',
        identifier: data.identifier,
        value: data.value,
        dataType: 'bool',
        time: Date.now(),
        source: 'user'
      });
      // 尝试通过 MQTT 发送（best-effort）
      if (mqttConnected) {
        setPropertyViaMQTT(mqttConfig.productId, mqttConfig.deviceName, data.identifier, data.value)
          .then(result => {
            ws.send(JSON.stringify({ type: 'set_result', success: true, ...result }));
          })
          .catch(err => {
            ws.send(JSON.stringify({ type: 'set_result', success: false, message: err.message }));
          });
      } else {
        ws.send(JSON.stringify({ type: 'set_result', success: true, message: '本地已记录（MQTT 未连接）' }));
      }
      break;

    case 'get_property':
      if (!mqttConnected) {
        ws.send(JSON.stringify({ type: 'error', message: 'MQTT 未连接' }));
        return;
      }
      getPropertyViaMQTT(mqttConfig.productId, mqttConfig.deviceName)
        .then(result => ws.send(JSON.stringify({ type: 'get_result', success: true, ...result })))
        .catch(err => ws.send(JSON.stringify({ type: 'get_result', success: false, message: err.message })));
      break;

    case 'get_history':
      const limit = data.limit || 50;
      const props = data.identifier
        ? getPropsByIdentifier.all(data.identifier, limit)
        : getRecentProps.all(limit);
      const logs = getRecentLogs.all(limit);
      ws.send(JSON.stringify({ type: 'history', properties: props, logs }));
      break;

    case 'disconnect':
      disconnectMQTT();
      broadcast({ type: 'mqtt_status', connected: false });
      break;
  }
}

function broadcast(data) {
  const msg = JSON.stringify(data);
  wsClients.forEach(ws => {
    if (ws.readyState === 1) ws.send(msg);
  });
}

// ========== HTTP 代理（保持兼容）==========
function proxyRequest(targetUrl, method, headers, body) {
  return new Promise((resolve, reject) => {
    const url = new URL(targetUrl);
    const isHttps = url.protocol === 'https:';
    const lib = isHttps ? https : http;
    const options = {
      hostname: url.hostname,
      port: url.port || (isHttps ? 443 : 80),
      path: url.pathname + url.search,
      method,
      headers: { ...headers, 'Content-Type': 'application/json' }
    };

    const req = lib.request(options, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve({ status: res.statusCode, body: data }));
    });

    req.on('error', reject);
    if (body) req.write(body);
    req.end();
  });
}

const httpServer = http.createServer(async (req, res) => {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  // 收集请求体
  let body = '';
  req.on('data', chunk => body += chunk);
  await new Promise(r => req.on('end', r));

  const urlParams = new URL(req.url, `http://localhost:${PORT}`);

  // 路径规范化（兼容 /rlsb/api 和 /api）
  const reqPath = normalizePath(req.url);

  // HTTP API 代理
  if (reqPath.startsWith('/api/onenet')) {
    const targetUrl = urlParams.searchParams.get('url');
    if (!targetUrl) {
      res.writeHead(400, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: 'Missing ?url= parameter' }));
      return;
    }

    const forwardHeaders = {};
    if (req.headers.authorization) forwardHeaders['Authorization'] = req.headers.authorization;

    try {
      console.log(`[HTTP] ${req.method} ${targetUrl}`);
      const result = await proxyRequest(targetUrl, req.method, forwardHeaders, body || null);
      console.log(`[HTTP] 响应 ${result.status}: ${result.body.substring(0, 200)}`);
      res.writeHead(result.status, { 'Content-Type': 'application/json' });
      res.end(result.body);
    } catch (err) {
      console.error('[HTTP] 代理错误:', err.message);
      res.writeHead(502, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: 'Proxy error', message: err.message }));
    }
    return;
  }

  // 保存属性变更和操作日志（前端 HTTP API 操作后持久化）
  if (reqPath === '/api/log' && req.method === 'POST') {
    try {
      const logData = JSON.parse(body);
      if (logData.properties) {
        logData.properties.forEach(p => {
          insertProp.run(p.identifier, String(p.value), p.dataType || typeof p.value, p.source || 'http');
        });
      }
      if (logData.operations) {
        logData.operations.forEach(op => {
          insertLog.run(op.action, op.detail || '', op.result || 'unknown');
        });
      }
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ success: true }));
    } catch (e) {
      console.error('[HTTP] 日志保存失败:', e.message);
      res.writeHead(400, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ error: e.message }));
    }
    return;
  }

  // 查询历史数据 API
  if (reqPath.startsWith('/api/history')) {
    const limit = parseInt(urlParams.searchParams.get('limit')) || 50;
    const identifier = urlParams.searchParams.get('identifier');
    const props = identifier
      ? getPropsByIdentifier.all(identifier, limit)
      : getRecentProps.all(limit);
    const logs = getRecentLogs.all(limit);
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ properties: props, logs }));
    return;
  }

  // MQTT 状态
  if (reqPath === '/api/mqtt/status') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ connected: mqttConnected, config: mqttConfig }));
    return;
  }

  res.writeHead(404, { 'Content-Type': 'application/json' });
  res.end(JSON.stringify({ error: 'Not Found' }));
});

// ========== 启动 ==========
httpServer.listen(PORT, () => {
  console.log('========================================');
  console.log('  人脸识别智能锁 - 实时服务器');
  console.log(`  HTTP 代理: http://localhost:${PORT}`);
  console.log(`  WebSocket: ws://localhost:${WS_PORT}`);
  console.log(`  数据库: ${path.join(__dirname, 'lock_data.db')}`);
  console.log('========================================');
});

// 优雅退出
process.on('SIGINT', () => {
  console.log('\n正在关闭...');
  disconnectMQTT();
  db.close();
  process.exit(0);
});
