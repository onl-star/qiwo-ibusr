# 齐我输入法 Linux 端安装指南

## 系统要求

- Linux（任何发行版）
- IBus 1.5+
- Python 3.8+
- librime 1.x
- CMake 3.10+
- GCC 或 Clang（支持 C11）
- libnotify
- GTK 3
- libsecret（Secret Service 密码存储）

## 依赖安装

### Debian / Ubuntu

```bash
sudo apt install \
  ibus libibus-1.0-dev \
  librime-dev librime-data \
  libnotify-dev libgtk-3-dev libsecret-1-dev \
  cmake gcc pkg-config \
  python3
```

### Fedora

```bash
sudo dnf install \
  ibus ibus-devel \
  librime librime-devel rime-data \
  libnotify-devel gtk3-devel libsecret-devel \
  cmake gcc pkg-config \
  python3
```

### Arch Linux

```bash
sudo pacman -S \
  ibus \
  librime rime-data \
  libnotify gtk3 libsecret \
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

推荐直接使用一键安装脚本：

```bash
cd qiwo-ibusr
./install-linux.sh
```

脚本会自动安装系统依赖、配置 CMake、编译、安装并重启 IBus。常用参数：

```bash
# 指定安装前缀
./install-linux.sh --prefix /usr/local

# CMake 找不到 Rime 数据目录时手动指定
./install-linux.sh --rime-data-dir /usr/share/rime-data

# 安装前运行 CTest
./install-linux.sh --run-tests
```

也可以手动编译安装：

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
/usr/lib/qiwo/ibus-engine-rime          # IBus 引擎可执行文件
/usr/share/qiwo/qiwo-rime-sync          # WebDAV 同步命令
/usr/bin/qiwo-webdav-settings           # WebDAV 图形设置窗口
/usr/share/qiwo/icons/                   # 图标文件
/usr/share/applications/qiwo-webdav-settings.desktop  # 桌面菜单入口
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

如果列表里没有「齐我输入法」，先确认 IBus component 已安装并重启 IBus：

```bash
ls /usr/share/ibus/component/qiwo.xml
ibus restart
ibus list-engine | grep -i qiwo
```

如果使用 `--prefix /usr/local` 安装，component 路径通常是 `/usr/local/share/ibus/component/qiwo.xml`；部分发行版的 IBus 默认只扫描 `/usr/share/ibus/component`，这种情况下请安装到 `/usr`。

## 配置 WebDAV 同步

WebDAV 同步可以在图形窗口中配置。可以从桌面应用菜单搜索 **Qiwo WebDAV Settings** / **齐我 WebDAV 设置**，也可以直接运行：

```bash
qiwo-webdav-settings
```

切换到齐我输入法后，如果当前桌面显示 IBus 属性菜单，也可以在 IBus 输入法面板中点击 **「WebDAV 设置」**。填写 URL、用户名、密码、设备标识和自动同步间隔，然后点击保存。

设置窗口还提供 **测试连接** 和 **立即同步**：

- **测试连接** 使用当前窗口内容执行一次 dry-run，不会修改远端或本地数据。
- **立即同步** 使用当前有效配置执行同步，并在窗口中显示结果或失败原因。

密码优先保存到桌面 Secret Service（例如 GNOME Keyring、KWallet 兼容服务）。如果当前桌面没有可用的 Secret Service，会回退写入 `~/.config/qiwo/webdav.conf`，文件权限会设置为 `0600`，设置窗口会显示当前密码存储模式。

环境变量仍然可用，并且优先级高于图形保存的配置。需要临时覆盖或继续使用脚本配置时，在 shell 配置文件（`~/.bashrc`、`~/.zshrc` 或 `~/.profile`）中添加：

```bash
# WebDAV 服务器地址（必填）
export QIWO_WEBDAV_URL="https://dav.example.com/qiwo-rime-sync"

# 凭据（如需要）
export QIWO_WEBDAV_USERNAME="username"
export QIWO_WEBDAV_PASSWORD="password"

# 设备标识（可选，默认使用系统 hostname）
export QIWO_DEVICE_ID="linux-main"

# 自动同步间隔，单位分钟；0 表示禁用
export QIWO_AUTO_SYNC_INTERVAL_MINUTES="30"
```

配置完成后，重新登录或执行 `source ~/.bashrc` 使环境变量生效。

自动同步间隔的优先级如下：

1. `QIWO_AUTO_SYNC_INTERVAL_MINUTES` 环境变量。
2. `ibus_rime.yaml` 中的 `sync/auto_sync_interval_minutes`。
3. 图形窗口保存的自动同步间隔。
4. 默认禁用（`0`）。

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
qiwo-rime-sync sync \
  --rime-user-dir ~/.config/ibus/rime \
  --remote-url "$QIWO_WEBDAV_URL" \
  --username "$QIWO_WEBDAV_USERNAME" \
  --password-env QIWO_WEBDAV_PASSWORD \
  --device-id "linux-main" \
  --dry-run
```

### 定时自动同步

自动同步由 IBus 引擎按分钟间隔触发。可在 **WebDAV 设置** 窗口中填写间隔，或在 `ibus_rime.yaml` 中配置：

```yaml
sync:
  auto_sync_interval_minutes: 30
```

设置为 `0` 表示禁用自动同步。

## 命令行参考

```bash
qiwo-rime-sync <mode> [options]

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
| WebDAV 图形配置 | `~/.config/qiwo/webdav.conf` |
| WebDAV 环境覆盖 | `QIWO_WEBDAV_URL`、`QIWO_WEBDAV_USERNAME`、`QIWO_WEBDAV_PASSWORD`、`QIWO_DEVICE_ID`、`QIWO_AUTO_SYNC_INTERVAL_MINUTES` |

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
