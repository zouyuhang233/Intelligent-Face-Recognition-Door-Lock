"""
部署脚本 - 上传项目到阿里云服务器并配置
"""
import paramiko
import os
import time

SERVER = '8.134.127.141'
USER = 'root'
PASS = '123456789zyhZ'
REMOTE_DIR = '/var/www/rlsb'
DOMAIN = 'zouyuhang.online'
PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

def ssh_exec(ssh, cmd, sudo=False):
    """执行远程命令"""
    if sudo:
        cmd = f'echo "{PASS}" | sudo -S {cmd}'
    print(f'[SSH] {cmd}')
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=120)
    out = stdout.read().decode('utf-8', errors='ignore')
    err = stderr.read().decode('utf-8', errors='ignore')
    if out.strip():
        print(out.strip())
    if err.strip() and 'password' not in err.lower():
        print(f'[ERR] {err.strip()}')
    return out, err

def main():
    # 1. 连接服务器
    print('=' * 50)
    print('连接服务器...')
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(SERVER, username=USER, password=PASS, timeout=10)
    print('连接成功!')

    # 2. 检查环境
    print('\n--- 检查环境 ---')
    ssh_exec(ssh, 'node -v')
    ssh_exec(ssh, 'npm -v')
    ssh_exec(ssh, 'nginx -v 2>&1')

    # 3. 创建目录
    print('\n--- 创建目录 ---')
    ssh_exec(ssh, f'mkdir -p {REMOTE_DIR}')

    # 4. 上传文件
    print('\n--- 上传文件 ---')
    sftp = ssh.open_sftp()
    files_to_upload = ['index.html', 'server.js', 'package.json']
    for fname in files_to_upload:
        local_path = os.path.join(PROJECT_DIR, fname)
        remote_path = f'{REMOTE_DIR}/{fname}'
        if os.path.exists(local_path):
            print(f'上传 {fname}...')
            sftp.put(local_path, remote_path)
            print(f'  -> {remote_path}')
        else:
            print(f'跳过 {fname} (不存在)')
    sftp.close()

    # 5. 安装依赖（后台执行，用淘宝镜像）
    print('\n--- 安装依赖（后台执行）---')
    ssh_exec(ssh, f'cd {REMOTE_DIR} && nohup npm install --production --registry https://registry.npmmirror.com > /tmp/npm_install.log 2>&1 &')
    print('npm install 已在后台运行，等待完成...')
    time.sleep(30)  # 等待安装
    ssh_exec(ssh, f'cat /tmp/npm_install.log 2>/dev/null | tail -5')

    # 6. 停止旧进程
    print('\n--- 停止旧进程 ---')
    ssh_exec(ssh, 'pkill -f "node.*server.js" 2>/dev/null || true')
    time.sleep(1)

    # 7. 修改 server.js 端口（避免冲突）和前端 WebSocket 地址
    print('\n--- 修改端口配置 ---')
    # 服务端口改为 3010/3011
    ssh_exec(ssh, f"sed -i 's/const PORT = 3000/const PORT = 3010/' {REMOTE_DIR}/server.js")
    ssh_exec(ssh, f"sed -i 's/const WS_PORT = 3001/const WS_PORT = 3011/' {REMOTE_DIR}/server.js")
    # 前端 WebSocket 地址改为相对路径（通过 Nginx 代理）
    ssh_exec(ssh, f"sed -i \"s|const WS_URL='ws://localhost:3001'|const WS_URL=(location.protocol==='https:'?'wss://':'ws://')+location.host+'/rlsb/ws'|\" {REMOTE_DIR}/index.html")
    # 前端 HTTP 代理地址改为相对路径
    ssh_exec(ssh, f"sed -i \"s|const PX='http://localhost:3000/api/onenet'|const PX='/rlsb/api/onenet'|\" {REMOTE_DIR}/index.html")

    # 8. 配置 systemd 服务
    print('\n--- 配置 systemd 服务 ---')
    service_content = f"""[Unit]
Description=Face Recognition Smart Lock Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory={REMOTE_DIR}
ExecStart=/usr/bin/node {REMOTE_DIR}/server.js
Restart=always
RestartSec=5
Environment=NODE_ENV=production

[Install]
WantedBy=multi-user.target
"""
    ssh_exec(ssh, f"cat > /etc/systemd/system/rlsb.service << 'EOF'\n{service_content}EOF")
    ssh_exec(ssh, 'systemctl daemon-reload')
    ssh_exec(ssh, 'systemctl enable rlsb')
    ssh_exec(ssh, 'systemctl start rlsb')
    ssh_exec(ssh, 'systemctl status rlsb --no-pager 2>&1')

    # 9. 配置 Nginx（CentOS 用 conf.d）
    print('\n--- 配置 Nginx ---')
    nginx_conf = f"""server {{
    listen 80;
    server_name {DOMAIN};

    # 前端页面
    location /rlsb/ {{
        alias {REMOTE_DIR}/;
        index index.html;
        try_files $uri $uri/ /rlsb/index.html;
    }}

    # HTTP API 代理
    location /rlsb/api/ {{
        proxy_pass http://127.0.0.1:3010/api/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_http_version 1.1;
        proxy_set_header Connection '';
    }}

    # WebSocket 代理
    location /rlsb/ws {{
        proxy_pass http://127.0.0.1:3011;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 86400;
    }}
}}
"""
    ssh_exec(ssh, f"cat > /etc/nginx/conf.d/rlsb.conf << 'EOF'\n{nginx_conf}EOF")
    ssh_exec(ssh, 'nginx -t 2>&1')
    ssh_exec(ssh, 'systemctl reload nginx 2>&1')

    # 10. 检查端口
    print('\n--- 检查端口 ---')
    ssh_exec(ssh, 'ss -tlnp | grep -E "3010|3011|80" 2>&1')

    # 11. 测试
    print('\n--- 测试 ---')
    ssh_exec(ssh, 'curl -s http://localhost:3010/api/onenet 2>&1 | head -5')
    ssh_exec(ssh, 'curl -s http://localhost/rlsb/ 2>&1 | head -3')

    ssh.close()
    print('\n' + '=' * 50)
    print(f'部署完成!')
    print(f'访问地址: http://{DOMAIN}/rlsb')
    print(f'WebSocket: ws://{DOMAIN}/rlsb/ws')
    print('=' * 50)

if __name__ == '__main__':
    main()
