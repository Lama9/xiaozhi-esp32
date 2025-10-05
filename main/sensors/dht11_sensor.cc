#include "dht11_sensor.h"

#define TAG "DHT11_Sensor"

DHT11Sensor::DHT11Sensor(gpio_num_t data_pin, uint32_t max_retry, uint32_t timeout_ms) 
    : data_pin_(data_pin), max_retry_count_(max_retry), timeout_ms_(timeout_ms), last_retry_count_(0), is_enabled_(false) {
    ESP_LOGI(TAG, "DHT11Sensor created with GPIO %d, enabled: %s", data_pin_, is_enabled_ ? "true" : "false");
}

DHT11Sensor::~DHT11Sensor() {
    ESP_LOGI(TAG, "DHT11Sensor destroyed");
}

bool DHT11Sensor::Init() {
    ESP_LOGI(TAG, "Initializing DHT11 sensor on GPIO %d", data_pin_);
    
    // GPIO不需要特殊初始化，esp-idf-lib会自动配置
    // 这里可以进行连接测试
    if (!TestConnection()) {
        ESP_LOGW(TAG, "DHT11 connection test failed, but continuing...");
    }
    
    ESP_LOGI(TAG, "DHT11 sensor initialized successfully");
    return true;
}

bool DHT11Sensor::TestConnection() {
    ESP_LOGI(TAG, "Testing DHT11 connection on GPIO %d...", data_pin_);
    
    DHT11Data test_data = Read();
    if (test_data.is_valid) {
        ESP_LOGI(TAG, "DHT11 connection test passed - Temp: %.1f°C, Humidity: %.1f%%", 
                 test_data.temperature, test_data.humidity);
        return true;
    }
    
    ESP_LOGW(TAG, "DHT11 connection test failed");
    return false;
}

DHT11Data DHT11Sensor::Read() {
    DHT11Data result = {0.0f, 0.0f, false, (uint64_t)esp_timer_get_time(), 0};
    
    // 检查传感器是否启用
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (!is_enabled_) {
        ESP_LOGW(TAG, "DHT11 sensor is disabled, returning invalid data");
        result.is_valid = false;
        return result;
    }
    
    float temperature = 0.0f;
    float humidity = 0.0f;
    
    // 使用esp-idf-lib的dht_read_float_data函数读取数据
    esp_err_t ret = dht_read_float_data(DHT_TYPE_DHT11, data_pin_, &humidity, &temperature);
    
    if (ret == ESP_OK) {
        result.temperature = temperature;
        result.humidity = humidity;
        result.is_valid = true;
        result.timestamp = (uint64_t)esp_timer_get_time();
        result.retry_count = last_retry_count_;
        
        // 重置重试计数
        last_retry_count_ = 0;
        
        ESP_LOGI(TAG, "DHT11 read success: Temperature=%.1f°C, Humidity=%.1f%%", 
                 result.temperature, result.humidity);
    } else {
        result.timestamp = (uint64_t)esp_timer_get_time();
        result.retry_count = ++last_retry_count_;
        
        ESP_LOGE(TAG, "DHT11 read failed: %s (retry #%d)", 
                 esp_err_to_name(ret), last_retry_count_);
    }
    
    return result;
}

std::string DHT11Sensor::GetStatusJson() {
    DHT11Data data = Read();
    
    char json_str[256];
    
    if (data.is_valid) {
        snprintf(json_str, sizeof(json_str),
            "{\"driver\":\"esp-idf-lib\",\"temperature\":%.1f,\"humidity\":%.1f,\"is_valid\":%s,\"timestamp\":%llu,\"retry_count\":%lu}",
            data.temperature, data.humidity, data.is_valid ? "true" : "false", 
            data.timestamp, (unsigned long)data.retry_count);
    } else {
        snprintf(json_str, sizeof(json_str),
            "{\"driver\":\"esp-idf-lib\",\"temperature\":null,\"humidity\":null,\"is_valid\":false,\"timestamp\":%llu,\"retry_count\":%lu}",
            data.timestamp, (unsigned long)data.retry_count);
    }
    
    return std::string(json_str);
}

void DHT11Sensor::SetRetryCount(uint32_t max_retries) {
    max_retry_count_ = max_retries;
    last_retry_count_ = 0;  // 重置重试计数
    ESP_LOGI(TAG, "DHT11 retry count configured to %d", max_retries);
}

bool DHT11Sensor::Enable() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (is_enabled_) {
        ESP_LOGW(TAG, "DHT11 sensor is already enabled");
        return true;
    }
    
    // 测试传感器连接
    if (!TestConnection()) {
        ESP_LOGE(TAG, "Failed to enable DHT11 sensor: connection test failed");
        return false;
    }
    
    is_enabled_ = true;
    ESP_LOGI(TAG, "DHT11 sensor enabled successfully");
    return true;
}

bool DHT11Sensor::Disable() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (!is_enabled_) {
        ESP_LOGW(TAG, "DHT11 sensor is already disabled");
        return true;
    }
    
    is_enabled_ = false;
    ESP_LOGI(TAG, "DHT11 sensor disabled successfully");
    return true;
}

bool DHT11Sensor::IsEnabled() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return is_enabled_;
}

bool DHT11Sensor::SetEnabled(bool enabled) {
    if (enabled) {
        return Enable();
    } else {
        return Disable();
    }
}
