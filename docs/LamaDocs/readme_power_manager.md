# 基于ESP32-S3的电池管理系统技术说明书

## 概述

本文档详细阐述了基于ESP32-S3平台的电池管理系统实现，包括硬件原理、软件架构、关键算法和移植指南。该系统适用于带有锂电池充电管理芯片的ESP32-S3开发板。

## 硬件架构

### 1. 电源管理电路拓扑

```
电池 → 5V升压 → 3.3V LDO → ESP32-S3
  ↓
充电管理芯片 (TP4054)
  ↓
充电状态指示 (GPIO19)
  ↓
电池电压检测 (GPIO20)
```

### 2. 关键硬件组件

#### 2.1 电池充电管理 (TP4054)
- **功能**: 锂电池充电管理
- **充电电流设置**: 通过PROG引脚电阻设置 (1.5K → 660mA)
- **充电状态指示**: CHGR引脚 (开漏输出)
  - 充电时: CHGR = 0 (低电平)
  - 未充电/充满: CHGR = 高阻态

#### 2.2 电压检测电路
- **分压器**: R5(1.5K) + R6(4.7K)
- **分压比**: 0.758 (4.7K / (1.5K + 4.7K))
- **电压范围**: 电池3.0V-4.2V → ADC 2.274V-3.184V

#### 2.3 电源转换链
- **5V升压**: SX1308 (电池 → 5V)
- **3.3V LDO**: AMS1117-3.3 (5V → 3.3V)

## 软件架构

### 1. 核心类设计

```cpp
class PowerManager {
private:
    gpio_num_t charging_pin_;           // 充电状态检测引脚
    adc_oneshot_unit_handle_t adc_handle_;  // ADC句柄
    std::vector<uint16_t> adc_values_;  // ADC值缓存
    uint32_t battery_level_;            // 电池电量百分比
    bool is_charging_;                  // 充电状态
    bool is_low_battery_;              // 低电量状态
};
```

### 2. 关键算法

#### 2.1 ADC值到电池电压转换

```cpp
// 步骤1: ADC值转换为电压
float adc_voltage = (float)adc_value * 3.3f / 4095.0f;

// 步骤2: 考虑分压器，反推电池电压
float battery_voltage = adc_voltage / 0.758f;
```

#### 2.2 电池电量映射算法

```cpp
// 基于实际硬件校准的映射表
const struct {
    uint16_t adc;      // ADC原始值
    uint8_t level;     // 电量百分比
} levels[] = {
    {2815, 0},   // 3.0V电池电压
    {3000, 20},  // 3.3V电池电压
    {3200, 40},  // 3.6V电池电压
    {3400, 60},  // 3.8V电池电压
    {3600, 80},  // 4.0V电池电压
    {3940, 100}  // 4.2V电池电压
};
```

#### 2.3 充电状态检测

```cpp
// TP4054充电状态检测逻辑
bool new_charging_status = gpio_get_level(charging_pin_) == 0;
// 0 = 正在充电, 1 = 未充电/充满
```

## 技术要点

### 1. ADC配置要点

#### 1.1 引脚映射
- **GPIO20** → **ADC2_CH9** (ESP32-S3)
- **ADC单元**: ADC_UNIT_2
- **ADC通道**: ADC_CHANNEL_9

#### 1.2 ADC参数配置
```cpp
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_2,           // 使用ADC2单元
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};

adc_oneshot_chan_cfg_t chan_config = {
    .atten = ADC_ATTEN_DB_12,        // 12dB衰减，支持0-3.3V
    .bitwidth = ADC_BITWIDTH_12,     // 12位精度
};
```

### 2. 数据滤波策略

#### 2.1 滑动平均滤波
```cpp
// 维护3个ADC值的滑动窗口
std::vector<uint16_t> adc_values_;
const int kBatteryAdcDataCount = 3;

// 计算平均值
uint32_t average_adc = 0;
for (auto value : adc_values_) {
    average_adc += value;
}
average_adc /= adc_values_.size();
```

#### 2.2 采样频率控制
```cpp
const int kBatteryAdcInterval = 60;  // 每60秒读取一次
const int kDebugInterval = 10;       // 每10次检查输出调试信息
```

### 3. 低电量检测

