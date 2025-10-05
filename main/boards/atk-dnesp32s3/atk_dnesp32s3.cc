#include "wifi_board.h"
#include "codecs/es8388_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "led/single_led.h"
#include "esp32_camera.h"
#include "sensors/dht11_sensor.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#define TAG "atk_dnesp32s3"

class XL9555 : public I2cDevice {
public:
    XL9555(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x06, 0x03);
        WriteReg(0x07, 0xF0);
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint16_t data;
        int index = bit;

        if (bit < 8) {
            data = ReadReg(0x02);
        } else {
            data = ReadReg(0x03);
            index -= 8;
        }

        data = (data & ~(1 << index)) | (level << index);

        if (bit < 8) {
            WriteReg(0x02, data);
        } else {
            WriteReg(0x03, data);
        }
    }
};

class atk_dnesp32s3 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;
    XL9555* xl9555_;
    Esp32Camera* camera_;
    DHT11Sensor* dht11_sensor_;
    DHT11Data last_dht11_data_;
    uint64_t last_dht11_read_time_;
    esp_timer_handle_t dht11_timer_;
    bool dht11_timer_running_;        // DHT11定时器运行状态

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // Initialize XL9555
        xl9555_ = new XL9555(i2c_bus_, 0x20);
    }

    // Initialize spi peripheral
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = LCD_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = LCD_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        ESP_LOGD(TAG, "Install panel IO");
        // 液晶屏控制IO初始化
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = LCD_CS_PIN;
        io_config.dc_gpio_num = LCD_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 7;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io);

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel);
        
        esp_lcd_panel_reset(panel);
        xl9555_->SetOutputState(8, 1);
        xl9555_->SetOutputState(2, 0);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY); 
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // 初始化摄像头：ov2640；
    // 根据正点原子官方示例参数
    void InitializeCamera() {
        
        xl9555_->SetOutputState(OV_PWDN_IO, 0); // PWDN=低 (上电)
        xl9555_->SetOutputState(OV_RESET_IO, 0); // 确保复位
        vTaskDelay(pdMS_TO_TICKS(50));           // 延长复位保持时间
        xl9555_->SetOutputState(OV_RESET_IO, 1); // 释放复位
        vTaskDelay(pdMS_TO_TICKS(50));           // 延长 50ms

        camera_config_t config = {};

        config.pin_pwdn = CAM_PIN_PWDN;  // 实际由 XL9555 控制
        config.pin_reset = CAM_PIN_RESET;// 实际由 XL9555 控制
        config.pin_xclk = CAM_PIN_XCLK;
        config.pin_sccb_sda = CAM_PIN_SIOD;
        config.pin_sccb_scl = CAM_PIN_SIOC;

        config.pin_d7 = CAM_PIN_D7;
        config.pin_d6 = CAM_PIN_D6;
        config.pin_d5 = CAM_PIN_D5;
        config.pin_d4 = CAM_PIN_D4;
        config.pin_d3 = CAM_PIN_D3;
        config.pin_d2 = CAM_PIN_D2;
        config.pin_d1 = CAM_PIN_D1;
        config.pin_d0 = CAM_PIN_D0;
        config.pin_vsync = CAM_PIN_VSYNC;
        config.pin_href = CAM_PIN_HREF;
        config.pin_pclk = CAM_PIN_PCLK;

        /* XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental) */
        config.xclk_freq_hz = 24000000;
        config.ledc_timer = LEDC_TIMER_0;
        config.ledc_channel = LEDC_CHANNEL_0;

        config.pixel_format = PIXFORMAT_RGB565;   /* YUV422,GRAYSCALE,RGB565,JPEG */
        config.frame_size = FRAMESIZE_QVGA;       /* QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates */

        config.jpeg_quality = 12;                 /* 0-63, for OV series camera sensors, lower number means higher quality */
        config.fb_count = 2;                      /* When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode */
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        esp_err_t err = esp_camera_init(&config); // 测试相机是否存在
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Camera is not plugged in or not supported, error: %s", esp_err_to_name(err));
            // 如果摄像头初始化失败，设置 camera_ 为 nullptr
            camera_ = nullptr;
            return;
        }else
        {
            esp_camera_deinit();// 释放之前的摄像头资源,为正确初始化做准备
            camera_ = new Esp32Camera(config);
        }
        
    }

    /**
     * @brief 初始化DHT11温湿度传感器
     */
    void InitializeDHT11() {
        ESP_LOGI(TAG, "Initializing DHT11 sensor on GPIO %d", DHT11_DATA_PIN);
        
        dht11_sensor_ = new DHT11Sensor(DHT11_DATA_PIN, DHT11_MAX_RETRY_COUNT, DHT11_TIMEOUT_MS);
        
        if (!dht11_sensor_->Init()) {
            ESP_LOGE(TAG, "Failed to initialize DHT11 sensor");
            delete dht11_sensor_;
            dht11_sensor_ = nullptr;
            return;
        }
        
        // 执行一次初始读取测试
        last_dht11_data_ = dht11_sensor_->Read();
        last_dht11_read_time_ = (uint64_t)esp_timer_get_time();
        
        if (last_dht11_data_.is_valid) {
            ESP_LOGI(TAG, "DHT11 sensor test successful - Temp: %.1f°C, Humidity: %.1f%%", 
                     last_dht11_data_.temperature, last_dht11_data_.humidity);
        } else {
            ESP_LOGW(TAG, "DHT11 sensor test failed, but continuing");
        }
    }
    
    /**
     * @brief 读取DHT11传感器数据
     * @note 只有在传感器启用时才会读取数据
     */
    void ReadDHT11Data() {
        if (!dht11_sensor_) {
            ESP_LOGW(TAG, "DHT11 sensor not initialized");
            return;
        }
        
        // 检查传感器是否启用
        if (!dht11_sensor_->IsEnabled()) {
            ESP_LOGD(TAG, "DHT11 sensor is disabled, skipping read");
            return;
        }
        
        uint64_t current_time = (uint64_t)esp_timer_get_time();
        if (current_time - last_dht11_read_time_ < DHT11_READ_INTERVAL_MS * 1000) {
            return; // 未到读取时间
        }
        
        DHT11Data new_data = dht11_sensor_->Read();
        if (new_data.is_valid) {
            last_dht11_data_ = new_data;
            last_dht11_read_time_ = current_time;
            ESP_LOGI(TAG, "DHT11 read: Temperature=%.1f°C, Humidity=%.1f%%", 
                     new_data.temperature, new_data.humidity);
        } else {
            ESP_LOGW(TAG, "DHT11 read failed, retry count: %d", new_data.retry_count);
        }
    }
    
    /**
     * @brief 初始化MCP工具
     */
    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        
        // DHT11温度查询工具
        mcp_server.AddTool("self.atk_dnesp32s3.get_temperature", 
            "获取当前环境温度（基于ESP-IDF-Lib DHT驱动）", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                ReadDHT11Data(); // 更新最新数据
                if (last_dht11_data_.is_valid) {
                    return std::to_string(last_dht11_data_.temperature);
                } else {
                    return std::string("-999.0"); // 表示读取失败
                }
            });
            
        // DHT11湿度查询工具
        mcp_server.AddTool("self.atk_dnesp32s3.get_humidity", 
            "获取当前环境湿度（基于ESP-IDF-Lib DHT驱动）", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                ReadDHT11Data(); // 更新最新数据
                if (last_dht11_data_.is_valid) {
                    return std::to_string(last_dht11_data_.humidity);
                } else {
                    return std::string("-1.0"); // 表示读取失败
                }
            });
            
        // DHT11完整气象数据工具
        mcp_server.AddTool("self.atk_dnesp32s3.get_weather_data", 
            "获取当前环境温湿度数据（基于ESP-IDF-Lib DHT驱动）", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                ReadDHT11Data(); // 更新最新数据
                if (dht11_sensor_) {
                    return dht11_sensor_->GetStatusJson();
                } else {
                    return std::string("{\"driver\":\"esp-idf-lib\",\"error\":\"DHT11 sensor not initialized\"}");
                }
            });
            
        // DHT11传感器开关控制工具
        mcp_server.AddTool("self.atk_dnesp32s3.enable_dht11", 
            "启用DHT11温湿度传感器监测功能", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                if (!dht11_sensor_) {
                    return std::string("DHT11传感器未初始化");
                }
                
                if (dht11_sensor_->Enable()) {
                    StartDHT11Timer();
                    return std::string("DHT11温湿度监测功能已启用");
                } else {
                    return std::string("DHT11传感器启用失败，请检查硬件连接");
                }
            });
            
        // DHT11传感器关闭工具
        mcp_server.AddTool("self.atk_dnesp32s3.disable_dht11", 
            "禁用DHT11温湿度传感器监测功能", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                if (!dht11_sensor_) {
                    return std::string("DHT11传感器未初始化");
                }
                
                if (dht11_sensor_->Disable()) {
                    StopDHT11Timer();
                    return std::string("DHT11温湿度监测功能已禁用");
                } else {
                    return std::string("DHT11传感器禁用失败");
                }
            });
            
        // DHT11传感器状态查询工具
        mcp_server.AddTool("self.atk_dnesp32s3.get_dht11_status", 
            "查询DHT11温湿度传感器当前状态", 
            PropertyList(), 
            [this](const PropertyList& properties) -> ReturnValue {
                if (!dht11_sensor_) {
                    return std::string("{\"enabled\":false,\"timer_running\":false,\"error\":\"DHT11传感器未初始化\"}");
                }
                
                char status_json[256];
                snprintf(status_json, sizeof(status_json),
                    "{\"enabled\":%s,\"timer_running\":%s,\"last_read_time\":%llu}",
                    dht11_sensor_->IsEnabled() ? "true" : "false",
                    dht11_timer_running_ ? "true" : "false",
                    last_dht11_read_time_);
                return std::string(status_json);
            });
    }

