
# Signal Server - Qt5 C++ WebSocket信令服务器

一个高性能的Qt5 C++实现的WebSocket信令服务器，支持实时通信与消息转发。


## 功能特性

- **实时WebSocket通信**：支持多客户端并发连接
- **消息转发**：智能路由消息到目标客户端
- **用户管理**：持久化用户数据，在线/离线状态跟踪
- **心跳机制**：连接保活与监控
- **JSON协议**：结构化消息格式，兼容Java实现
- **线程安全**：多客户端场景下并发保护
- **跨平台**：支持Windows、Linux、macOS


## 架构设计

### 核心组件

- **WebSocketServer**：主服务器，管理客户端连接与生命周期
- **WebSocketClient**：单个客户端连接包装，状态管理
- **UserManager**：用户数据持久化与查询的单例服务
- **MessageHandler**：消息处理与路由核心逻辑
- **RcsUser**：用户模型，支持JSON序列化
- **WsMsg**：WebSocket消息模型，结构化通信


### 消息协议

服务器采用与Java实现兼容的JSON协议：

```json
{
  "type": "消息类型",
  "data": "消息内容", 
  "sender": "发送者ID",
  "receiver": "接收者ID"
}
```

特殊消息：
- `@heart`：心跳/保活消息
- `onlineOne`：用户上线通知
- `offlineOne`：用户离线通知
- `onlineList`：所有在线用户列表
- `error`：错误响应消息


## 环境要求

- **Qt5**：Core、Network、WebSockets模块
- **CMake**：3.16及以上
- **C++**：C++17标准
- **编译器**：MSVC 2019+、GCC 8+、Clang 8+

## 构建依赖

### 第三方库

本项目使用了以下优秀的开源库：

- **[Qt5](https://www.qt.io/)** - 跨平台 C++ 应用程序开发框架
  - qt5-base (Core, GUI, Widgets, Network, Concurrent)
  - qt5-websockets - WebSocket 通信支持
- **[spdlog](https://github.com/gabime/spdlog)** - 快速 C++ 日志库
- **[vcpkg](https://github.com/microsoft/vcpkg)** - C++ 包管理器

## 构建指南

### Windows

#### 前置要求

1. 安装 [Visual Studio](https://visualstudio.microsoft.com/) 编译环境（推荐 Visual Studio 2022）
2. 安装 [vcpkg](https://github.com/microsoft/vcpkg) 包管理器

#### 安装依赖

```cmd
vcpkg install spdlog qt5-base[core,openssl] qt5-websockets --triplet=x64-windows
```

#### 编译

```cmd
cmake -DCMAKE_TOOLCHAIN_FILE=D:/software/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -S . -B out/build/x64 -G "Visual Studio 17 2022" -T host=x64 -A x64

cmake --build out/build/x64 --config RelWithDebInfo --
```

### Linux (Ubuntu)

#### 前置要求

安装系统依赖：

```bash
sudo apt update
sudo apt install nasm pkg-config meson ninja-build build-essential python3 python3-jinja2 libdbus-1-dev libxi-dev libxtst-dev
sudo apt install '^libxcb.*-dev' libx11-xcb-dev libgl1-mesa-dev libxrender-dev libxi-dev libxkbcommon-dev libxkbcommon-x11-dev libfontconfig1-dev libfreetype6-dev libharfbuzz-dev
```

#### 安装依赖

```bash
vcpkg install spdlog qt5-base[core,openssl] qt5-websockets --triplet=x64-linux
```

#### 编译

```bash
cmake --preset x64-linux
cmake --build --preset x64-linux
```

或者使用手动命令：

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux -G Ninja
cmake --build build
```


## 使用方法

### 启动服务器

```bash
# 基本用法（自动读取 config.ini 配置）
./signal_server
```

服务器会自动读取与可执行文件同目录下的 `config.ini` 配置文件。

### 配置文件说明

服务器使用 `config.ini` 进行参数配置，结构如下：

```ini
[local]
logLevel=info

[signal_server]
serverPort=8080
serverName=Signal Server

```

**主要参数说明：**

| 区块 | 参数 | 说明 | 默认值 |
|-------|--------|------|--------|
| signal_server | serverPort | 服务器监听端口 | 8080 |
| signal_server | serverName | 服务器显示名称 | "Signal Server" |
| local | logLevel | 日志级别（debug/info/warn/error） | "info" |

### 客户端连接

客户端可通过如下URL格式连接WebSocket服务器：
```
ws://localhost:8080?sessionId=your_session_id&hostname=your_hostname
```

参数说明：
- `sessionId`：客户端唯一标识（必填）
- `hostname`：客户端名称（可选）

### 消息示例

**向其他客户端发送消息：**
```json
{
  "type": "signal",
  "data": "Hello from client A",
  "sender": "client_a",
  "receiver": "client_b"
}
```

**心跳包：**
```
@heart
```

## 配置说明

### 配置文件

服务器会自动读取可执行文件目录下的 `config.ini`，如文件不存在则首次运行时自动生成默认配置。

**配置区块：**

- **[signal_server]**：WebSocket服务器参数
- **[local]**：本地应用参数

### 运行时行为

- **用户数据**：存储于应用数据目录（`users.json`）
- **连接清理**：每30秒自动清理无效连接
- **状态通知**：实时推送在线/离线状态
- **错误处理**：优雅响应错误并记录日志

## 开发说明

### 项目结构

```
signal_server/
├── CMakeLists.txt          # 构建配置
├── README.md               # 项目说明
├── .github/
│   └── copilot-instructions.md  # AI助手指令
├── src/                    # 源代码
│   ├── main.cpp           # 程序入口
│   ├── websocketserver.*  # 主服务器实现
│   ├── websocketclient.*  # 客户端连接包装
│   ├── usermanager.*      # 用户数据管理
│   ├── messagehandler.*   # 消息处理逻辑
│   ├── rcsuser.*          # 用户模型
│   └── wsmsg.*            # 消息模型
└── build/                 # 构建输出（自动生成）
```

### 新增功能

1. **新增消息类型**：扩展 `MessageHandler::handleSignalMessage()`
2. **用户属性扩展**：修改 `RcsUser` 类并更新JSON序列化
3. **自定义协议**：在 `MessageHandler` 中实现并校验
4. **持久化扩展**：在 `UserManager` 中增加数据存储逻辑

### 测试方法

可使用多种WebSocket客户端工具进行测试：

- **浏览器控制台**：
  ```javascript
  const ws = new WebSocket('ws://localhost:8080?sessionId=test123&hostname=TestClient');
  ws.onmessage = (event) => console.log('收到:', event.data);
  ws.send('@heart');
  ```

- **wscat**（Node.js工具）：
  ```bash
  npm install -g wscat
  wscat -c 'ws://localhost:8080?sessionId=test123&hostname=TestClient'
  ```

## 许可证

本项目仅供学习和开发使用，按现状提供。

## 贡献指南

1. 遵循Qt5编码规范
2. 保证共享资源线程安全
3. 增加适当的错误处理与日志
4. 新功能需同步更新文档
5. 多客户端并发场景下充分测试

## 常见问题排查

### 常见问题

1. **端口被占用**：请修改 `config.ini` 中端口号
2. **未找到Qt5**：确保Qt5已正确安装并加入PATH
3. **编译失败**：检查CMake版本和Qt5模块可用性
4. **连接被拒绝**：检查防火墙设置和端口开放情况
