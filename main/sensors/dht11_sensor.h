#ifndef DHT11_SENSOR_H
#define DHT11_SENSOR_H

#include <driver/gpio.h>
#include <dht.h>
#include <string>
#include <esp_timer.h>
#include <esp_log.h>
#include <mutex>

/**
 * @brief DHT11温湿度传感器数据结构
 */
struct DHT11Data {
    float temperature;    // 温度 (°C)
    float humidity;       // 湿度 (%)
    bool is_valid;       // 数据有效性
    uint64_t timestamp;  // 时间戳(微秒)
    uint32_t retry_count; // 重试次数
};

/**
 * @brief DHT11温湿度传感器类
 * 使用ESP-IDF-Lib中的DHT组件实现
 */
class DHT11Sensor {
private:
    gpio_num_t data_pin_;           // DHT11数据引脚
    uint32_t max_retry_count_;      // 最大重试次数
    uint32_t timeout_ms_;           // 超时时间(毫秒)
    uint32_t last_retry_count_;     // 上次重试计数
    bool is_enabled_;               // 传感器开关状态(默认关闭)
    std::mutex state_mutex_;        // 状态保护锁
    
    /**
     * @brief 测试DHT11连接
     * @return true 连接正常，false 连接异常
     */
    bool TestConnection();
    
public:
    /**
     * @brief 构造函数
     * @param data_pin DHT11数据引脚
     * @param max_retry 最大重试次数(默认3次)
     * @param timeout_ms 超时时间(默认2000毫秒)
     */
    DHT11Sensor(gpio_num_t data_pin, uint32_t max_retry = 3, uint32_t timeout_ms = 2000);
    
    /**
     * @brief 析构函数
     */
    ~DHT11Sensor();
    
    /**
     * @brief 初始化DHT11传感器
     * @return true 初始化成功，false 初始化失败
     */
    bool Init();
    
    /**
     * @brief 读取DHT11温湿度数据
     * @return DHT11Data 包含温度、湿度等信息的结构体
     */
    DHT11Data Read();
    
    /**
     * @brief 获取传感器状态JSON字符串
     * @return std::string JSON格式的状态信息
     */
    std::string GetStatusJson();
    
    /**
     * @brief 设置重试次数
     * @param max_retries 最大重试次数
     */
    void SetRetryCount(uint32_t max_retries);
    
    /**
     * @brief 获取最近一次重试次数
     * @return uint32_t 重试次数
     */
    uint32_t GetLastRetryCount() const { return last_retry_count_; }
    
    /**
     * @brief 启用DHT11传感器
     * @return true 启用成功，false 启用失败
     */
    bool Enable();
    
    /**
     * @brief 禁用DHT11传感器
     * @return true 禁用成功，false 禁用失败
     */
    bool Disable();
    
    /**
     * @brief 检查传感器是否启用
     * @return true 已启用，false 已禁用
     */
    bool IsEnabled();
    
    /**
     * @brief 设置传感器开关状态
     * @param enabled true启用，false禁用
     * @return true 设置成功，false 设置失败
     */
    bool SetEnabled(bool enabled);
};

#endif // DHT11_SENSOR_H
