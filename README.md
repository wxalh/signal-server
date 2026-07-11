
# Signal Server - C++ WebSocket信令服务器

一个轻量的 C++17 WebSocket 信令服务器，支持实时通信与消息转发。


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

- **C++17 编译器**：GCC 8+、Clang 8+ 或 MSVC 2019+
- **CMake**：3.21及以上
- **C++**：C++17标准
- **编译器**：MSVC 2019+、GCC 8+、Clang 8+

## 构建依赖

### 第三方库

本项目使用了以下优秀的开源库：

- **[Asio](https://think-async.com/Asio/)** - 异步网络与事件循环
- **[WebSocket++](https://github.com/zaphoyd/websocketpp)** - WebSocket 协议实现
- **[nlohmann/json](https://github.com/nlohmann/json)** - JSON 解析与序列化
- **[spdlog](https://github.com/gabime/spdlog)** - 快速 C++ 日志库

## 构建指南

首次构建前先拉取第三方子模块：

```bash
git submodule update --init --recursive
```

Asio、WebSocket++ 和 nlohmann/json 是头文件库，不需要单独执行编译命令。CMake 在编译
`signal_server` 时会自动使用这些头文件；spdlog 会由 CMake 构建为静态库并链接到程序中。

#### windows前置要求

1. 安装 [Visual Studio](https://visualstudio.microsoft.com/) 编译环境（推荐 Visual Studio 2019）

#### 编译（根据系统选择不同的preset）

```cmd
cmake --preset win64-msvc-release
cmake --build --preset win64-msvc-release
```

可执行文件位于 `out/build/win64-msvc-release/Release/signal_server.exe`。

#### Linux x86_64 本机构建

需要安装 GCC、G++、CMake 3.21 或更高版本：

```bash
cmake --preset linux-x64
cmake --build --preset linux-x64
```

可执行文件位于 `out/build/linux-x64/signal_server`。生成可发布压缩包请参阅
[`PACKAGING.md`](PACKAGING.md)，不要将本机构建与 Zig 交叉编译发布流程混用。


## 使用方法

### 启动服务器

```bash
# 基本用法（自动读取 config.ini 配置）
./signal_server
```

查看程序版本：

```bash
./signal_server --version
```

`-v`、`-V`、`--version` 和 `-version` 均可使用。版本参数会直接打印版本并退出，不会启动服务。

服务器会自动读取与可执行文件同目录下的 `config.ini` 配置文件。

### 配置文件说明

服务器使用 `config.ini` 进行参数配置，结构如下：

```ini
[local]
logLevel=info

[signal_server]
serverPort=3480
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
├── src/                    # 源代码
│   ├── main.cpp           # 程序入口
│   ├── websocketserver.*  # 主服务器实现
│   ├── websocketclient.*  # 客户端连接包装
│   ├── usermanager.*      # 用户数据管理
│   ├── messagehandler.*   # 消息处理逻辑
│   ├── rcsuser.*          # 用户模型
│   └── wsmsg.*            # 消息模型
└── out/                 # 构建输出（自动生成）
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

1. 遵循 C++17 编码规范
2. 保证共享资源线程安全
3. 增加适当的错误处理与日志
4. 新功能需同步更新文档
5. 多客户端并发场景下充分测试

## 常见问题排查

### 常见问题

1. **端口被占用**：请修改 `config.ini` 中端口号
2. **子模块缺失**：执行 `git submodule update --init --recursive`
3. **编译失败**：检查 CMake 版本和 C++17 编译器是否可用
4. **连接被拒绝**：检查防火墙设置和端口开放情况
