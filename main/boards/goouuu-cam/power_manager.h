/**
 * @file power_manager.h
 * @brief 基于ESP32-S3的电池管理系统
 * 
 * 本文件实现了完整的电池管理系统，包括：
 * - 电池电压检测 (通过ADC读取分压器电压)
 * - 充电状态检测 (通过GPIO读取TP4054状态)
 * - 电池电量计算 (基于ADC值映射到电量百分比)
 * - 低电量检测和回调
 * 
 * 硬件要求：
 * - 电池电压检测：GPIO20 → ADC2_CH9 (通过1.5K+4.7K分压器)
 * - 充电状态检测：GPIO19 (TP4054的CHGR引脚)
 * - 分压比：0.758 (4.7K/(1.5K+4.7K))
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
#include <driver/adc.h>

/**
 * @class PowerManager
 * @brief 电池管理系统核心类
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
    /// 充电时：GPIO19 = 0 (低电平)
    /// 未充电/充满：GPIO19 = 1 (高电平)
    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    
    // === ADC相关配置 ===
    adc_oneshot_unit_handle_t adc_handle_;                               ///< ADC句柄
    adc_unit_t power_adc_unit_ = ADC_UNIT_2;                             ///< 电量检测ADC单元,默认值为ADC_UNIT_2
    adc_channel_t power_adc_channel_ = ADC_CHANNEL_9;                    ///< 电量检测ADC通道,默认值为ADC_CHANNEL_9
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
        // 如果电池电量数据不足，则立即读取
        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据充足，则按间隔读取
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
     * 3. 计算电池电压
     * 4. 映射到电量百分比
     * 5. 检测低电量状态
     * 6. 输出调试信息
     * 
     * 硬件配置：
     * - GPIO20 → ADC2_CH9
     * - 分压器：1.5K + 4.7K (分压比0.758)
     * - ADC参考电压：3.3V
     * - ADC精度：12位 (0-4095)
     */
    void ReadBatteryAdcData() {
        int adc_value;
        // 读取ADC原始值 - GPIO20对应ADC2_CH9
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, power_adc_channel_, &adc_value));
        
        // === 滑动平均滤波 ===
        // 维护固定大小的滑动窗口，提高ADC读取稳定性
        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        
        // 计算滑动平均值
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();

        // === 电池电量映射表 ===
        // 基于实际硬件校准的ADC值到电量百分比映射
        // 计算依据：
        // - 分压比：0.758 (4.7K/(1.5K+4.7K))
        // - ADC参考电压：3.3V
        // - 电池电压范围：3.0V-4.2V
        // - ADC值计算：ADC = (V_bat * 0.758 / 3.3) * 4095
        const struct {
            uint16_t adc;      ///< ADC原始值
            uint8_t level;     ///< 电量百分比
        } levels[] = {
            {2815, 0},   // 3.0V电池电压 → 2.274V ADC电压 → 2815 ADC值
            {3000, 20},  // 3.3V电池电压 → 2.501V ADC电压 → 3000 ADC值
            {3200, 40},  // 3.6V电池电压 → 2.729V ADC电压 → 3200 ADC值
            {3400, 60},  // 3.8V电池电压 → 2.880V ADC电压 → 3400 ADC值
            {3600, 80},  // 4.0V电池电压 → 3.032V ADC电压 → 3600 ADC值
            {3940, 100}  // 4.2V电池电压 → 3.184V ADC电压 → 3940 ADC值
        };

        // === 电池电量计算 ===
        // 使用分段线性插值算法计算电量百分比
        if (average_adc < levels[0].adc) {
            // 低于最低阈值，设为0%
            battery_level_ = 0;
        }
        else if (average_adc >= levels[5].adc) {
            // 高于最高阈值，设为100%
            battery_level_ = 100;
        } else {
            // 线性插值计算中间值
            for (int i = 0; i < 5; i++) {
                if (average_adc >= levels[i].adc && average_adc < levels[i+1].adc) {
                    // 计算插值比例
                    float ratio = static_cast<float>(average_adc - levels[i].adc) / (levels[i+1].adc - levels[i].adc);
                    // 线性插值计算电量百分比
                    battery_level_ = levels[i].level + ratio * (levels[i+1].level - levels[i].level);
                    break;
                }
            }
        }

        // === 低电量检测 ===
        // 只有在ADC数据充足时才进行低电量检测，避免误报
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
        // 分压比 = 4.7K / (1.5K + 4.7K) = 0.758
        // ADC读取的是分压后的电压，需要反推电池电压
        // 计算实际电池电压（用于调试和校准）
        // 电压转换公式：
        // 1. ADC值 → ADC电压：V_adc = ADC_value * 3.3V / 4095
        // 2. ADC电压 → 电池电压：V_bat = V_adc / 0.758 (考虑分压器)
        float adc_voltage = (float)average_adc * 3.3f / 4095.0f;  // ADC值转换为电压
        float battery_voltage = adc_voltage / 0.758f;  // 反推电池电压
        
        // 详细调试信息输出 (每10次检查输出一次)
        if (debug_counter_ % kDebugInterval == 0) {
            ESP_LOGI("PowerManager", "=== 电池状态调试信息 ===");
            ESP_LOGI("PowerManager", "原始ADC值: %d", adc_value);
            ESP_LOGI("PowerManager", "平均ADC值: %ld", average_adc);
            ESP_LOGI("PowerManager", "ADC电压: %.3fV", adc_voltage);
            ESP_LOGI("PowerManager", "计算电池电压: %.3fV", battery_voltage);
            ESP_LOGI("PowerManager", "电池电量: %ld%%", battery_level_);
            ESP_LOGI("PowerManager", "充电状态: %s", is_charging_ ? "充电中" : "未充电");
            ESP_LOGI("PowerManager", "低电量状态: %s", is_low_battery_ ? "是" : "否");
            ESP_LOGI("PowerManager", "GPIO19电平: %d", gpio_get_level(charging_pin_));
            ESP_LOGI("PowerManager", "========================");
        } else {
            // 普通日志输出 (每次检查都输出)
            ESP_LOGI("PowerManager", "ADC: %d, 平均: %ld, 电压: %.3fV, 电量: %ld%%, 充电: %s", 
                     adc_value, average_adc, battery_voltage, battery_level_, 
                     is_charging_ ? "是" : "否");
        }
    }

