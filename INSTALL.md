# 齐我输入法 Linux 端安装指南

## 系统要求

- Linux（任何发行版）
- IBus 1.5+
- Python 3.8+
- librime 1.x
- CMake 3.10+
- GCC 或 Clang（支持 C11）
- libnotify

## 依赖安装

### Debian / Ubuntu

```bash
sudo apt install \
  ibus libibus-1.0-dev \
  librime-dev librime-data \
  libnotify-dev \
  cmake gcc pkg-config \
  python3
```

### Fedora

```bash
sudo dnf install \
  ibus ibus-devel \
  librime librime-devel rime-data \
  libnotify-devel \
  cmake gcc pkg-config \
  python3
```

### Arch Linux

```bash
sudo pacman -S \
  ibus \
  librime rime-data \
  libnotify \
  cmake gcc pkg-config \
  python
```

## 源码准备

qiwo-ibusr 使用系统包管理器安装的 librime，不需要本地子模块。但如果是通过 git 获取源码，建议递归克隆：

```bash
git clone --recursive https://github.com/qiwo/qiwo-ibusr.git
cd qiwo-ibusr
```

如果已克隆但没有子模块内容（目录为空），初始化它们：

```bash
git submodule update --init --recursive
```

> 注意：librime 和 plum 子模块仅用于参考，编译时使用系统中已安装的 `librime-dev`。

## 编译

```bash
cd qiwo-ibusr

# 配置（默认安装到 /usr）
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr

# 编译
make

# 安装
sudo make install
```

安装后的文件布局：

```
/usr/libexec/qiwo/ibus-engine-qiwo      # IBus 引擎可执行文件
/usr/share/qiwo/qiwo_sync.py            # WebDAV 同步脚本
/usr/share/qiwo/icons/                   # 图标文件
/usr/share/ibus/component/qiwo.xml       # IBus 组件注册文件
/usr/share/rime-data/ibus_rime.yaml      # 默认 UI 配置文件
```

## 启用输入法

### 1. 重启 IBus

```bash
ibus restart
```

### 2. 添加输入法

```bash
# 方式一：命令行
ibus-setup

# 在 IBus 设置窗口中：
# 点击「输入法」标签 → 「添加」→ 搜索「齐我」→ 添加
```

### 3. 切换到齐我输入法

使用 IBus 快捷键 `Super + Space`（或你的自定义快捷键）切换输入法。

## 配置 WebDAV 同步

WebDAV 同步通过环境变量配置。在你的 shell 配置文件（`~/.bashrc`、`~/.zshrc` 或 `~/.profile`）中添加：

```bash
# WebDAV 服务器地址（必填）
export QIWO_WEBDAV_URL="https://dav.example.com/qiwo-rime-sync"

# 凭据（如需要）
export QIWO_WEBDAV_USERNAME="username"
export QIWO_WEBDAV_PASSWORD="password"

# 设备标识（可选，默认使用系统 hostname）
export QIWO_DEVICE_ID="linux-main"
```

配置完成后，重新登录或执行 `source ~/.bashrc` 使环境变量生效。

## 使用同步

### 手动同步

在 IBus 输入法面板中，点击齐我输入法旁边的 **「WebDAV 同步」** 按钮（同步图标）。

同步完成后，结果会通过桌面通知显示。

### 命令行测试

```bash
cd /usr/share/qiwo

# 预览模式（不实际写入）
QIWO_WEBDAV_URL="https://dav.example.com/qiwo-rime-sync" \
QIWO_WEBDAV_USERNAME="username" \
QIWO_WEBDAV_PASSWORD="password" \
python3 qiwo_sync.py sync \
  --rime-user-dir ~/.config/ibus/rime \
  --remote-url "$QIWO_WEBDAV_URL" \
  --username "$QIWO_WEBDAV_USERNAME" \
  --password-env QIWO_WEBDAV_PASSWORD \
  --json
```

### 定时自动同步

```bash
# 使用 crontab 设置定时同步（每 30 分钟）
crontab -e
# 添加：
*/30 * * * * QIWO_WEBDAV_URL="..." QIWO_WEBDAV_USERNAME="..." QIWO_WEBDAV_PASSWORD="..." python3 /usr/share/qiwo/qiwo_sync.py sync --rime-user-dir ~/.config/ibus/rime --remote-url "$QIWO_WEBDAV_URL" --username "$QIWO_WEBDAV_USERNAME" --password-env QIWO_WEBDAV_PASSWORD
```

或使用 systemd timer：

```ini
# ~/.config/systemd/user/qiwo-sync.service
[Unit]
Description=Qiwo WebDAV Sync

[Service]
Type=oneshot
Environment=QIWO_WEBDAV_URL=https://dav.example.com/qiwo-rime-sync
Environment=QIWO_WEBDAV_USERNAME=username
Environment=QIWO_WEBDAV_PASSWORD=password
ExecStart=/usr/bin/python3 /usr/share/qiwo/qiwo_sync.py sync --rime-user-dir %h/.config/ibus/rime --remote-url $QIWO_WEBDAV_URL --username $QIWO_WEBDAV_USERNAME --password-env QIWO_WEBDAV_PASSWORD
```

```ini
# ~/.config/systemd/user/qiwo-sync.timer
[Unit]
Description=Qiwo WebDAV Sync Timer

[Timer]
OnCalendar=*:0/30
Persistent=true

[Install]
WantedBy=timers.target
```

```bash
systemctl --user enable --now qiwo-sync.timer
```

## 命令行参考

```bash
python3 qiwo_sync.py <mode> [options]

模式:
  sync                    双向同步（默认）
  push                    仅上传本地文件到远程
  pull                    仅从远程下载文件

选项:
  --rime-user-dir <dir>   Rime 用户数据目录（必填）
  --remote-url <url>      WebDAV 服务器 URL（必填）
  --username <name>       WebDAV Basic Auth 用户名
  --password <pw>         WebDAV 密码（不推荐在命令行中明文传递）
  --password-env <var>    从环境变量读取密码（推荐）
  --device-id <id>        设备标识（默认使用主机名）
  --json                  以 JSON 格式输出结果
```

## 数据存储位置

| 内容 | 路径 |
|------|------|
| Rime 用户配置 | `~/.config/ibus/rime/` |
| Rime 共享数据 | `/usr/share/rime-data/` |
| 同步清单 | `~/.config/ibus/rime/.qiwo-sync/manifest.json` |
| 冲突备份 | `~/.config/ibus/rime/.qiwo-sync/backups/` |
| WebDAV 配置 | 通过环境变量（`QIWO_WEBDAV_*`） |

## 卸载

```bash
cd qiwo-ibusr/build
sudo make uninstall
# 或手动删除安装的文件
```

删除用户数据（可选）：

```bash
rm -rf ~/.config/ibus/rime
```

重新启动 IBus：

```bash
ibus restart
```
