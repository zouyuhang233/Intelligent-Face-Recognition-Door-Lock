import paramiko
import time

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect('8.134.127.141', username='root', password='123456789zyhZ', timeout=10)

def run(cmd):
    print(f'[SSH] {cmd}')
    _, out, err = ssh.exec_command(cmd, timeout=30)
    o = out.read().decode('utf-8', errors='ignore').strip()
    e = err.read().decode('utf-8', errors='ignore').strip()
    if o: print(o)
    if e and 'warn' not in e.lower(): print(f'ERR: {e}')
    return o

# 1. 删除独立的 rlsb.conf
run('rm -f /etc/nginx/conf.d/rlsb.conf')

# 2. 检查是否已有 rlsb 配置
existing = run('grep -c rlsb /etc/nginx/conf.d/ridge.conf 2>/dev/null || echo 0')
print(f'Existing rlsb count: {existing}')

if existing.strip() == '0':
    # 3. 读取 ridge.conf，在最后一个 } 前插入 rlsb location
    _, out, _ = ssh.exec_command('cat /etc/nginx/conf.d/ridge.conf', timeout=10)
    content = out.read().decode('utf-8', errors='ignore')

    rlsb_block = """
    # === 人脸识别智能锁 ===
    location /rlsb/ {
        alias /var/www/rlsb/;
        index index.html;
        try_files $uri $uri/ /rlsb/index.html;
    }
    location /rlsb/api/ {
        proxy_pass http://127.0.0.1:3010/api/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_http_version 1.1;
        proxy_set_header Connection '';
    }
    location /rlsb/ws {
        proxy_pass http://127.0.0.1:3011;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_read_timeout 86400;
    }
    # === END 人脸识别智能锁 ===
"""

    # 在最后一个 } 前插入
    last_brace = content.rfind('}')
    if last_brace > 0:
        new_content = content[:last_brace] + rlsb_block + content[last_brace:]

        # 写入服务器
        sftp = ssh.open_sftp()
        with sftp.open('/etc/nginx/conf.d/ridge.conf', 'w') as f:
            f.write(new_content)
        sftp.close()
        print('Nginx config updated!')
    else:
        print('ERROR: Could not find closing brace')
else:
    print('rlsb config already exists, skipping')

# 4. 测试 nginx
run('nginx -t 2>&1')

# 5. 重载
run('systemctl reload nginx')
time.sleep(1)

# 6. 测试
run('curl -sk https://localhost/rlsb/ 2>&1 | head -3')
run('curl -s http://localhost:3010/api/onenet 2>&1 | head -3')

ssh.close()
print('Done!')
