# CoTURN STUN/TURN 服务器配置指南

本文档介绍如何在Ubuntu 22.04系统上安装和配置CoTURN服务器，为WebRTC应用提供NAT穿透支持。

## 系统要求

- **操作系统**：Ubuntu 22.04 LTS（推荐）
- **网络**：具备公网IP的服务器
- **权限**：sudo管理员权限

## 安装步骤

### 1. 添加CoTURN仓库

```bash
sudo add-apt-repository ppa:ubuntuhandbook1/coturn
```

### 2. 更新软件包列表

```bash
sudo apt update
```

### 3. 安装CoTURN

```bash
sudo apt install coturn
```

## 配置说明

### 1. 备份原配置文件

```bash
sudo mv /etc/turnserver.conf{,.bak}
```

### 2. 创建新配置文件

```bash
sudo vim /etc/turnserver.conf
```

### 3. 配置文件内容

将以下内容复制到配置文件中：

```ini
# 日志文件路径
log-file=/var/log/coturn/turnserver.log

# TLS监听端口（TURNS协议）
tls-listening-port=5349

# --- 中继配置 ---
# UDP/TCP中继端口范围，根据网络环境和负载调整
min-port=40000
max-port=60000

# STUN/TURN监听端口
listening-port=3478

# 服务器公网IP地址（必须替换为实际公网IP）
external-ip=YOUR_PUBLIC_IP

# 启用长期凭证机制
lt-cred-mech

# 管理控制台密码
cli-password=YOUR_ADMIN_PASSWORD

# 用户认证（格式：用户名:密码）
user=YOUR_USERNAME:YOUR_PASSWORD

# 服务域名
realm=YOUR_DOMAIN

# 详细日志级别（0-7，7为最详细）
verbose=7

# 允许所有对等IP连接
allowed-peer-ip=0.0.0.0/0
allowed-peer-ip=::/0
```

### 4. 配置参数说明

| 参数 | 说明 | 示例值 |
|------|------|--------|
| `external-ip` | 服务器公网IP地址 | `203.0.113.1` |
| `cli-password` | 管理控制台密码 | `admin123` |
| `user` | TURN用户凭证 | `webrtc:secret123` |
| `realm` | 服务域名 | `example.com` |
| `min-port/max-port` | 中继端口范围 | `40000-60000` |

### 5. 保存并退出

按 `:wq` 然后回车保存配置文件。

## 服务管理

### 启动CoTURN服务

```bash
sudo systemctl start coturn
```

### 重启CoTURN服务

```bash
sudo systemctl restart coturn
```

### 启用开机自启

```bash
sudo systemctl enable coturn
```

### 查看服务状态

```bash
sudo systemctl status coturn
```

### 查看实时日志

```bash
sudo tail -f /var/log/coturn/turnserver.log
```

## 防火墙配置

确保以下端口在防火墙中开放：

```bash
# STUN/TURN主端口
sudo ufw allow 3478

# TURNS TLS端口
sudo ufw allow 5349

# UDP中继端口范围
sudo ufw allow 40000:60000/udp

# TCP中继端口范围（可选）
sudo ufw allow 40000:60000/tcp
```

## 测试验证

### 1. 检查端口监听

```bash
sudo netstat -tulpn | grep coturn
```

预期输出应包含：
```
udp    0.0.0.0:3478    coturn
tcp    0.0.0.0:3478    coturn
tcp    0.0.0.0:5349    coturn
```

### 2. STUN服务器测试

可以使用在线工具测试STUN功能：
- 访问：https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/
- 在STUN服务器字段输入：`stun:YOUR_PUBLIC_IP:3478`

### 3. TURN服务器测试

同样使用上述工具测试TURN功能：
- 输入：`turn:YOUR_PUBLIC_IP:3478`
- 用户名：配置文件中的用户名
- 密码：配置文件中的密码

## 与AiRanDesk集成

在AiRanDesk的 `config.ini` 中配置ICE服务器：

```ini
[ice_server]
host=YOUR_PUBLIC_IP
port=3478
username=YOUR_USERNAME
password=YOUR_PASSWORD
```

## 性能优化

### 1. 调整内核参数

编辑 `/etc/sysctl.conf`：

```bash
# 增加UDP缓冲区大小
net.core.rmem_max = 134217728
net.core.wmem_max = 134217728

# 增加TCP连接数
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
```

应用配置：
```bash
sudo sysctl -p
```

### 2. 调整文件描述符限制

编辑 `/etc/security/limits.conf`：

```
coturn soft nofile 65535
coturn hard nofile 65535
```

## 常见问题

### 1. 服务启动失败

- 检查配置文件语法：`sudo coturn -c /etc/turnserver.conf --check-config`
- 查看错误日志：`sudo journalctl -u coturn -f`

### 2. 客户端无法连接

- 确认防火墙端口开放
- 验证公网IP配置正确
- 检查用户认证配置

### 3. 中继性能问题

- 调整端口范围大小
- 增加服务器带宽
- 优化内核网络参数

## 安全建议

1. **使用强密码**：为用户认证和管理控制台设置复杂密码
2. **限制访问源**：可根据需要限制 `allowed-peer-ip` 范围
3. **启用TLS**：生产环境建议配置SSL证书
4. **定期更新**：保持CoTURN版本最新
5. **监控日志**：定期检查访问日志和错误日志

## 更多资源

- [CoTURN官方文档](https://github.com/coturn/coturn)
- [WebRTC STUN/TURN概念](https://webrtc.org/getting-started/turn-server)
- [RFC 5389 - STUN协议](https://tools.ietf.org/html/rfc5389)
- [RFC 5766 - TURN协议](https://tools.ietf.org/html/rfc5766)
