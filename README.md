# 儿童咳嗽检测项目 - 开发板代码

## 项目简介

本项目是基于 **Edgi-Talk 平台** 的儿童咳嗽检测设备开发板代码，运行在 **RT-Thread 实时操作系统** 上。该设备能够实时监测儿童的咳嗽情况，并将数据上传至云端进行分析和展示。

## 功能特性

### 核心功能

- **咳嗽检测**：基于 Edge Impulse 机器学习模型实时检测咳嗽声音
- **咳嗽统计**：记录咳嗽次数、频率等统计数据
- **数据上传**：将咳嗽事件和统计数据上传至云端服务器
- **状态显示**：通过 LCD 屏幕展示设备状态和检测结果

### 系统功能

- WiFi 连接与状态管理
- 按钮唤醒与用户交互
- 设备状态管理（待机、监听、睡眠等）
- 环境数据采集（温度、湿度等）

## 项目结构

```
edgi-talk_cough/
├── applications/          # 应用层代码
│   ├── common/            # 通用模块（网络、显示、按键等）
│   ├── cough_detect/      # 咳嗽检测模块
│   ├── cough_ui/          # UI界面模块
│   └── main.c             # 主入口文件
├── board/                 # 开发板配置
├── edge-impulse/          # Edge Impulse SDK（机器学习模型）
├── libraries/             # 硬件驱动库
│   ├── HAL_Drivers/       # 硬件抽象层驱动
│   └── components/        # 组件库（LVGL、IPC等）
├── packages/              # 第三方软件包
│   ├── cJSON/             # JSON 解析库
│   ├── mbedtls/           # TLS 加密库
│   ├── opus/              # 音频编解码库
│   └── ...
└── figures/               # 文档图片资源
```

## 快速开始

### 1. 准备工作

- **开发环境**：推荐使用 RT-Thread Studio
- **硬件平台**：Edgi-Talk 开发板（基于 PSoC 6 系列）
- **工具链**：ARM GCC

### 2. WiFi 资源准备（首次使用）

WiFi 驱动需要从 FAL 加载三个固件文件（`.bin`、`.clm_blob`、`nvram.txt`）。这些文件位于项目根目录的 `resources/` 文件夹中。

- 在 menuconfig 中保持 `WHD_RESOURCES_IN_EXTERNAL_STORAGE_FAL` 选项启用
- 确保 FAL 分区表包含 `whd_firmware`、`whd_clm` 和 `whd_nvram` 分区
- 通过串口终端，在 `msh` 命令行执行以下命令上传资源：

```
whd_res_download whd_firmware
whd_res_download whd_clm
whd_res_download whd_nvram
```

每个命令会进入 YMODEM 模式，请使用支持 YMODEM 协议的终端（如 Xshell）发送对应的文件。

### 3. 首次配置（AP 模式）

1. 开发板启动后进入 **AP 模式**
2. 使用手机或电脑连接设备热点（密码显示在屏幕上）
3. 打开浏览器访问 **192.168.169.1** 进入配置界面
4. 点击 **Scan** 扫描附近的 WiFi 热点并选择连接
5. 连接成功后屏幕显示 **"Standby"**，表示设备已就绪

### 4. 设备交互

- **按钮操作**：按下用户按键进入语音输入状态
- **状态指示**：屏幕显示当前设备状态（待机、监听、睡眠等）
- **咳嗽检测**：设备自动检测咳嗽并记录事件

## 设备状态说明

### 1. 连接中（Connecting）

设备正在连接 WiFi 网络，等待网络建立。

### 2. 待机状态（Standby）

设备处于正常工作状态，持续监测咳嗽声音。

### 3. 监听状态（Listening）

按下按键后进入语音输入状态，正在处理音频数据。

### 4. 睡眠模式（Sleep）

低功耗模式，按下按钮可唤醒设备。

## 启动顺序

```
+------------------+
|   Secure M33     |
|  (安全核心)      |
+------------------+
         |
         v
+------------------+
|       M33        |
|  (非安全核心)    |
+------------------+
         |
         v
+-------------------+
|       M55         |
|  (应用核心)       |
+-------------------+
```

⚠️ 请严格按照此顺序烧录固件以确保正常运行。

## 注意事项

- 首次使用需访问 [小智官网](https://xiaozhi.me/) 完成后端绑定
- 确保 WiFi SSID 和密码正确，使用 **2.4GHz 网络**
- 设备需要网络连接才能正常工作
- 修改图形配置需使用以下工具：
  - `tools/device-configurator/device-configurator.exe`
  - `libs/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/design.modus`

## 启用 M55 核心

如需启用 M55 核心进行机器学习推理，请在 menuconfig 中配置：

```
RT-Thread Settings --> Hardware --> select SOC Multi Core Mode --> Enable CM55 Core
```

## 云端服务

本开发板代码配合云端服务使用，云端代码位于：`../edgi-talk-cough-cloud/`

云端提供以下功能：

- 设备管理与认证
- 咳嗽数据存储与分析
- 健康报告生成
- 告警规则配置

---

**项目维护者**: 儿童咳嗽检测项目组  
**平台**: Edgi-Talk + RT-Thread  
**版本**: 1.0.0
