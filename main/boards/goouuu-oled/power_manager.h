/**
 * @file power_manager.h
 * @brief 基于ESP32-S3的电池管理系统 (goouuu-oled/lcd版本)
 * 
 * 本文件实现了完整的电池管理系统，包括：
 * - 电池电压检测 (通过ADC读取分压器电压)
 * - 充电状态检测 (通过GPIO读取TP4054状态)
 * - 电池电量计算 (基于ADC值映射到电量百分比)
 * - 低电量检测和回调
 * 
 * 硬件要求：
 * - 电池电压检测：GPIO11 → ADC2_CH0 (通过4.7K+1K分压器)
 * - 充电状态检测：GPIO12 (TP4054的CHGR引脚)
 * - 分压比：0.175 (1K/(4.7K+1K))
 * 
 * 技术要点：
 * - 使用滑动平均滤波提高ADC读取稳定性
 * - 基于实际硬件校准的电池电量映射表
 * - 定时器驱动的周期性检测机制
 * - 详细的调试信息输出用于校准
 */

#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>

/**
 * @class PowerManager
 * @brief 电池管理系统核心类 (goouuu-oled/lcd版本)
 * 
 * 功能特性：
 * - 实时电池电压监测
 * - 充电状态检测
 * - 电池电量百分比计算
 * - 低电量警告
 * - 状态变化回调通知
 */
class PowerManager {
private:
    // === 定时器和回调相关 ===
    esp_timer_handle_t timer_handle_;                                    ///< 电池检测定时器句柄
    std::function<void(bool)> on_charging_status_changed_;              ///< 充电状态变化回调
    std::function<void(bool)> on_low_battery_status_changed_;            ///< 低电量状态变化回调

    // === 硬件引脚配置 ===
    /// @brief 充电状态检测引脚 (TP4054的CHGR引脚)
    /// 注意：通过构造函数设置，默认为GPIO_NUM_NC
    /// 充电时：GPIO12 = 0 (低电平)
    /// 未充电/充满：GPIO12 = 1 (高电平)
    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    
    // === ADC相关配置 ===
    adc_oneshot_unit_handle_t adc_handle_;                               ///< ADC句柄
    adc_unit_t power_adc_unit_ = ADC_UNIT_2;                             ///< 电量检测ADC单元,默认值为ADC_UNIT_2
    adc_channel_t power_adc_channel_ = ADC_CHANNEL_0;                    ///< 电量检测ADC通道,默认值为ADC_CHANNEL_0 (GPIO11)
    std::vector<uint16_t> adc_values_;                                   ///< ADC值滑动窗口缓存
    
    // === 电池状态数据 ===
    uint32_t battery_level_ = 0;                                        ///< 电池电量百分比 (0-100)
    bool is_charging_ = false;                                           ///< 当前充电状态
    bool is_low_battery_ = false;                                       ///< 低电量状态标志
    
    // === 检测控制参数 ===
    int ticks_ = 0;                                                     ///< 检测计数器
    const int kBatteryAdcInterval = 60;                                 ///< ADC检测间隔 (秒)
    const int kBatteryAdcDataCount = 3;                                 ///< ADC滑动窗口大小
    const int kLowBatteryLevel = 20;                                    ///< 低电量阈值 (%)
    
    // === 调试相关 ===
    int debug_counter_ = 0;                                             ///< 调试计数器
    const int kDebugInterval = 10;                                     ///< 详细调试信息输出间隔

    /**
     * @brief 检查电池状态 (定时器回调函数)
     * 
     * 功能：
     * 1. 检测充电状态变化
     * 2. 控制ADC采样频率
     * 3. 触发状态变化回调
     * 
     * 调用频率：每1秒一次
     */
    void CheckBatteryStatus() {
        // === 充电状态检测 ===
        // TP4054充电状态逻辑：充电时CHGR=0，未充电/充满时CHGR=1
        bool new_charging_status = gpio_get_level(charging_pin_) == 0;
        
        // 充电状态发生变化时立即处理
        if (new_charging_status != is_charging_) {
            is_charging_ = new_charging_status;
            if (on_charging_status_changed_) {
                on_charging_status_changed_(is_charging_);
            }
            ReadBatteryAdcData();  // 状态变化时立即读取ADC
            return;
        }

        // === ADC采样控制 ===
        // 如果电池电量数据不足，则读取电池电量数据
        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据充足，则每 kBatteryAdcInterval 个 tick 读取一次电池电量数据
        ticks_++;
        if (ticks_ % kBatteryAdcInterval == 0) {
            ReadBatteryAdcData();
        }
    }

