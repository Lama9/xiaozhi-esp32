# DHT11温湿度传感器

## 概述

本项目为ATK-DNESp32S3板子添加了DHT11温湿度传感器支持，使用ESP-IDF-Lib的dht组件实现。

## 实现方案

- **驱动库**: ESP-IDF-Lib/dht
- **连接方式**: GPIO直连（不使用XL9555扩展）
- **数据引脚**: GPIO2（可在config.h中配置）
- **读取间隔**: 5秒
- **重试机制**: 最多3次重试

## 文件结构

```
main/sensors/
├── dht11_sensor.h      # DHT11传感器类声明
├── dht11_sensor.cc     # DHT11传感器类实现
└── README.md          # 说明文档
```

## 硬件连接

```
DHT11传感器连接：
DHT11 VCC  ---- 3.3V
DHT11 GND  ---- GND  
DHT11 DATA ---- ESP32-S3 GPIO2（DHT11_DATA_PIN）
               上拉电阻10kΩ到3.3V（可选）
```

## MCP工具接口

该实现提供了以下MCP工具：

### 1. 温度查询
- **工具名**: `self.atk_dnesp32s3.get_temperature`
- **描述**: 获取当前环境温度（基于ESP-IDF-Lib DHT驱动）
- **返回值**: 温度值字符串（单位：°C），失败时返回"-999.0"

### 2. 湿度查询
- **工具名**: `self.atk_dnesp32s3.get_humidity`
- **描述**: 获取当前环境湿度（基于ESP-IDF-Lib DHT驱动）
- **返回值**: 湿度值字符串（单位：%），失败时返回"-1.0"

### 3. 完整气象数据
- **工具名**: `self.atk_dnesp32s3.get_weather_data`
- **描述**: 获取当前环境温湿度数据（基于ESP-IDF-Lib DHT驱动）
- **返回值**: JSON格式的完整传感器信息

### 4. 启用DHT11监测
- **工具名**: `self.atk_dnesp32s3.enable_dht11`
- **描述**: 启用DHT11温湿度传感器监测功能
- **返回值**: 操作结果字符串
- **功能**: 启动传感器和定时读取任务

### 5. 禁用DHT11监测
- **工具名**: `self.atk_dnesp32s3.disable_dht11`
- **描述**: 禁用DHT11温湿度传感器监测功能
- **返回值**: 操作结果字符串
- **功能**: 停止传感器和定时读取任务

### 6. 查询DHT11状态
- **工具名**: `self.atk_dnesp32s3.get_dht11_status`
- **描述**: 查询DHT11温湿度传感器当前状态
- **返回值**: JSON格式的状态信息

### JSON返回格式

#### 气象数据格式
```json
{
  "driver": "esp-idf-lib",
  "temperature": 25.6,
  "humidity": 45.2,
  "is_valid": true,
  "timestamp": 1703123456789,
  "retry_count": 0
}
```

#### 状态查询格式
```json
{
  "enabled": true,
  "timer_running": true,
  "last_read_time": 1703123456789
}
```

## 配置参数

在`main/boards/atk-dnesp32s3/config.h`中配置：

```cpp
#define DHT11_DATA_PIN  GPIO_NUM_2          // DHT11数据引脚
#define DHT11_READ_INTERVAL_MS 5000         // 读取间隔(毫秒)
#define DHT11_MAX_RETRY_COUNT 3             // 最大重试次数
#define DHT11_TIMEOUT_MS 2000               // 超时时间(毫秒)
```

## 代码集成

### 初始化
在ATK-DNESp32S3板子的构造函数中，DHT11传感器会被自动初始化：

```cpp
atk_dnesp32s3() : boot_button_(BOOT_BUTTON_GPIO) {
    InitializeI2c();
    InitializeSpi();
    InitializeSt7789Display();
    InitializeDHT11();        // 初始化DHT11传感器
    InitializeButtons();
    InitializeTools();         // 注册MCP工具
    InitializeCamera();
}
```

### 定期读取
在`Run()`方法中会定期读取传感器数据：

```cpp
virtual void Run() override {
    ReadDHT11Data();  // 定期读取DHT11数据
    WifiBoard::Run(); // 调用父类Run方法
}
```

## 日志输出示例

```
I (12345) atk_dnesp32s3: Initializing DHT11 sensor on GPIO 2
I (12346) DHT11_Sensor: DHT11Sensor created with GPIO 2
I (12347) DHT11_Sensor: Initializing DHT11 sensor on GPIO 2
I (12348) DHT11_Sensor: Testing DHT11 connection on GPIO 2...
I (12349) DHT11_Sensor: DHT11 read success: Temperature=25.6°C, Humidity=45.2%
I (12350) DHT11_Sensor: DHT11 connection test passed - Temp: 25.6°C, Humidity: 45.2%
I (12351) DHT11_Sensor: DHT11 sensor initialized successfully
I (12352) atk_dnesp32s3: DHT11 sensor test successful - Temp: 25.6°C, Humidity: 45.2%
I (12353) MCP: Add tool: self.atk_dnesp32s3.get_temperature
I (12354) MCP: Add tool: self.atk_dnesp32s3.get_humidity  
I (12355) MCP: Add tool: self.atk_dnesp32s3.get_weather_data
```

