# Linux 打包

GitHub Actions 使用兼容 manylinux2014/CentOS 7 的工具链（glibc 2.17）构建发布包。
Asio、WebSocket++、nlohmann/json 和 spdlog 均使用 `third_party` 中锁定版本的源码。前三者是
头文件库，编译 `signal_server` 时直接使用；spdlog 会构建为静态库。

`Build packages` 工作流会在代码推送到 `main` 分支或手动触发时运行。
它使用 Zig 的 glibc 2.17 交叉编译目标，为 x86_64、arm64 和 armhf 架构生成兼容 glibc 的
可执行文件。C++ runtime 和第三方依赖静态链接，glibc 和动态加载器由目标系统提供。工作流会构建
Linux x86_64、arm64、armhf 和 Windows x64、x86 五个平台包及其 SHA-256
校验文件。

每次推送到 `main` 后，五个平台会并行构建。全部成功后，工作流读取 `CMakeLists.txt` 中的项目
版本，强制移动 `v<版本>` 和 `latest` tag 到本次提交，并删除、重建对应的两个 GitHub Release。
例如项目版本为 `1.0.0` 时，会更新 `v1.0.0` 和 `latest`：

```bash
git push origin main
```

发布前需要先修改 `CMakeLists.txt` 中的版本号。相同版本下再次提交会覆盖已有版本 Release，适合
持续交付，但不保留旧提交对应的同版本二进制。手动触发只生成 Actions artifact，不更新 Release。

直接执行脚本而不设置 `ZIG_TARGET` 时，只会构建当前 Linux 主机架构的常规动态链接包：

```bash
./packaging/build-linux.sh
```

复现 GitHub Actions 的 glibc 2.17 交叉编译需要先安装 Zig 0.13.0。例如构建 x86_64：

```bash
ZIG=/usr/local/zig \
ZIG_TARGET=x86_64-linux-gnu.2.17 \
PACKAGE_ARCH=x86_64 \
./packaging/build-linux.sh
```

另外两个发布目标分别为 `aarch64-linux-gnu.2.17`/`arm64` 和
`arm-linux-gnueabihf.2.17`/`armhf`。

构建结果位于 `dist/signal-server-linux-<arch>.tar.gz`。在目标机器上解压后，通过顶层的
`./signal_server` 启动脚本运行服务器。

交叉编译发布产物只允许依赖 glibc 基础库和 Linux 动态加载器；出现 `libstdc++.so` 或其他动态库
时打包步骤会失败。本机动态构建会保留系统提供的 glibc，并打包其余可解析的共享库。
