# 果云 AI小智扩展板（LCD版）

增加对果云AI小智扩展板 硬件V1.6版 的支持
需要配合果云版小智AI ESP32-S3核心板 兼容DevKitC-N16R8 GoouuuESP32开发板
支持了最新的音量加按钮，电池电量显示，电池充电状态显示，LCD显示屏显示。

## 📋 功能特性

- ✅ 语音识别与合成
- ✅ WiFi连接配置
- ✅ 电池电量监测
- ✅ 充电状态显示
- ✅ 音量控制按钮
- ✅ LCD显示屏支持
- ✅ 多语言支持

产品链接：[ 小智AI ESP32-S3核心板 兼容DevKitC-N16R8 GoouuuESP32开发板 ] (<https://item.taobao.com/item.htm?id=659498263638>)

## 🛠️ 硬件连接

硬件基于果云ESP32-S3开发板+果云小智AI扩展板，代码基于bread-compact-wifi修改

### 主要引脚连接

| 功能 | GPIO引脚 | 说明 |
|------|----------|------|
| 电池电量ADC | GPIO11 | 电池电量检测 |
| 充电状态CHGR | GPIO12 | 充电状态检测 |
| 音量加按钮 | GPIO38 | 音量增加控制 |
| 音量减按钮 | GPIO39 | 音量减少控制 |
| LCD背光控制 | GPIO42 | LCD背光开关 |
| LCD数据线 | GPIO47 | SPI MOSI |
| LCD时钟线 | GPIO21 | SPI CLK |
| LCD数据/命令 | GPIO40 | DC控制引脚 |
| LCD复位 | GPIO45 | RST复位引脚 |
| LCD片选 | GPIO41 | CS片选引脚 |

### 音频接口
- I2S音频输入/输出接口
- 支持麦克风和扬声器

### 支持的LCD类型
- ST7789系列 (240x320, 240x240, 240x135等)
- ST7735系列 (128x160, 128x128)
- ST7796系列 (320x480)
- ILI9341系列 (240x320)
- GC9A01系列 (240x240)

## 🛠️ 编译指南

### 开发环境要求
- ESP-IDF v5.4.1+
- Python 3.8+
- Git

### 编译步骤

**1. 配置编译目标为 ESP32S3：**

```bash
idf.py set-target esp32s3
```

**2. 打开 menuconfig：**

```bash
idf.py menuconfig
```

**3. 选择板子：**

```
Xiaozhi Assistant -> Board Type -> 果云小智扩展版（LCD）
```

**4. 选择LCD屏幕类型：**

```
Xiaozhi Assistant -> LCD Type -> 选择对应的LCD型号
```

**5. 编译烧入：**

```bash
idf.py build flash
```

### 注意事项
- 确保硬件连接正确
- 根据实际使用的LCD型号选择对应的配置
- 首次使用需要配置WiFi网络
- 支持OTA在线升级功能
- 连线方式参考config.h文件中对引脚的定义
