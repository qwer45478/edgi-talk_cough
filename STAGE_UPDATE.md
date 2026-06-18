# 儿童咳嗽检测项目 - 开发板代码阶段性更新文档

## 更新日期

2026-06-18

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

## 六、环境传感器增强

### 6.1 功能描述

在首页环境卡片中添加舒适度状态提示，帮助用户判断当前环境是否适合儿童。

### 6.2 修改文件

| 文件                                | 操作 | 说明                     |
| ----------------------------------- | ---- | ------------------------ |
| `applications/cough_ui/page_home.c` | 修改 | 添加舒适度判断函数和显示 |

### 6.3 舒适度判断规则

| 状态     | 温度条件 | 湿度条件 |
| -------- | -------- | -------- |
| OK       | 18-28°C  | 30-70%RH |
| Cool     | <18°C    | 30-70%RH |
| Warm     | >28°C    | 30-70%RH |
| Dry      | 18-28°C  | <30%RH   |
| Humid    | 18-28°C  | >70%RH   |
| Cold+Dry | <18°C    | <30%RH   |
| Cold+Wet | <18°C    | >70%RH   |
| Hot+Dry  | >28°C    | <30%RH   |
| Hot+Wet  | >28°C    | >70%RH   |

### 6.4 显示格式

```
25.5 °C
60.0 %RH
OK
```

---

## 七、咳嗽事件日志

### 7.1 功能描述

记录每次咳嗽事件的时间戳和白天/夜晚标志，方便家长回顾和分析。

### 7.2 修改文件

| 文件                                     | 操作 | 说明                 |
| ---------------------------------------- | ---- | -------------------- |
| `applications/common/common_cough_log.h` | 新建 | 头文件               |
| `applications/common/common_cough_log.c` | 新建 | 实现文件             |
| `applications/common/app_common.h`       | 修改 | 添加初始化标志       |
| `applications/common/app_common.c`       | 修改 | 添加初始化调用       |
| `applications/cough_ui/page_home.c`      | 修改 | 每次检测到咳嗽时记录 |

### 7.3 数据结构

```c
typedef struct
{
    time_t timestamp;       /* Unix timestamp */
    rt_bool_t is_day;       /* RT_TRUE = day, RT_FALSE = night */
} cough_log_entry_t;
```

### 7.4 存储策略

| 项目     | 说明           |
| -------- | -------------- |
| 存储方式 | 内存循环缓冲区 |
| 最大条目 | 100 条         |
| 线程安全 | 使用互斥锁保护 |
| 白天定义 | 6:00 - 18:00   |

### 7.5 API 接口

```c
void common_cough_log_init(void);          /* 初始化 */
void common_cough_log_add(rt_bool_t is_day);  /* 添加记录 */
int common_cough_log_count(void);          /* 获取记录数 */
int common_cough_log_get_recent(...);      /* 获取最近记录 */
```

---

## 八、云端数据上传接口对接

### 8.1 功能描述

修改咳嗽统计数据 JSON 输出格式，添加 `device_id` 和 `ts` 字段以匹配云端 `/api/cough/stats` 接口，实现开发板与云端的互联互通。

### 8.2 修改/新增文件

| 文件                                     | 类型 | 说明                                        |
| ---------------------------------------- | ---- | ------------------------------------------- |
| `applications/cough_detect/cough_stat.c` | 修改 | `cough_stat_to_json()` 添加 device_id 和 ts |
| `applications/common/common_config.c`    | 修改 | 默认 server_url 设为云端局域网地址          |

### 8.3 JSON 格式变化

**修改前：**

```json
{"date":20260617,"total":12,"day":8,"night":4,"bursts":2,"temp":26.5,"hum":58.2,"last_ts":1750200000,"hourly":[...]}
```

**修改后：**

```json
{"device_id":"a1b2c3d4e5f6","ts":1750201234,"date":20260617,"total":12,"day":8,"night":4,"bursts":2,"temp":26.5,"hum":58.2,"last_ts":1750200000,"hourly":[...]}
```

### 8.4 修改详情

**cough_stat.c - cough_stat_to_json()**

- 添加 `device_id` 字段：从 `common_config_get_device_id()` 获取
- 添加 `ts` 字段：从 `time(RT_NULL)` 获取当前 Unix 时间戳
- 优化数据复制：复制统计结构体数据到栈变量后释放锁，避免长时间持锁

**common_config.c - 默认配置表**

- `CFG_KEY_SERVER_URL` 默认值改为 `http://192.168.137.1:8000`（云端局域网地址）

