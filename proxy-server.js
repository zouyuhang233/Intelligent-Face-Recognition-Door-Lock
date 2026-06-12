/**
 * OneNET API 本地代理服务器
 * 解决浏览器 CORS 跨域问题
 *
 * 使用方法：
 *   1. 安装 Node.js (https://nodejs.org)
 *   2. 启动代理: node proxy-server.js
 *   3. 保持终端窗口运行，然后打开 index.html 网页使用
 */

const http = require('http');
const https = require('https');

const PORT = 3000;

// 通用 HTTPS/HTTP 请求代理
function proxyRequest(targetUrl, method, headers, body) {
    return new Promise((resolve, reject) => {
        const url = new URL(targetUrl);
        const isHttps = url.protocol === 'https:';
        const lib = isHttps ? https : http;
        const options = {
            hostname: url.hostname,
            port: url.port || (isHttps ? 443 : 80),
            path: url.pathname + url.search,
            method: method,
            headers: {
                ...headers,
                'Content-Type': 'application/json'
            }
        };

        const req = lib.request(options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => {
                resolve({ status: res.statusCode, body: data });
            });
        });

        req.on('error', reject);
        if (body) req.write(body);
        req.end();
    });
}

const server = http.createServer(async (req, res) => {
    // 设置 CORS 头
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization, access_token');

    // 处理预检请求
    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    // 只处理 /api/onenet 路径
    if (!req.url.startsWith('/api/onenet')) {
        res.writeHead(404, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Not Found. Use /api/onenet?url=<target_url>' }));
        return;
    }

    const urlParams = new URL(req.url, `http://localhost:${PORT}`);
    const targetUrl = urlParams.searchParams.get('url');

    if (!targetUrl) {
        res.writeHead(400, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Missing ?url= parameter' }));
        return;
    }

    // 收集请求体
    let body = '';
    req.on('data', chunk => body += chunk);

    await new Promise(r => req.on('end', r));

    // 透传 Authorization 头
    const forwardHeaders = {};
    if (req.headers.authorization) {
        forwardHeaders['Authorization'] = req.headers.authorization;
    }

    try {
        console.log(`[${new Date().toLocaleTimeString()}] ${req.method} ${targetUrl}`);
        const result = await proxyRequest(targetUrl, req.method, forwardHeaders, body || null);
        res.writeHead(result.status, { 'Content-Type': 'application/json' });
        res.end(result.body);
    } catch (err) {
        console.error('代理错误:', err.message);
        res.writeHead(502, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Proxy error', message: err.message }));
    }
});

server.listen(PORT, () => {
    console.log('========================================');
    console.log('  OneNET API 代理服务器已启动');
    console.log(`  地址: http://localhost:${PORT}`);
    console.log('  保持此窗口运行，然后打开 index.html');
    console.log('========================================');
});
