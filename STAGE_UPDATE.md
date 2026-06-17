# 儿童咳嗽检测项目 - 开发板代码阶段性更新文档

## 更新日期

2026-06-16

## 一、WiFi 动态配网功能（AP 模式）

### 1.1 功能描述

支持用户通过浏览器输入 WiFi 信息连接网络，保留硬编码作为备选方案。

### 1.2 新增/修改文件

| 文件                                       | 操作 | 说明                              |
| ------------------------------------------ | ---- | --------------------------------- |
| `applications/webserver/wifi_config_cgi.c` | 新增 | WiFi 扫描/连接 CGI 接口及配网页面 |
| `applications/webserver/wifi_config_cgi.h` | 新增 | CGI 接口头文件                    |
| `applications/webserver/webserver.c`       | 新增 | Web 服务器初始化                  |
| `applications/webserver/SConscript`        | 新增 | 编译配置                          |
| `applications/common/common_network.c`     | 修改 | AP 模式切换、凭证保存             |
| `applications/common/common_network.h`     | 修改 | AP 模式相关函数声明               |
| `applications/cough_ui/page_settings.c`    | 修改 | 添加"配置 WiFi"按钮               |

### 1.3 工作流程

1. 用户在设置页面点击"配置 WiFi"按钮
2. 开发板切换到 AP 模式，发出热点 `CoughDetect_xxx`
3. 用户连接热点，浏览器打开配网页面
4. 点击"扫描 WiFi"选择网络，输入密码，点击连接
5. 开发板收到凭证后切换到 STA 模式连接用户 WiFi
6. 连接成功后保存凭证到 Flash

### 1.4 CGI 接口

| 路由            | 功能               |
| --------------- | ------------------ |
| `/wifi_scan`    | 扫描可用 WiFi 网络 |
| `/wifi_connect` | 连接指定 WiFi      |
| `/index`        | 配网页面           |

### 1.5 MSH 命令

```
wifi_ap_start   # 手动开启 AP 模式
wifi_ap_stop    # 停止 AP 模式
```

---

## 二、NTP 时间同步增强

### 2.1 功能描述

增强时间同步可靠性，支持重试机制和定时自动校准。

### 2.2 新增/修改文件

| 文件                                   | 操作 | 说明                  |
| -------------------------------------- | ---- | --------------------- |
| `applications/common/common_network.h` | 修改 | 添加 NTP 相关函数声明 |
| `applications/common/common_network.c` | 修改 | 实现重试+定时校准逻辑 |

### 2.3 新增功能

| 功能     | 说明                                     |
| -------- | ---------------------------------------- |
| 重试机制 | 同步失败自动重试最多 3 次，每次间隔 2 秒 |
| 定时校准 | 每 30 分钟自动校准一次（可配置）         |
| 状态查询 | `common_network_ntp_is_synced()`         |
| MSH 命令 | `ntp_sync`、`ntp_resync`                 |

### 2.4 配置参数

```c
NTP_SYNC_MAX_RETRIES     = 3       // 最大重试次数
NTP_RESYNC_INTERVAL_MS   = 1800000 // 定时校准间隔（30分钟）
```

### 2.5 MSH 命令

```
ntp_sync           # 手动同步 NTP（带重试）
ntp_sync force     # 强制重新同步
ntp_resync start   # 启动定时校准
ntp_resync stop    # 停止定时校准
ntp_resync interval <ms>  # 设置校准间隔
```

### 2.6 工作流程

```
WiFi 连接成功 → wifi_ready_handler() → 发送 CD_EVENT_NTP_SYNC 事件
                                            │
                                            ▼
                              cough_detect_control_thread()
                                            │
                                            ▼
                              common_network_ntp_sync()
                                ├─ 失败？重试 3 次
                                └─ 成功？启动定时校准器
                                            │
                                            ▼
                              每 30 分钟自动触发 ntp_sync
```

---

## 三、代码规范

### 3.1 修改标记

所有新增和修改的代码都标记了 `// @yyc edit` 注释。

### 3.2 遵循原则

- **开闭原则**：对扩展开放，对修改封闭
- **非破坏性**：所有增强功能不影响原有逻辑
- **向后兼容**：原有 API 调用方式不变

---

## 四、首页时间显示

### 4.1 功能描述

在首页底部显示当前时间，每秒自动更新。

### 4.2 新增/修改文件

| 文件                                | 操作 | 说明                           |
| ----------------------------------- | ---- | ------------------------------ |
| `applications/cough_ui/page_home.c` | 修改 | 添加时间显示卡片和 LVGL 定时器 |

### 4.3 实现内容

- 添加 `CURRENT TIME` 卡片显示区域
- 使用 LVGL 定时器每秒更新时间
- 初始化显示 `--:--:--`，NTP 同步后显示实际时间

### 4.4 显示格式

```
CURRENT TIME
          14:30:25
```

### 4.5 前提条件

系统时间通过 NTP 同步成功后才能显示正确时间。

---

## 五、阶段性工作总结

以上所有模块均已完成。