### 8.5 云端接口说明

云端 `/api/cough/stats` 接口具备完整兜底兼容：

- `device_id` 缺失时使用 `demo_default_device_sn`
- `ts` 缺失时使用服务器当前时间
- `date` 支持 YYYYMMDD 数字格式自动转换

### 8.6 注意事项

- 只修改了 `cough_stat_to_json()` 一个函数的 JSON 输出格式
- SD 卡本地日志格式不变
- 原有 UI 显示功能不受影响
- 所有修改均标记 `// @yyc` 便于追溯

---

## 十、云端全接口对接

### 10.1 功能描述

将开发板与云端的所有设备 API 接口完全对接，实现心跳、环境数据、咳嗽事件批量上传、配置拉取、OTA 检查和提醒同步。

### 10.2 修改/新增文件

| 文件                                       | 类型 | 说明                                           |
| ------------------------------------------ | ---- | ---------------------------------------------- |
| `applications/common/common_network.c`     | 修改 | 新增 `common_network_get_json()` HTTP GET 方法 |
| `applications/common/common_network.h`     | 修改 | 新增 `common_network_get_json()` 声明          |
| `applications/common/ota.h`                | 新增 | OTA 模块头文件                                 |
| `applications/common/ota.c`                | 新增 | OTA 检查实现（GET /ota/check）                 |
| `applications/cough_detect/cough_stat.h`   | 修改 | 新增事件队列 API 声明                          |
| `applications/cough_detect/cough_stat.c`   | 修改 | 实现事件队列和批量上传 JSON 构造               |
| `applications/cough_detect/cough_remind.h` | 修改 | 新增提醒同步 API 声明                          |
| `applications/cough_detect/cough_remind.c` | 修改 | 实现提醒同步相关函数                           |
| `applications/cough_detect/cough_detect.c` | 修改 | 云端上传逻辑（心跳/环境/事件/统计/提醒）       |

### 10.3 新增 API 接口

| 接口                                    | 方法 | 开发板调用位置                      | 说明               |
| --------------------------------------- | ---- | ----------------------------------- | ------------------ |
| `/api/v1/device-api/heartbeat`          | POST | `cloud_upload_heartbeat()`          | 设备心跳上报       |
| `/api/v1/device-api/environment`        | POST | `cloud_upload_environment()`        | 环境数据上传       |
| `/api/v1/device-api/cough-events/batch` | POST | `cloud_upload_cough_events_batch()` | 咳嗽事件批量上传   |
| `/api/v1/device-api/config`             | GET  | `cloud_fetch_config()`              | 拉取云端设备配置   |
| `/api/v1/device-api/ota/check`          | GET  | `cloud_check_ota()`                 | 检查固件更新       |
| `/api/v1/device-api/reminders`          | GET  | `cloud_fetch_reminders()`           | 从云端获取提醒设置 |
| `/api/v1/device-api/reminders`          | POST | `cloud_upload_reminders()`          | 上传本地提醒到云端 |

### 10.4 云端上传时序

```
WiFi连接成功
    ↓
NTP时间同步
    ↓
cloud_fetch_config()      ← GET /api/v1/device-api/config
cloud_check_ota()         ← GET /api/v1/device-api/ota/check
cloud_fetch_reminders()   ← GET /api/v1/device-api/reminders (获取云端提醒)
cloud_upload_reminders()  ← POST /api/v1/device-api/reminders (上传本地提醒)
    ↓
每10分钟定时上传
    ├── cloud_upload_heartbeat()      ← POST /api/v1/device-api/heartbeat
    ├── cloud_upload_environment()    ← POST /api/v1/device-api/environment
    ├── cloud_upload_cough_events_batch() ← POST /api/v1/device-api/cough-events/batch
    └── cloud_upload_stats()         ← POST /api/cough/stats (已有接口)
```

### 10.5 事件队列设计

- **队列容量**：64 个事件
- **队列满策略**：丢弃最旧事件，保留最新
- **批量上传**：每次最多上传 16 个事件
- **线程安全**：使用互斥锁保护

### 10.6 OTA 模块

- **固件版本**：当前固定为 `1.0.0`
- **检查结果**：`ota_check_result_t` 结构体包含 `has_update`、`version`、`download_url`、`file_size`
- **解析方式**：简单 JSON 字段解析（`"version"`, `"download_url"`, `"file_size"`）

### 10.7 提醒同步设计

#### 10.7.1 数据结构