    /**
     * @brief 读取电池ADC数据并计算电量
     * 
     * 功能：
     * 1. 读取ADC原始值
     * 2. 滑动平均滤波
     * 3. 根据分压比计算实际电池电压
     * 4. 映射到电量百分比
     * 5. 检测低电量状态
     * 6. 输出调试信息
     */
    void ReadBatteryAdcData() {
        int adc_value;
        // 读取ADC原始值 - GPIO11对应ADC2_CH0
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, power_adc_channel_, &adc_value));
        
        // === 滑动平均滤波 ===
        // 将 ADC 值添加到队列中
        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();
        
        // === 电池电量映射表 (基于4.7K+1K分压器，分压比0.175) ===
        // 电池电压范围：3.0V-4.2V
        // 分压后电压范围：0.525V-0.735V
        // ADC值范围：约650-910 (基于3.3V参考电压)
        // === 电池电量映射表 (基于最新实测数据校准) ===
        // 最新实测数据：ADC值826-831对应电池电压4.174V (满电状态)
        // 根据实测分压比0.1595重新计算映射表
        // === 电池电量映射表 (基于2026-01-12实测数据校准) ===
        // 实测数据：ADC值826对应电池电压4.083V，分压比0.163
        // 计算公式：ADC值 = 电池电压 * 分压比 * 4095 / 3.3
        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            {600, 0},   // 3.0V电池电压 → 0.489V ADC电压 → 600 ADC值
            {655, 20},  // 3.3V电池电压 → 0.538V ADC电压 → 655 ADC值
            {715, 40},  // 3.6V电池电压 → 0.587V ADC电压 → 715 ADC值
            {755, 60},  // 3.8V电池电压 → 0.619V ADC电压 → 755 ADC值
            {795, 80},  // 4.0V电池电压 → 0.652V ADC电压 → 795 ADC值
            {835, 100}  // 4.2V电池电压 → 0.685V ADC电压 → 835 ADC值
        };

        // === 电量百分比计算 ===
        // 低于最低值时
        if (average_adc < levels[0].adc) {
            battery_level_ = 0;
        }
        // 高于最高值时
        else if (average_adc >= levels[5].adc) {
            battery_level_ = 100;
        } else {
            // 线性插值计算中间值
            for (int i = 0; i < 5; i++) {
                if (average_adc >= levels[i].adc && average_adc < levels[i+1].adc) {
                    float ratio = static_cast<float>(average_adc - levels[i].adc) / (levels[i+1].adc - levels[i].adc);
                    battery_level_ = levels[i].level + ratio * (levels[i+1].level - levels[i].level);
                    break;
                }
            }
        }

        // === 低电量检测 ===
        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
            if (new_low_battery_status != is_low_battery_) {
                is_low_battery_ = new_low_battery_status;
                if (on_low_battery_status_changed_) {
                    on_low_battery_status_changed_(is_low_battery_);
                }
            }
        }

        // === 调试信息输出 ===
        // 增加调试计数器
        debug_counter_++;
        
        // 计算实际电池电压（根据分压器计算）
        // 2026-01-12实测分压比 = 0.163 (根据实测数据：ADC电压0.665V / 电池电压4.083V)
        // 理论分压比 = 1K / (4.7K + 1K) = 0.175
        // 电压转换公式：
        // ADC电压 = ADC值 * 3.3V / 4095
        // 电池电压 = ADC电压 / 实际分压比
        float adc_voltage = (float)average_adc * 3.3f / 4095.0f;
        float battery_voltage = adc_voltage / 0.163f; // 2026-01-12实测分压比校准
        
        // 每kDebugInterval次输出详细调试信息
        if (debug_counter_ % kDebugInterval == 0) {
            ESP_LOGI("PowerManager", "=== 电池状态调试信息 ===");
            ESP_LOGI("PowerManager", "ADC原始值: %d", adc_value);
            ESP_LOGI("PowerManager", "ADC平均值: %lu", average_adc);
            ESP_LOGI("PowerManager", "ADC电压: %.3fV", adc_voltage);
            ESP_LOGI("PowerManager", "电池电压: %.3fV", battery_voltage);
            ESP_LOGI("PowerManager", "电池电量: %lu%%", battery_level_);
            ESP_LOGI("PowerManager", "GPIO12状态: %d", gpio_get_level(charging_pin_));
            ESP_LOGI("PowerManager", "充电状态: %s", is_charging_ ? "充电中" : "未充电");
            ESP_LOGI("PowerManager", "低电量: %s", is_low_battery_ ? "是" : "否");
        } else {
            // 普通日志输出
            ESP_LOGI("PowerManager", "ADC: %d, 平均: %lu, 电量: %lu%%, 电压: %.3fV, GPIO12: %d", 
                adc_value, average_adc, battery_level_, battery_voltage, gpio_get_level(charging_pin_));
        }
    }

