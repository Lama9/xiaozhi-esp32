# 果云 AI小智扩展板（摄像头版）

增加对果云AI小智CAM扩展板的支持
需要配合果云版小智AI ESP32-S3-CAM核心板 兼容DevKitC-N16R8 GoouuuESP32开发板
支持了最新的音量加按钮，电池电量显示，电池充电状态显示，摄像头功能。

## 📋 功能特性

- ✅ 语音识别与合成
- ✅ WiFi连接配置
- ✅ 电池电量监测
- ✅ 充电状态显示
- ✅ 音量控制按钮
- ✅ 摄像头图像采集
- ✅ LCD显示屏支持
- ✅ 多语言支持

产品链接：[ 小智AI ESP32-S3核心板 兼容DevKitC-N16R8 GoouuuESP32开发板 ] (<https://item.taobao.com/item.htm?id=659498263638>)

## 🛠️ 硬件连接

硬件基于ESP32S3CAM开发板，代码基于bread-compact-wifi-lcd修改
使用的摄像头是OV2640
注意因为摄像头占用IO较多，所以占用了ESP32S3的USB 19 20两个引脚

### 主要引脚连接

| 功能 | GPIO引脚 | 说明 |
|------|----------|------|
| 摄像头数据线 | GPIO11,9,8,10,12,18,17,16 | OV2640摄像头数据接口 |
| 摄像头控制线 | GPIO15,13,6,7,5,4 | 摄像头时钟和控制信号 |
| 音量加按钮 | GPIO38 | 音量增加控制 |
| 音量减按钮 | GPIO39 | 音量减少控制 |
| LCD显示屏 | SPI接口 | 支持多种LCD显示屏 |

### 音频接口
- I2S音频输入/输出接口
- 支持麦克风和扬声器

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
Xiaozhi Assistant -> Board Type -> 果云小智扩展版（Camera）
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
- 摄像头占用较多GPIO引脚，注意引脚冲突
- 首次使用需要配置WiFi网络
- 支持OTA在线升级功能
- 连线方式参考config.h文件中对引脚的定义