```json
{
  "slots": [
    {
      "slot_index": 0,
      "hour": 8,
      "minute": 0,
      "enabled": true,
      "label": "Morning Medicine"
    },
    {
      "slot_index": 1,
      "hour": 20,
      "minute": 30,
      "enabled": false,
      "label": "Night Medicine"
    }
  ]
}
```

#### 10.7.2 同步逻辑

- **拉取时机**：NTP 同步成功后自动从云端获取
- **上传时机**：NTP 同步成功后自动上传本地提醒到云端
- **保底机制**：当云端获取失败时，自动应用默认提醒配置（所有提醒关闭）
- **默认配置**：默认提醒为空，所有插槽禁用

#### 10.7.3 新增提醒 API 函数

| 函数                            | 说明                     |
| ------------------------------- | ------------------------ |
| `cough_remind_to_json()`        | 将本地提醒序列化为 JSON  |
| `cough_remind_from_json()`      | 从 JSON 应用提醒配置     |
| `cough_remind_apply_defaults()` | 应用默认提醒配置（保底） |
| `cough_remind_get_all_slots()`  | 获取所有插槽数组指针     |

### 10.8 保底机制汇总

| 功能         | 获取失败时保底行为           |
| ------------ | ---------------------------- |
| 云端配置拉取 | 使用本地硬编码配置           |
| OTA 检查     | 静默忽略，继续运行           |
| 提醒获取     | 应用默认提醒配置（全部禁用） |
| 提醒上传     | 保留本地提醒，下次重试       |

### 10.9 遵循开闭原则

- **扩展**：所有云端上传逻辑作为独立函数添加，不修改已有函数
- **新增**：事件队列、OTA 模块、提醒同步作为新功能模块添加
- **兼容**：原有 `/api/cough/stats` 上传逻辑保留，通过 `cloud_upload_stats()` 调用

### 10.10 注意事项

- 所有修改均标记 `// @yyc` 便于追溯
- HTTP GET 功能依赖 `PKG_USING_WEBCLIENT` 宏
- OTA 下载和更新逻辑预留 TODO，暂未实现
- **云端数据库**：新增 `device_reminders` 表，需要执行 `alembic upgrade head` 迁移
- **前端提醒设置**：新增 `GET/PUT /api/v1/devices/{device_sn}/reminders` 接口，设备页面可编辑用药提醒
- **提醒合并策略**：云端拉取时先从 Flash 加载本地提醒，再应用云端数据（云端权威），本地独有提醒不被清除
- **提醒声音优化**：从单次上行琶音改为"叮-咚"风格，重复 3 次，更友好易察觉
- **提醒弹窗**：触发提醒时屏幕弹出模态弹窗，显示时间+标签+"Got it!"按钮，按下或 30 秒后自动关闭

---

## 十二、小智AI语音助手集成

### 12.1 功能描述

集成官方"小智"AI语音助手到儿童咳嗽检测开发板。核心解决咳嗽检测与语音助手共用同一麦克风带来的资源冲突问题。采用官方小智SDK，设备绑定通过小智官网完成。

### 12.2 新增/修改文件

#### 开发板端 (edgi-talk_cough)

| 文件                                                     | 类型 | 说明                                         |
| -------------------------------------------------------- | ---- | -------------------------------------------- |
| `applications/voice_assistant/voice_assistant.h`         | 新增 | 语音助手模块头文件                           |
| `applications/voice_assistant/voice_assistant.c`         | 新增 | 语音助手模块实现（调用SDK）                  |
| `applications/voice_assistant/xiaozhi/`                  | 新增 | 官方小智SDK（完整复制）                      |
| `applications/voice_assistant/xiaozhi/xiaozhi.cpp`       | 新增 | 小智主逻辑（状态机、WebSocket通信）          |
| `applications/voice_assistant/xiaozhi/xiaozhi.h`         | 新增 | 头文件及宏定义                               |
| `applications/voice_assistant/xiaozhi/xiaozhi_audio.cpp` | 新增 | 音频采集/播放（已集成资源仲裁）              |
| `applications/voice_assistant/xiaozhi/xiaozhi_ui.c`      | 新增 | UI界面（状态/输出/Emoji显示）                |
| `applications/voice_assistant/xiaozhi/xiaozhi_ui.h`      | 新增 | UI头文件                                     |
| `applications/voice_assistant/xiaozhi/wake_word/`        | 新增 | 唤醒词检测（Edge Impulse SDK）               |
| `applications/voice_assistant/xiaozhi/iot/`              | 新增 | IoT设备管理                                  |
| `applications/voice_assistant/xiaozhi/mcp/`              | 新增 | MCP协议实现                                  |
| `applications/voice_assistant/xiaozhi/ui/3d_demo/`       | 新增 | 3D动画UI资源                                 |
| `applications/voice_assistant/SConscript`                | 新增 | 编译配置（容器）                             |
| `applications/voice_assistant/xiaozhi/SConscript`        | 新增 | 编译配置（核心）                             |
| `applications/voice_assistant/xiaozhi/*/SConscript`      | 新增 | 各子模块编译配置                             |
| `applications/cough_ui/page_xiaozhi.c`                   | 新增 | 小智助手页面封装                             |
| `applications/cough_ui/page_xiaozhi.h`                   | 新增 | 小智助手页面头文件                           |
| `applications/common/common_audio_capture.h`             | 修改 | 新增资源仲裁API（request_exclusive/release） |
| `applications/common/common_audio_capture.c`             | 修改 | 实现资源仲裁逻辑                             |
| `applications/cough_detect/cough_detect.c`               | 修改 | 集成资源仲裁（请求独占→读取→释放）           |
| `applications/main.c`                                    | 修改 | 初始化语音助手模块                           |