public:

    /**
     * @brief 电源电池管理模块构造函数 (goouuu-oled/lcd版本)
     * 
     * @param pin 充电状态检测引脚 (GPIO12)
     * 
     * 硬件配置：
     * - 充电检测：GPIO12 (TP4054的CHGR引脚)
     * - 电池电压检测：GPIO11 → ADC2_CH0
     * - 分压器：4.7K+1K (分压比0.175)
     * 
     * 初始化流程：
     * 1. 配置充电状态检测GPIO
     * 2. 创建定时器用于周期性检测
     * 3. 初始化ADC2单元和通道
     */
    PowerManager(gpio_num_t pin) : charging_pin_(pin) {
        // === 初始化充电状态检测引脚 ===
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << charging_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     
        gpio_config(&io_conf);

        // === 创建电池检测定时器 ===
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                PowerManager* self = static_cast<PowerManager*>(arg);
                self->CheckBatteryStatus();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000)); // 1秒间隔

        // === 初始化ADC2单元，用于电池电压检测 ===
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = power_adc_unit_,                  // 使用ADC2单元
            .ulp_mode = ADC_ULP_MODE_DISABLE,       // 禁用ULP模式
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        // === 配置ADC通道参数 ===
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,               // 12dB衰减，支持0-3.3V输入
            .bitwidth = ADC_BITWIDTH_12,             // 12位精度
        };
        // 配置ADC2_CH0通道，对应GPIO11
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, power_adc_channel_, &chan_config));
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    bool IsCharging() {
        // 直接返回GPIO状态，不做额外判断
        // TP4054：CHGR=0表示充电中，CHGR=1表示未充电/充满
        return is_charging_;
    }

    bool IsDischarging() {
        // 没有区分充电和放电，所以直接返回相反状态
        return !is_charging_;
    }

    uint8_t GetBatteryLevel() {
        return battery_level_;
    }

    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }

    /**
     * @brief 获取电池电压 (用于调试和校准)
     * 
     * @return float 电池电压值 (V)
     * 
     * 注意：此方法用于调试和校准，实际应用中建议使用GetBatteryLevel()
     */
    float GetBatteryVoltage() {
        if (adc_values_.empty()) {
            return 0.0f;
        }
        
        // 计算平均ADC值
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();
        
        // 计算ADC电压
        float adc_voltage = (float)average_adc * 3.3f / 4095.0f;
        
        // 根据2026-01-12实测分压比计算实际电池电压
        // 实测分压比 = 0.163
        return adc_voltage / 0.163f;
    }

    /**
     * @brief 获取原始ADC值 (用于调试和校准)
     * 
     * @return uint32_t 原始ADC值
     * 
     * 注意：此方法用于调试和校准，实际应用中建议使用GetBatteryLevel()
     */
    uint32_t GetRawAdcValue() {
        if (adc_values_.empty()) {
            return 0;
        }
        
        // 返回最新的ADC值
        return adc_values_.back();
    }
};
