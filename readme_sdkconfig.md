# 小智AI聊天机器人 - SDKConfig 配置系统详解

## 概述

SDKConfig 是 ESP-IDF 框架的核心配置系统，用于管理项目的编译时配置选项。小智AI聊天机器人项目通过 SDKConfig 系统实现了灵活的硬件适配、功能选择和性能优化。

## SDKConfig 系统架构

### 1. 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                    SDKConfig 配置系统架构                        │
├─────────────────────────────────────────────────────────────────┤
│  配置定义层                                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              Kconfig 文件系统                               │ │
│  │  ├─ main/Kconfig.projbuild (项目配置)                      │ │
│  │  ├─ components/*/Kconfig (组件配置)                        │ │
│  │  └─ ESP-IDF/*/Kconfig (框架配置)                          │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              配置编辑器                                     │ │
│  │              menuconfig (基于ncurses)                      │ │
│  │  • 交互式配置界面                                          │ │
│  │  • 依赖关系检查                                            │ │
│  │  • 配置验证                                                │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              配置存储层                                     │ │
│  │  ├─ sdkconfig.defaults (默认配置)                          │ │
│  │  ├─ sdkconfig.defaults.{target} (平台配置)                 │ │
│  │  ├─ sdkconfig (用户配置)                                   │ │
│  │  └─ build/config/sdkconfig.h (C头文件)                    │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              应用程序层                                     │ │
│  │  ├─ 编译时宏定义 (CONFIG_*)                                │ │
│  │  ├─ 条件编译 (#ifdef CONFIG_*)                            │ │
│  │  └─ 运行时配置读取                                          │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 2. 核心文件结构

```
项目根目录/
├── sdkconfig                         # 用户配置文件 (自动生成)
├── sdkconfig.defaults                # 通用默认配置
├── sdkconfig.defaults.esp32s3        # ESP32-S3 平台配置
├── sdkconfig.defaults.esp32c3        # ESP32-C3 平台配置
├── sdkconfig.defaults.esp32p4        # ESP32-P4 平台配置
├── sdkconfig.old                     # 配置备份文件
│
├── main/
│   └── Kconfig.projbuild             # 项目配置定义
│
├── build/
│   └── config/
│       ├── sdkconfig.h               # C头文件 (自动生成)
│       ├── sdkconfig.cmake           # CMake配置 (自动生成)
│       └── sdkconfig.json            # JSON配置 (自动生成)
│
└── managed_components/
    └── */
        └── Kconfig                   # 组件配置定义
```

## SDKConfig 文件详解

### 1. sdkconfig 文件

**用途**: 存储用户的所有配置选项

**特点**:
- 自动生成，不建议手动编辑
- 包含所有配置项的完整值
- 版本控制中应该忽略 (在.gitignore中)

**文件格式**:
```bash
#
# Automatically generated file. DO NOT EDIT.
# Espressif IoT Development Framework (ESP-IDF) 5.4.2 Project Configuration
#

# SoC 硬件能力配置
CONFIG_SOC_CPU_CORES_NUM=2
CONFIG_SOC_CPU_HAS_FPU=y
CONFIG_SOC_WIFI_SUPPORTED=y
CONFIG_SOC_BT_SUPPORTED=y

# 项目特定配置
CONFIG_BOARD_TYPE_ESP32S3_KORVO2_V3=y
CONFIG_USE_CUSTOM_WAKE_WORD=y
CONFIG_CUSTOM_WAKE_WORD="xiao tu dou"
CONFIG_CUSTOM_WAKE_WORD_DISPLAY="小土豆"
CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20

# ESP-IDF 组件配置
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_80M=y
```

### 2. sdkconfig.defaults 文件

**用途**: 定义项目的默认配置

**特点**:
- 手动维护
- 版本控制中应该包含
- 为所有目标平台提供通用配置

**示例配置**:
```bash
# 编译优化
CONFIG_COMPILER_OPTIMIZATION_SIZE=y
CONFIG_COMPILER_CXX_EXCEPTIONS=y
CONFIG_COMPILER_CXX_RTTI=y

# 引导加载程序
CONFIG_BOOTLOADER_COMPILER_OPTIMIZATION_PERF=y
CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y
CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y

# 分区表
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/16m.csv"

# 任务配置
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10

# WiFi 优化
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=6
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=8
CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT=n

# LVGL 配置
CONFIG_LV_OS_NONE=y
CONFIG_LV_USE_OS=0
CONFIG_LV_MEM_CUSTOM=y
```

### 3. 平台特定配置文件

#### sdkconfig.defaults.esp32s3
```bash
# Flash 配置
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y

# CPU 频率
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y

# PSRAM 配置
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=512
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=65536

# 缓存配置
CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y

# 语音识别模型
CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y

# LVGL 图形
CONFIG_LV_USE_SNAPSHOT=y
```

### 4. 生成的配置文件

#### build/config/sdkconfig.h
```c
#pragma once

// 布尔配置转换为宏定义
#define CONFIG_SOC_CPU_CORES_NUM 2
#define CONFIG_SOC_CPU_HAS_FPU 1
#define CONFIG_SPIRAM 1

// 字符串配置
#define CONFIG_CUSTOM_WAKE_WORD "xiao tu dou"
#define CONFIG_CUSTOM_WAKE_WORD_DISPLAY "小土豆"

// 数值配置
#define CONFIG_CUSTOM_WAKE_WORD_THRESHOLD 20
#define CONFIG_ESP_MAIN_TASK_STACK_SIZE 8192
```

## Menuconfig 工作原理

### 1. 系统架构

**基础技术**: 基于 Linux Kconfig 系统和 ncurses 库

**工作流程**:
```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Kconfig 文件   │───▶│  menuconfig     │───▶│   sdkconfig     │
│   (配置定义)     │    │  (配置编辑器)    │    │   (配置文件)     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
         │                       │                       │
         ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│ 依赖关系定义     │    │ 交互式界面      │    │ C 头文件生成     │
│ 默认值设置      │    │ 配置验证        │    │ CMake 变量       │
│ 帮助文档        │    │ 搜索功能        │    │ JSON 导出        │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### 2. Kconfig 语法

#### 2.1 基本配置项
```kconfig
# 布尔选项
config USE_CUSTOM_WAKE_WORD
    bool "Multinet model (Custom Wake Word)"
    depends on (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
    help
        Requires ESP32 S3 and PSRAM

# 字符串选项
config CUSTOM_WAKE_WORD
    string "Custom Wake Word"
    default "xiao tu dou"
    depends on USE_CUSTOM_WAKE_WORD
    help
        Custom Wake Word, use pinyin for Chinese, separated by spaces

# 整数选项
config CUSTOM_WAKE_WORD_THRESHOLD
    int "Custom Wake Word Threshold (%)"
    default 20
    range 1 99
    depends on USE_CUSTOM_WAKE_WORD
    help
        Custom Wake Word Threshold, range 1-99, the smaller the more sensitive
```

#### 2.2 选择组
```kconfig
choice WAKE_WORD_TYPE
    prompt "Wake Word Implementation Type"
    default USE_AFE_WAKE_WORD if (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
    default WAKE_WORD_DISABLED
    help
        Choose the type of wake word implementation to use

    config WAKE_WORD_DISABLED
        bool "Disabled"
        help
            Disable wake word detection

    config USE_ESP_WAKE_WORD
        bool "Wakenet model without AFE"
        depends on IDF_TARGET_ESP32C3 || IDF_TARGET_ESP32C5 || IDF_TARGET_ESP32C6
        help
            Support ESP32 C3、ESP32 C5 与 ESP32 C6

    config USE_AFE_WAKE_WORD
        bool "Wakenet model with AFE"
        depends on (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
        help
            Support AEC if available, requires ESP32 S3 and PSRAM

    config USE_CUSTOM_WAKE_WORD
        bool "Multinet model (Custom Wake Word)"
        depends on (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
        help
            Requires ESP32 S3 and PSRAM
endchoice
```

#### 2.3 菜单组织
```kconfig
menu "Xiaozhi Assistant"
    
    config OTA_URL
        string "Default OTA URL"
        default "https://api.tenclass.net/xiaozhi/ota/"
        help
            The application will access this URL to check for new firmwares

    choice BOARD_TYPE
        prompt "Board Type"
        default BOARD_TYPE_BREAD_COMPACT_WIFI
        help
            Board type. 开发板类型
        
        config BOARD_TYPE_ESP32S3_KORVO2_V3
            bool "ESP32S3 KORVO2 V3"
            depends on IDF_TARGET_ESP32S3
            
        config BOARD_TYPE_ESP_BOX_3
            bool "ESP BOX 3"
            depends on IDF_TARGET_ESP32S3
    endchoice

endmenu
```

### 3. 使用方法

#### 3.1 启动 menuconfig
```bash
# 基本用法
idf.py menuconfig

# 指定目标平台
idf.py set-target esp32s3
idf.py menuconfig

# 保存配置并退出
# 在 menuconfig 界面中：
# - 使用方向键导航
# - 使用空格键或回车键选择
# - 按 'S' 保存配置
# - 按 'Q' 退出
```

#### 3.2 界面操作

**主界面**:
```
┌─────────────────────────────────────────────────────────────────┐
│                    ESP-IDF Configuration                        │
├─────────────────────────────────────────────────────────────────┤
│ ┌─ Xiaozhi Assistant                                          │ │
│ │ ┌─ Component config                                         │ │
│ │ │   ESP System Settings                                     │ │
│ │ │   WiFi                                                    │ │
│ │ │   Bluetooth                                               │ │
│ │ │   LVGL Configuration                                      │ │
│ │ └─ ...                                                     │ │
│ └─ ...                                                        │ │
├─────────────────────────────────────────────────────────────────┤
│     <Select>    < Exit >    < Help >    < Save >    < Load >    │
└─────────────────────────────────────────────────────────────────┘
```

**快捷键**:
- `↑/↓`: 上下导航
- `←/→`: 左右导航  
- `Space/Enter`: 选择/进入
- `/`: 搜索配置项
- `?`: 显示帮助
- `S`: 保存配置
- `Q`: 退出

#### 3.3 搜索功能
```
按 '/' 键打开搜索对话框：

┌─────────────────────────────────────────┐
│ Search Configuration Parameter          │
├─────────────────────────────────────────┤
│ Symbol: CUSTOM_WAKE_WORD                │
├─────────────────────────────────────────┤
│ Location:                               │
│   -> Xiaozhi Assistant                  │
│     -> Custom Wake Word                 │
├─────────────────────────────────────────┤
│ Defined at main/Kconfig.projbuild:528   │
│ Depends on: USE_CUSTOM_WAKE_WORD        │
└─────────────────────────────────────────┘
```

### 4. 配置验证机制

#### 4.1 依赖关系检查
```kconfig
config USE_CUSTOM_WAKE_WORD
    bool "Multinet model (Custom Wake Word)"
    depends on (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
    # 只有在 ESP32S3/P4 且启用 SPIRAM 时才可选

config CUSTOM_WAKE_WORD_THRESHOLD
    int "Custom Wake Word Threshold (%)"
    range 1 99
    depends on USE_CUSTOM_WAKE_WORD
    # 数值范围限制和依赖检查
```

#### 4.2 默认值计算
```kconfig
choice WAKE_WORD_TYPE
    default USE_AFE_WAKE_WORD if (IDF_TARGET_ESP32S3 || IDF_TARGET_ESP32P4) && SPIRAM
    default WAKE_WORD_DISABLED
    # 条件默认值：根据硬件能力选择最佳默认选项
```

## CUSTOM_WAKE_WORD 自定义唤醒词详解

### 1. 配置流程

#### 1.1 menuconfig 配置步骤

**步骤1**: 选择唤醒词类型
```bash
idf.py menuconfig
# 导航到: Xiaozhi Assistant -> Wake Word Implementation Type
# 选择: Multinet model (Custom Wake Word)
```

**步骤2**: 配置唤醒词参数
```bash
# 在 menuconfig 中设置：
# Custom Wake Word: "xiao tu dou"          (拼音，空格分隔)
# Custom Wake Word Display: "小土豆"       (显示文本)
# Custom Wake Word Threshold (%): 20       (检测阈值，1-99)
```

**步骤3**: 保存配置
```bash
# 在 menuconfig 中按 'S' 保存
# 退出后生成的 sdkconfig 包含：
CONFIG_USE_CUSTOM_WAKE_WORD=y
CONFIG_CUSTOM_WAKE_WORD="xiao tu dou"
CONFIG_CUSTOM_WAKE_WORD_DISPLAY="小土豆"
CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20
```

#### 1.2 配置文件解析

**build_default_assets.py 中的解析逻辑**:
```python
def read_custom_wake_word_from_sdkconfig(sdkconfig_path):
    """从 sdkconfig 读取自定义唤醒词配置"""
    config_values = {}
    with io.open(sdkconfig_path, "r") as f:
        for line in f:
            line = line.strip("\n")
            if line.startswith('#') or '=' not in line:
                continue
                
            # 检查自定义唤醒词配置
            if 'CONFIG_USE_CUSTOM_WAKE_WORD=y' in line:
                config_values['use_custom_wake_word'] = True
            elif 'CONFIG_CUSTOM_WAKE_WORD=' in line:
                # 提取字符串值 (去除引号)
                value = line.split('=', 1)[1].strip('"')
                config_values['wake_word'] = value
            elif 'CONFIG_CUSTOM_WAKE_WORD_DISPLAY=' in line:
                value = line.split('=', 1)[1].strip('"')
                config_values['display'] = value
            elif 'CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=' in line:
                value = line.split('=', 1)[1]
                config_values['threshold'] = int(value)
    
    return config_values
```

### 2. 工作原理

#### 2.1 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                  自定义唤醒词系统架构                            │
├─────────────────────────────────────────────────────────────────┤
│  配置层                                                         │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              SDKConfig 配置                                 │ │
│  │  ├─ CONFIG_USE_CUSTOM_WAKE_WORD=y                          │ │
│  │  ├─ CONFIG_CUSTOM_WAKE_WORD="xiao tu dou"                 │ │
│  │  ├─ CONFIG_CUSTOM_WAKE_WORD_DISPLAY="小土豆"               │ │
│  │  └─ CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20                  │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              模型层                                         │ │
│  │              Multinet (ESP-SR)                             │ │
│  │  ├─ 语音识别模型加载                                        │ │
│  │  ├─ 命令词注册                                              │ │
│  │  ├─ 检测阈值设置                                            │ │
│  │  └─ 实时音频处理                                            │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              应用层                                         │ │
│  │              CustomWakeWord 类                             │ │
│  │  ├─ 音频数据输入                                            │ │
│  │  ├─ 唤醒词检测                                              │ │
│  │  ├─ 回调触发                                                │ │
│  │  └─ 音频数据编码                                            │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

#### 2.2 核心数据结构

```cpp
class CustomWakeWord : public WakeWord {
private:
    struct Command {
        std::string command;    // 命令词 (如: "xiao tu dou")
        std::string text;       // 显示文本 (如: "小土豆")
        std::string action;     // 动作类型 (如: "wake")
    };

    // Multinet 相关成员
    esp_mn_iface_t* multinet_ = nullptr;           // Multinet 接口
    model_iface_data_t* multinet_model_data_ = nullptr; // 模型数据
    srmodel_list_t* models_ = nullptr;             // 模型列表
    char* mn_name_ = nullptr;                      // 模型名称
    
    // 配置参数
    std::string language_ = "cn";                  // 语言 (中文)
    int duration_ = 3000;                          // 检测持续时间 (ms)
    float threshold_ = 0.2;                        // 检测阈值 (0.0-1.0)
    std::deque<Command> commands_;                 // 命令词列表
    
    // 音频处理
    std::deque<std::vector<int16_t>> wake_word_pcm_;     // PCM 音频缓冲
    std::deque<std::vector<uint8_t>> wake_word_opus_;    // Opus 编码缓冲
    std::mutex wake_word_mutex_;                         // 线程同步
    std::condition_variable wake_word_cv_;               // 条件变量
};
```

### 3. 工作流程

#### 3.1 初始化流程

```cpp
bool CustomWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    codec_ = codec;
    commands_.clear();

    // 1. 读取配置或使用默认值
    if (models_list == nullptr) {
        language_ = "cn";
        models_ = esp_srmodel_init("model");
#ifdef CONFIG_CUSTOM_WAKE_WORD
        // 从 SDKConfig 读取配置
        threshold_ = CONFIG_CUSTOM_WAKE_WORD_THRESHOLD / 100.0f;  // 20 -> 0.2
        commands_.push_back({
            CONFIG_CUSTOM_WAKE_WORD,        // "xiao tu dou"
            CONFIG_CUSTOM_WAKE_WORD_DISPLAY, // "小土豆"
            "wake"                           // 动作类型
        });
#endif
    } else {
        // 从资源包读取配置
        models_ = models_list;
        ParseWakenetModelConfig();
    }

    // 2. 验证模型
    if (models_ == nullptr || models_->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }

    // 3. 初始化 Multinet
    mn_name_ = esp_srmodel_filter(models_, ESP_MN_PREFIX, language_.c_str());
    if (mn_name_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize multinet, mn_name is nullptr");
        return false;
    }

    // 4. 创建 Multinet 实例
    multinet_ = esp_mn_handle_from_name(mn_name_);
    multinet_model_data_ = multinet_->create(mn_name_, duration_);
    
    // 5. 设置检测阈值
    multinet_->set_det_threshold(multinet_model_data_, threshold_);
    
    // 6. 注册命令词
    esp_mn_commands_clear();
    for (int i = 0; i < commands_.size(); i++) {
        esp_mn_commands_add(i + 1, commands_[i].command.c_str());
    }
    esp_mn_commands_update();
    
    // 7. 打印激活的命令词
    multinet_->print_active_speech_commands(multinet_model_data_);
    return true;
}
```

#### 3.2 音频检测流程

```cpp
void CustomWakeWord::Feed(const std::vector<int16_t>& data) {
    if (multinet_model_data_ == nullptr || !running_) {
        return;
    }

    esp_mn_state_t mn_state;
    
    // 1. 处理双声道音频 (提取左声道)
    if (codec_->input_channels() == 2) {
        auto mono_data = std::vector<int16_t>(data.size() / 2);
        for (size_t i = 0, j = 0; i < mono_data.size(); ++i, j += 2) {
            mono_data[i] = data[j];
        }
        
        StoreWakeWordData(mono_data);
        mn_state = multinet_->detect(multinet_model_data_, 
                                   const_cast<int16_t*>(mono_data.data()));
    } else {
        StoreWakeWordData(data);
        mn_state = multinet_->detect(multinet_model_data_, 
                                   const_cast<int16_t*>(data.data()));
    }
    
    // 2. 处理检测结果
    if (mn_state == ESP_MN_STATE_DETECTING) {
        // 正在检测中，继续等待
        return;
    } else if (mn_state == ESP_MN_STATE_DETECTED) {
        // 检测到命令词
        esp_mn_results_t *mn_result = multinet_->get_results(multinet_model_data_);
        for (int i = 0; i < mn_result->num && running_; i++) {
            ESP_LOGI(TAG, "Custom wake word detected: command_id=%d, string=%s, prob=%f", 
                    mn_result->command_id[i], mn_result->string, mn_result->prob[i]);
            
            auto& command = commands_[mn_result->command_id[i] - 1];
            if (command.action == "wake") {
                last_detected_wake_word_ = command.text;  // "小土豆"
                running_ = false;
                
                // 触发回调
                if (wake_word_detected_callback_) {
                    wake_word_detected_callback_(last_detected_wake_word_);
                }
            }
        }
        multinet_->clean(multinet_model_data_);
    } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
        // 检测超时，清理状态
        ESP_LOGD(TAG, "Command word detection timeout, cleaning state");
        multinet_->clean(multinet_model_data_);
    }
}
```

#### 3.3 音频数据存储

```cpp
void CustomWakeWord::StoreWakeWordData(const std::vector<int16_t>& data) {
    // 存储音频数据到缓冲区
    wake_word_pcm_.push_back(data);
    
    // 保持约2秒的音频数据 (检测间隔30ms，采样率16kHz，块大小512)
    while (wake_word_pcm_.size() > 2000 / 30) {
        wake_word_pcm_.pop_front();
    }
}
```

### 4. 配置参数详解

#### 4.1 CUSTOM_WAKE_WORD (唤醒词)

**格式**: 拼音字符串，空格分隔
```bash
CONFIG_CUSTOM_WAKE_WORD="xiao tu dou"
```

**规则**:
- 使用拼音表示中文
- 多个音节用空格分隔
- 不包含声调符号
- 长度建议2-4个音节

**示例**:
```bash
"ni hao"           # 你好
"xiao zhi"         # 小智  
"xiao tu dou"      # 小土豆
"kai shi gong zuo" # 开始工作
```

#### 4.2 CUSTOM_WAKE_WORD_DISPLAY (显示文本)

**格式**: UTF-8 中文字符串
```bash
CONFIG_CUSTOM_WAKE_WORD_DISPLAY="小土豆"
```

**用途**:
- 用户界面显示
- 日志输出
- 服务器通信

#### 4.3 CUSTOM_WAKE_WORD_THRESHOLD (检测阈值)

**格式**: 整数百分比 (1-99)
```bash
CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20
```

**转换**: 百分比转浮点数
```cpp
threshold_ = CONFIG_CUSTOM_WAKE_WORD_THRESHOLD / 100.0f;  // 20 -> 0.2
```

**调优指南**:
- **低阈值 (1-15)**: 高灵敏度，容易误触发
- **中阈值 (16-30)**: 平衡灵敏度和准确性 (推荐)
- **高阈值 (31-50)**: 低灵敏度，减少误触发
- **很高阈值 (51-99)**: 很难触发，可能漏检

### 5. 资源包配置

#### 5.1 index.json 配置格式

当使用资源包时，配置存储在 `index.json` 中：
```json
{
    "version": 1,
    "multinet_model": {
        "language": "cn",
        "duration": 3000,
        "threshold": 0.2,
        "commands": [
            {
                "command": "xiao tu dou",
                "text": "小土豆",
                "action": "wake"
            },
            {
                "command": "ting zhi",
                "text": "停止",
                "action": "stop"
            }
        ]
    }
}
```

#### 5.2 配置解析

```cpp
void CustomWakeWord::ParseWakenetModelConfig() {
    // 1. 读取 index.json
    auto& assets = Assets::GetInstance();
    void* ptr = nullptr;
    size_t size = 0;
    if (!assets.GetAssetData("index.json", ptr, size)) {
        ESP_LOGE(TAG, "Failed to read index.json");
        return;
    }
    
    // 2. 解析 JSON
    cJSON* root = cJSON_ParseWithLength(static_cast<char*>(ptr), size);
    cJSON* multinet_model = cJSON_GetObjectItem(root, "multinet_model");
    
    // 3. 提取配置参数
    if (cJSON_IsObject(multinet_model)) {
        cJSON* language = cJSON_GetObjectItem(multinet_model, "language");
        cJSON* duration = cJSON_GetObjectItem(multinet_model, "duration");
        cJSON* threshold = cJSON_GetObjectItem(multinet_model, "threshold");
        cJSON* commands = cJSON_GetObjectItem(multinet_model, "commands");
        
        if (cJSON_IsString(language)) {
            language_ = language->valuestring;
        }
        if (cJSON_IsNumber(duration)) {
            duration_ = duration->valueint;
        }
        if (cJSON_IsNumber(threshold)) {
            threshold_ = threshold->valuedouble;
        }
        
        // 4. 解析命令词列表
        if (cJSON_IsArray(commands)) {
            for (int i = 0; i < cJSON_GetArraySize(commands); i++) {
                cJSON* command = cJSON_GetArrayItem(commands, i);
                if (cJSON_IsObject(command)) {
                    cJSON* command_name = cJSON_GetObjectItem(command, "command");
                    cJSON* text = cJSON_GetObjectItem(command, "text");
                    cJSON* action = cJSON_GetObjectItem(command, "action");
                    
                    if (cJSON_IsString(command_name) && 
                        cJSON_IsString(text) && 
                        cJSON_IsString(action)) {
                        commands_.push_back({
                            command_name->valuestring,
                            text->valuestring,
                            action->valuestring
                        });
                    }
                }
            }
        }
    }
    
    cJSON_Delete(root);
}
```

### 6. 性能优化

#### 6.1 内存管理

**音频缓冲区优化**:
```cpp
void CustomWakeWord::StoreWakeWordData(const std::vector<int16_t>& data) {
    wake_word_pcm_.push_back(data);
    
    // 限制缓冲区大小，避免内存泄漏
    // 2秒音频 = 2000ms / 30ms = ~67 个音频块
    while (wake_word_pcm_.size() > 2000 / 30) {
        wake_word_pcm_.pop_front();
    }
}
```

**模型内存优化**:
```cpp
CustomWakeWord::~CustomWakeWord() {
    // 清理 Multinet 资源
    if (multinet_model_data_ != nullptr) {
        multinet_->destroy(multinet_model_data_);
    }
    
    // 清理模型列表
    if (models_ != nullptr) {
        esp_srmodel_deinit(models_);
    }
}
```

#### 6.2 计算优化

**单声道转换优化**:
```cpp
// 优化前：创建新向量
auto mono_data = std::vector<int16_t>(data.size() / 2);
for (size_t i = 0, j = 0; i < mono_data.size(); ++i, j += 2) {
    mono_data[i] = data[j];
}

// 优化后：预分配缓冲区，减少内存分配
static std::vector<int16_t> mono_buffer;
mono_buffer.resize(data.size() / 2);
for (size_t i = 0, j = 0; i < mono_buffer.size(); ++i, j += 2) {
    mono_buffer[i] = data[j];
}
```

#### 6.3 实时性优化

**检测状态优化**:
```cpp
void CustomWakeWord::Feed(const std::vector<int16_t>& data) {
    // 快速退出条件检查
    if (multinet_model_data_ == nullptr || !running_) {
        return;
    }
    
    // 使用原地检测，减少数据拷贝
    esp_mn_state_t mn_state = multinet_->detect(
        multinet_model_data_, 
        const_cast<int16_t*>(data.data())
    );
    
    // 状态机优化：减少不必要的处理
    switch (mn_state) {
        case ESP_MN_STATE_DETECTING:
            return;  // 快速返回，继续检测
            
        case ESP_MN_STATE_DETECTED:
            HandleDetection();
            break;
            
        case ESP_MN_STATE_TIMEOUT:
            multinet_->clean(multinet_model_data_);
            break;
    }
}
```

### 7. 错误处理

#### 7.1 常见错误

**模型加载失败**:
```cpp
mn_name_ = esp_srmodel_filter(models_, ESP_MN_PREFIX, language_.c_str());
if (mn_name_ == nullptr) {
    ESP_LOGE(TAG, "Failed to initialize multinet, mn_name is nullptr");
    ESP_LOGI(TAG, "Please refer to https://pcn7cs20v8cr.feishu.cn/wiki/CpQjwQsCJiQSWSkYEvrcxcbVnwh to add custom wake word");
    return false;
}
```

**配置解析错误**:
```cpp
try {
    config_values['threshold'] = int(value)
} except ValueError:
    try:
        config_values['threshold'] = float(value)
    except ValueError:
        print(f"Warning: Invalid threshold value: {value}")
        config_values['threshold'] = 20  # 使用默认值
```

#### 7.2 调试方法

**启用调试日志**:
```bash
# 在 menuconfig 中设置
Component config -> Log output -> Default log verbosity -> Debug
```

**检查模型状态**:
```cpp
multinet_->print_active_speech_commands(multinet_model_data_);
// 输出: Active speech commands: [1]xiao tu dou
```

**监控检测概率**:
```cpp
ESP_LOGI(TAG, "Custom wake word detected: command_id=%d, string=%s, prob=%f", 
         mn_result->command_id[i], mn_result->string, mn_result->prob[i]);
```

### 8. 最佳实践

#### 8.1 唤醒词选择

**推荐原则**:
- 长度适中 (2-4个音节)
- 发音清晰，不易混淆
- 避免常用词汇，减少误触发
- 考虑不同口音和语速

**好的例子**:
```bash
"xiao zhi"      # 小智 - 简短明确
"ni hao ya"     # 你好呀 - 友好自然
"kai shi ba"    # 开始吧 - 动作明确
```

**避免的例子**:
```bash
"a"             # 太短，容易误触发
"zhe ge na ge"  # 太长，不易记忆
"shi de"        # 太常用，误触发率高
```

#### 8.2 阈值调优

**调优步骤**:
1. 从默认值 20 开始
2. 测试正常使用场景
3. 记录误触发和漏检情况
4. 根据实际表现调整

**环境适配**:
- 安静环境: 可使用较低阈值 (15-25)
- 嘈杂环境: 需要较高阈值 (25-35)
- 多用户环境: 适中阈值，平衡各用户体验

#### 8.3 多语言支持

**配置示例**:
```json
{
    "multinet_model": {
        "language": "en",
        "commands": [
            {
                "command": "hello there",
                "text": "Hello There",
                "action": "wake"
            }
        ]
    }
}
```

## 总结

小智AI聊天机器人的 SDKConfig 系统是一个完整的配置管理解决方案，通过 Kconfig 语法定义、menuconfig 编辑器和多层配置文件，实现了灵活的项目配置管理。

### 主要优势

1. **层次化配置**: 通用配置、平台配置、用户配置的清晰分离
2. **交互式编辑**: menuconfig 提供友好的配置界面
3. **依赖管理**: 自动处理配置项之间的依赖关系
4. **类型安全**: 强类型检查和范围验证
5. **文档集成**: 内置帮助系统和配置说明

### 技术亮点

1. **CUSTOM_WAKE_WORD 系统**: 完整的自定义唤醒词解决方案
2. **多平台适配**: 根据目标芯片自动选择合适配置
3. **性能优化**: 针对嵌入式平台的内存和计算优化
4. **错误处理**: 完善的错误检测和恢复机制
5. **调试支持**: 丰富的日志和调试信息

### 应用价值

1. **开发效率**: 简化复杂配置的管理和维护
2. **用户体验**: 支持个性化的唤醒词定制
3. **平台兼容**: 统一的配置系统支持多种硬件平台
4. **可维护性**: 清晰的配置结构便于项目维护和扩展

这套配置系统为 ESP32 平台的项目开发提供了一个优秀的参考实现，具有很高的学习和应用价值。