#### 云端 (edgi-talk-cough-cloud)

| 文件 | 类型 | 说明                                               |
| ---- | ---- | -------------------------------------------------- |
| 无   | -    | 使用小智官方云端（api.tenclass.net），无需云端改造 |

### 12.3 小智SDK状态机

使用官方定义的状态机：

```
kDeviceStateUnknown → kDeviceStateStarting → kDeviceStateIdle
                                                    ↓
                              ┌─────────────────────┼─────────────────────┐
                              ↓                     ↓                     ↓
                   kDeviceStateListening    kDeviceStateSpeaking    kDeviceStateActivating
                   （用户说话中）           （AI回复中）             （等待绑定）
```

### 12.4 音频资源仲裁机制

通过互斥锁实现咳嗽检测与语音助手的麦克风共享：

```c
// 咳嗽检测线程（低优先级）
while (1) {
    common_audio_capture_request_exclusive(AUDIO_USER_COUGH_DETECT);
    // 读取音频帧...
    common_audio_capture_release_exclusive(AUDIO_USER_COUGH_DETECT);
}

// 语音助手（高优先级）
void xz_mic_open() {
    common_audio_capture_request_exclusive(AUDIO_USER_VOICE_ASSISTANT);
    rt_device_open(mic_dev, RDONLY);
}
void xz_mic_close() {
    rt_device_close(mic_dev);
    common_audio_capture_release_exclusive(AUDIO_USER_VOICE_ASSISTANT);
}
```

**特点**：

- 咳嗽检测每次读取后立即释放麦克风
- 语音助手需要时能快速获取独占权
- 资源仲裁对双方透明

### 12.5 设备绑定流程

```
1. 开发板WiFi连接成功
2. 按一下顶部按键 → 触发WebSocket连接小智云端
3. 云端返回session_id → 屏幕显示验证码
4. 用户访问 https://xiaozhi.me/ → 输入验证码绑定设备
5. 绑定成功，后续正常使用
```

### 12.6 触发方式

- **物理按键**：按开发板顶部按键（GET_PIN(8,3)）触发聆听
- **唤醒词**（可选）：说出"小智小智"唤醒（需SDK支持）

### 12.7 麦克风使用策略

| 使用者   | 优先级 | 说明                 |
| -------- | ------ | -------------------- |
| 咳嗽检测 | 低     | 几乎100%时间运行     |
| 语音助手 | 高     | 按键触发，使用时独占 |

### 12.8 注意事项

- 所有新增/修改代码均标记 `// @yyc` 便于追溯
- WiFi配网使用项目已有的AP模式配网页面
- 设备绑定通过小智官网完成，无需云端改造
- 小智SDK的WebSocket连接、ASR、TTS、对话逻辑均使用官方服务

### 12.9 初始化流程

```c
// main.c
voice_assistant_init()
├── common_audio_capture_init()    // 资源仲裁初始化
├── xiaozhi_ui_init()             // 小智UI初始化
└── ws_xiaozhi_init()             // 小智主线程启动
                                    ├── WiFi连接
                                    ├── WebSocket连接
                                    ├── 按键初始化
                                    └── 状态机运行
```

---

## 十一、阶段性工作总结

以上所有模块均已完成。