public:

    /**
     * @brief 电源电池管理模块构造函数
     * 
     * @param pin 充电状态检测引脚 (通常为GPIO19，连接TP4054的CHGR引脚)
     * 
     * 功能：
     * 1. 初始化充电状态检测GPIO
     * 2. 创建电池检测定时器
     * 3. 初始化ADC用于电池电压检测
     * 
     * 硬件要求：
     * - 充电状态引脚：连接TP4054的CHGR引脚
     * - 电池电压检测：GPIO20连接分压器输出
     * 
     * 注意事项：
     * - 构造函数会立即启动定时器，开始电池检测
     * - ADC配置为ADC2_CH9，对应GPIO20
     * - 定时器间隔为1秒，ADC采样间隔为60秒
     */
    PowerManager(gpio_num_t pin) : charging_pin_(pin) {
        // === GPIO配置 ===
        // 配置充电状态检测引脚为输入模式
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;        // 禁用中断
        io_conf.mode = GPIO_MODE_INPUT;               // 输入模式
        io_conf.pin_bit_mask = (1ULL << charging_pin_);  // 设置引脚位掩码
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // 禁用下拉
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     // 禁用上拉
        gpio_config(&io_conf);

        // === 定时器配置 ===
        // 创建电池检测定时器，每1秒执行一次CheckBatteryStatus
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                PowerManager* self = static_cast<PowerManager*>(arg);
                self->CheckBatteryStatus();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,        // 在任务中执行回调
            .name = "battery_check_timer",
            .skip_unhandled_events = true,           // 跳过未处理的事件
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));  // 1秒间隔

        // === ADC配置 ===
        // 初始化ADC2单元，用于电池电压检测
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = power_adc_unit_,                  // 使用ADC2单元
            .ulp_mode = ADC_ULP_MODE_DISABLE,       // 禁用ULP模式
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        // 配置ADC通道参数
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,               // 12dB衰减，支持0-3.3V输入
            .bitwidth = ADC_BITWIDTH_12,            // 12位精度 (0-4095)
        };
        // 配置ADC2_CH9通道，对应GPIO20
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

    /**
     * @brief 获取充电状态
     * 
     * @return true: 正在充电, false: 未充电/充满
     * 
     * 逻辑说明：
     * - 基于TP4054的CHGR引脚状态判断
     * - 充电时：CHGR = 0 (低电平)
     * - 未充电/充满：CHGR = 1 (高电平)
     * - 电量100%时强制返回false，避免显示"充电中"
     */
    bool IsCharging() {
        // 如果电量已经满了，则不再显示充电中
        if (battery_level_ == 100) {
            return false;
        }
        return is_charging_;
    }

    /**
     * @brief 获取放电状态
     * 
     * @return true: 正在放电, false: 充电中
     * 
     * 注意：这是充电状态的反向逻辑
     */
    bool IsDischarging() {
        // 没有区分充电和放电，所以直接返回相反状态
        return !is_charging_;
    }

    /**
     * @brief 获取电池电量百分比
     * 
     * @return 电池电量百分比 (0-100)
     * 
     * 计算方式：
     * - 基于ADC值通过映射表计算
     * - 使用滑动平均滤波提高稳定性
     * - 分段线性插值算法
     */
    uint8_t GetBatteryLevel() {
        return battery_level_;
    }

    /**
     * @brief 设置低电量状态变化回调
     * 
     * @param callback 回调函数，参数为bool类型
     *                 true: 进入低电量状态
     *                 false: 退出低电量状态
     * 
     * 使用示例：
     * ```cpp
     * power_manager->OnLowBatteryStatusChanged([](bool is_low) {
     *     if (is_low) {
     *         ESP_LOGI("App", "电池电量低，请充电！");
     *     }
     * });
     * ```
     */
    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }

    /**
     * @brief 设置充电状态变化回调
     * 
     * @param callback 回调函数，参数为bool类型
     *                 true: 开始充电
     *                 false: 停止充电
     * 
     * 使用示例：
     * ```cpp
     * power_manager->OnChargingStatusChanged([](bool is_charging) {
     *     if (is_charging) {
     *         ESP_LOGI("App", "开始充电");
     *     } else {
     *         ESP_LOGI("App", "充电完成或停止充电");
     *     }
     * });
     * ```
     */
    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }
    
    /**
     * @brief 获取当前电池电压（用于调试和校准）
     * 
     * @return 电池电压（伏特）
     * 
     * 功能：
     * - 计算当前电池的实际电压
     * - 用于调试和校准验证
     * - 基于滑动平均的ADC值计算
     * 
     * 计算公式：
     * 1. ADC值 → ADC电压：V_adc = ADC_value * 3.3V / 4095
     * 2. ADC电压 → 电池电压：V_bat = V_adc / 0.758
     * 
     * 使用场景：
     * - 校准验证：对比万用表测量值
     * - 调试分析：观察电压变化趋势
     * - 故障诊断：检查ADC读取是否正常
     */
    float GetBatteryVoltage() {
        if (adc_values_.empty()) {
            return 0.0f;
        }
        
        // 计算滑动平均ADC值
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();
        
        // 计算实际电池电压
        float adc_voltage = (float)average_adc * 3.3f / 4095.0f;
        float battery_voltage = adc_voltage / 0.758f;  // 分压比0.758
        
        return battery_voltage;
    }
    
    /**
     * @brief 获取原始ADC值（用于调试和校准）
     * 
     * @return 平均ADC值 (0-4095)
     * 
     * 功能：
     * - 获取当前滑动平均的ADC原始值
     * - 用于调试和校准分析
     * - 帮助理解ADC读取的稳定性
     * 
     * 使用场景：
     * - 校准映射表：记录不同电压下的ADC值
     * - 调试分析：观察ADC值变化范围
     * - 故障诊断：检查ADC读取是否在正常范围
     * 
     * 正常范围：
     * - 电池3.0V时：ADC值约2815
     * - 电池4.2V时：ADC值约3940
     */
    uint32_t GetRawAdcValue() {
        if (adc_values_.empty()) {
            return 0;
        }
        
        // 计算滑动平均ADC值
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();
        
        return average_adc;
    }
};