public:
    atk_dnesp32s3() : boot_button_(BOOT_BUTTON_GPIO), dht11_timer_running_(false) {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeDHT11();
        InitializeButtons();
        InitializeTools();
        InitializeCamera();
        // 注意：DHT11定时器默认不启动，需要用户语音开启
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8388AudioCodec audio_codec(
            i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC, 
            AUDIO_CODEC_ES8388_ADDR
        );
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Camera* GetCamera() override {
        return camera_;
    }
    
    /**
     * @brief 启动DHT11定期读取定时器
     * @note 只有在传感器启用且定时器未运行时才启动
     */
    void StartDHT11Timer() {
        if (dht11_timer_running_) {
            ESP_LOGW(TAG, "DHT11 timer is already running");
            return;
        }
        
        if (!dht11_sensor_ || !dht11_sensor_->IsEnabled()) {
            ESP_LOGW(TAG, "Cannot start DHT11 timer: sensor not enabled");
            return;
        }
        
        // 创建定时器（如果尚未创建）
        if (!dht11_timer_) {
            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    auto self = static_cast<atk_dnesp32s3*>(arg);
                    self->ReadDHT11Data();
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "dht11_timer",
                .skip_unhandled_events = false,
            };
            ESP_ERROR_CHECK(esp_timer_create(&timer_args, &dht11_timer_));
        }
        
        // 启动定时器
        ESP_ERROR_CHECK(esp_timer_start_periodic(dht11_timer_, DHT11_READ_INTERVAL_MS * 1000));
        dht11_timer_running_ = true;
        ESP_LOGI(TAG, "DHT11 periodic reading timer started");
    }
    
    /**
     * @brief 停止DHT11定期读取定时器
     * @note 彻底停止定时器并清理资源
     */
    void StopDHT11Timer() {
        if (!dht11_timer_running_) {
            ESP_LOGW(TAG, "DHT11 timer is not running");
            return;
        }
        
        // 停止定时器
        if (dht11_timer_) {
            ESP_ERROR_CHECK(esp_timer_stop(dht11_timer_));
            ESP_ERROR_CHECK(esp_timer_delete(dht11_timer_));
            dht11_timer_ = nullptr;
        }
        
        dht11_timer_running_ = false;
        ESP_LOGI(TAG, "DHT11 periodic reading timer stopped and cleaned up");
    }
};

DECLARE_BOARD(atk_dnesp32s3);