```cpp
const int kLowBatteryLevel = 20;  // 低电量阈值20%

bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
```

## 调试与校准

### 1. 调试信息输出

#### 1.1 详细调试信息 (每10次检查)
```
=== 电池状态调试信息 ===
原始ADC值: 3200
平均ADC值: 3180
ADC电压: 2.560V
计算电池电压: 3.377V
电池电量: 40%
充电状态: 未充电
低电量状态: 否
GPIO19电平: 1
========================
```

#### 1.2 普通日志 (每次检查)
```
ADC: 3200, 平均: 3180, 电压: 3.377V, 电量: 40%, 充电: 否
```

### 2. 校准步骤

#### 2.1 硬件校准
1. 使用万用表测量实际电池电压
2. 对比串口输出的"计算电池电压"
3. 调整分压比或ADC参考电压

#### 2.2 软件校准
1. 记录不同电池电压下的ADC值
2. 更新`levels[]`映射表
3. 验证电量百分比准确性

## 平台移植指南

### 1. 硬件适配

#### 1.1 引脚配置
```cpp
// 需要修改的引脚定义
#define CHARGING_PIN    GPIO_NUM_19    // 充电状态检测引脚
#define BATTERY_ADC_PIN GPIO_NUM_20    // 电池电压检测引脚
```

#### 1.2 ADC配置
```cpp
// 根据目标平台修改ADC配置
adc_unit_t adc_unit = ADC_UNIT_2;      // ESP32-S3使用ADC2
adc_channel_t adc_channel = ADC_CHANNEL_9;  // GPIO20对应ADC2_CH9
```

### 2. 软件适配

#### 2.1 分压比调整
```cpp
// 根据实际硬件分压器调整
const float VOLTAGE_DIVIDER_RATIO = 0.758f;  // 4.7K/(1.5K+4.7K)
```

#### 2.2 电池电量映射表
```cpp
// 根据实际电池类型和硬件校准
const struct {
    uint16_t adc;
    uint8_t level;
} levels[] = {
    // 需要根据实际测试结果调整
};
```

### 3. 不同平台的特殊处理

#### 3.1 ESP32 (非S3)
- ADC单元可能不同
- GPIO引脚映射不同
- 需要检查ADC通道对应关系

#### 3.2 其他MCU平台
- ADC精度可能不同 (非12位)
- 参考电压可能不同 (非3.3V)
- GPIO配置方式不同

## 性能优化

### 1. 功耗优化
- 降低ADC采样频率
- 使用定时器而非轮询
- 在低电量时减少功能

### 2. 精度优化
- 增加ADC采样次数
- 使用更精确的参考电压
- 温度补偿 (如需要)

### 3. 稳定性优化
- 增加数据滤波
- 异常值检测和处理
- 看门狗保护

## 常见问题与解决方案

### 1. ADC读取异常
- **问题**: ADC值不稳定或为0
- **解决**: 检查GPIO配置、ADC通道、参考电压

### 2. 充电状态检测错误
- **问题**: 充电状态不准确
- **解决**: 检查GPIO19配置、TP4054连接

### 3. 电池电量不准确
- **问题**: 电量百分比偏差大
- **解决**: 重新校准映射表、检查分压比

### 4. 低电量误报
- **问题**: 频繁低电量警告
- **解决**: 调整阈值、增加滤波

## 扩展功能

### 1. 温度补偿
```cpp
// 根据温度调整电池电压计算
float temperature_compensation(float voltage, float temperature) {
    // 实现温度补偿算法
}
```

### 2. 电池健康度
```cpp
// 计算电池健康度
float calculate_battery_health() {
    // 基于充放电循环、电压变化等计算
}
```

### 3. 充电时间估算
```cpp
// 估算充电完成时间
int estimate_charging_time() {
    // 基于当前电量、充电电流等计算
}
```

## 总结

本电池管理系统通过硬件分压器检测电池电压，结合软件滤波和映射算法，实现了准确的电池电量监测。系统具有良好的可移植性和扩展性，适用于各种基于ESP32-S3的电池供电设备。

关键成功因素：
1. 准确的硬件分压比计算
2. 合理的ADC配置和采样策略
3. 有效的软件滤波算法
4. 充分的调试和校准
5. 良好的平台适配性