## 使用示例

### AI查询示例

**启用DHT11监测：**
```
用户: "开启温湿度监测"
AI调用: self.atk_dnesp32s3.enable_dht11
返回: "DHT11温湿度监测功能已启用"
回复: "温湿度监测功能已开启，现在开始监测环境温湿度"
```

**查询当前温度：**
```
用户: "现在温度是多少？"
AI调用: self.atk_dnesp32s3.get_temperature
返回: "25.6"
回复: "当前的温度是25.6°C"
```

**查询当前湿度：**
```
用户: "湿度如何？"
AI调用: self.atk_dnesp32s3.get_humidity  
返回: "45.2"
回复: "当前的湿度是45.2%"
```

**查询完整气象数据：**
```
用户: "告诉我当前的环境状况"
AI调用: self.atk_dnesp32s3.get_weather_data
返回: {"driver":"esp-idf-lib","temperature":25.6,"humidity":45.2,"is_valid":true,"timestamp":1703123456789,"retry_count":0}
回复: "当前环境状况：温度25.6°C，湿度45.2%，数据有效"
```

**查询DHT11状态：**
```
用户: "温湿度传感器状态如何？"
AI调用: self.atk_dnesp32s3.get_dht11_status
返回: {"enabled":true,"timer_running":true,"last_read_time":1703123456789}
回复: "温湿度传感器已启用，定时器正在运行，最后读取时间正常"
```

**禁用DHT11监测：**
```
用户: "关闭温湿度监测"
AI调用: self.atk_dnesp32s3.disable_dht11
返回: "DHT11温湿度监测功能已禁用"
回复: "温湿度监测功能已关闭，停止监测环境温湿度"
```

## 错误处理

### 传感器读取失败
- 如果连续读取失败，会增加重试计数
- 失败时返回特定的错误值：温度-999.0，湿度-1.0
- JSON格式返回`"is_valid":false`

### 常见问题
1. **GPIO冲突**: 确保DHT11_DATA_PIN未与其他功能冲突
2. **硬件连接**: 检查传感器连接和电源供应
3. **上拉电阻**: 如传感器内置上拉电阻可不外加，否则需要10kΩ上拉电阻

## 依赖说明

### ESP-IDF-Lib组件
- **组件名称**: esp-idf-lib/dht
- **版本**: 最新版本（在idf_component.yml中指定）
- **功能**: 提供dht_read_float()等DHT传感器API

### CMakeLists.txt配置
```cmake
# 已自动添加源文件和头文件路径
set(SOURCES 
    # ... other sources ...
    "sensors/dht11_sensor.cc"
    )

set(INCLUDE_DIRS 
    "sensors"
    # ... other includes ...
    )
```

## 语音开关功能

### 功能特性
1. **默认关闭**: 每次启动时DHT11监测功能默认关闭
2. **语音控制**: 支持通过语音指令开启/关闭监测功能
3. **彻底关闭**: 关闭时完全停止定时器和传感器读取
4. **状态查询**: 可随时查询当前开关状态
5. **资源管理**: 智能管理定时器资源，避免内存泄漏

### 开关状态管理
- **启用**: 启动传感器连接测试，创建并启动定时器
- **禁用**: 停止定时器，删除定时器资源，禁用传感器
- **状态检查**: 读取数据前检查传感器是否启用
- **线程安全**: 使用互斥锁保护状态变量

### 语音指令示例
- "开启温湿度监测" → 启用DHT11功能
- "关闭温湿度监测" → 禁用DHT11功能  
- "温湿度传感器状态" → 查询当前状态
- "现在温度多少" → 查询温度（需要先启用）

## 技术优势

1. **稳定性**: 使用成熟的ESP-IDF-Lib组件
2. **简洁性**: 代码简洁，易于维护
3. **标准化**: 遵循项目MCP协议规范
4. **错误处理**: 完善的错误检测和恢复机制
5. **性能**: 定时读取，避免频繁查询影响性能
6. **可控性**: 支持语音开关，按需启用功能
7. **资源管理**: 智能管理定时器资源，避免浪费

---

**注意**: 该实现专注于稳定性，适合生产环境使用。如需XL9555扩展支持或更高级的自定义功能，请参考readme_DHT.md中的自定义驱动方案。
