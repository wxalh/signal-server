# 在飞牛 OS 安装 Signal Server + coturn

软件包同时包含 WebSocket 信令服务和 coturn。安装、启动、停止和升级由飞牛应用中心管理。在飞牛桌面点击 **Signal Server** 图标，即可查看运行状态和修改配置。设置页使用飞牛管理员会话，不占用额外的管理端口。

## 安装

1. 在 GitHub Release 或 Actions 产物中下载对应架构的 `.fpk`：`signal-server-fnos-<版本>-amd64.fpk` 对应 Intel/AMD x86_64，`signal-server-fnos-<版本>-arm64.fpk` 对应 ARM64。
2. 登录飞牛管理员账号，选择 **应用中心 → 手动安装**，上传 `.fpk` 并启动应用。
3. 点击飞牛桌面的 **Signal Server** 图标。按实际网络配置 WebSocket 信令端口，以及 coturn 的监听端口、中继范围、对外地址和凭据，然后点击 **保存并应用**。受影响的服务会短暂重启；需要重连的客户端应使用新端口和凭据。

配置、凭据和信令用户数据写入飞牛提供的应用数据目录。升级时沿用已有配置。完全卸载前请先备份应用数据。

## 端口与客户端配置

管理页的 **NAT 端口映射** 清单读取已保存的配置。保存成功后会显示新端口；在路由器上的转发规则需单独更新。

| 用途 | 默认端口 | 说明 |
| --- | --- | --- |
| WebSocket 信令 | `3480/TCP` | 例如 `ws://192.168.2.253:3480` |
| STUN/TURN | `3478/TCP+UDP` | 例如 `turn:192.168.2.253:3478` |
| TURN 中继 | `49160–49259/UDP` | WebRTC 中继。公网和内网的映射端口须一致 |
| TURN TCP 中继 | `49160–49259/TCP` | 仅在客户端使用 RFC 6062 TCP 中继时转发 |

局域网内可用 NAS 地址连接，TURN 对外地址可以留空。跨互联网时，按实际网络设置公网地址和端口转发；如果 NAS 位于 NAT 后，coturn 支持 `公网IP/内网IP` 格式。包默认提供普通 TURN（UDP/TCP），没有开放 TURN TLS/DTLS 端口。客户端若需要 `wss://`，请使用自己的 HTTPS 反向代理接入信令服务。

设置页会列出 TURN 用户和密码。若要让 NAS 本身接受远程文件和终端连接，请另外安装 [Airan Desk](https://github.com/wxalh/airan-desk) 的飞牛包，在飞牛桌面的 **Airan Desk** 设置页填写这里配置的信令地址和 STUN / TURN 参数。

桌面版 Airan Desk 可在 `conf/common.ini` 中按如下格式填写 TURN 参数；请用实际值替换示例：

```ini
[ice_server]
host=192.168.2.253
port=3478
username=webrtc
password=YOUR_TURN_PASSWORD
```

迁移以前独立安装的 coturn 时，从设置页填入原端口、认证和地址。若端口冲突，先为飞牛应用选择另一组端口，或计划停机后再切换原服务。

## 本地构建

在目标架构的 Debian 12 Linux 环境准备 Go 1.24 或更高版本、GNU C/C++、CMake、Ninja、Python 3、curl、pkg-config、libevent-dev 和 libssl-dev，再执行：

```bash
git submodule update --init --recursive
bash packaging/build-fnos.sh
python3 packaging/fnos/tests/smoke.py dist/signal-server-fnos-1.1.1-amd64.fpk
```

产物位于 `dist/`。发布工作流原生构建两个架构，并一起上传相应的 `.fpk` 和 SHA-256 校验文件。`main` 推送进入现有 Release 发布流程；手动运行工作流可以先下载 Actions artifact 验证。

默认生成供应用中心手动安装的未签名包。如已取得飞牛开发者签名文件，本地构建时可设置 `FN_PACK_KEY` 和 `FN_PACK_CERT`（分别为私钥和证书文件路径）。
