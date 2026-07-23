#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include <cJSON.h>
#include "mcp_server.h"
#include "display/lvgl_display/lvgl_theme.h"
// #include "lamp_controller.h" // Removed since we use GPIO46 for capture

#include "led/single_led.h"
#include "esp_video.h"
#include <wifi_station.h>
#include <esp_video_init.h>
#include <esp_cam_ctlr.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include "power_save_timer.h"
#include "power_manager.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#if defined(LCD_TYPE_ILI9341_SERIAL)
#include "esp_lcd_ili9341.h"
#endif

#if defined(LCD_TYPE_GC9A01_SERIAL)
#include "esp_lcd_gc9a01.h"
static const gc9a01_lcd_init_cmd_t gc9107_lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xfe, (uint8_t[]){0x00}, 0, 0},
    {0xef, (uint8_t[]){0x00}, 0, 0},
    {0xb0, (uint8_t[]){0xc0}, 1, 0},
    {0xb1, (uint8_t[]){0x80}, 1, 0},
    {0xb2, (uint8_t[]){0x27}, 1, 0},
    {0xb3, (uint8_t[]){0x13}, 1, 0},
    {0xb6, (uint8_t[]){0x19}, 1, 0},
    {0xb7, (uint8_t[]){0x05}, 1, 0},
    {0xac, (uint8_t[]){0xc8}, 1, 0},
    {0xab, (uint8_t[]){0x0f}, 1, 0},
    {0x3a, (uint8_t[]){0x05}, 1, 0},
    {0xb4, (uint8_t[]){0x04}, 1, 0},
    {0xa8, (uint8_t[]){0x08}, 1, 0},
    {0xb8, (uint8_t[]){0x08}, 1, 0},
    {0xea, (uint8_t[]){0x02}, 1, 0},
    {0xe8, (uint8_t[]){0x2A}, 1, 0},
    {0xe9, (uint8_t[]){0x47}, 1, 0},
    {0xe7, (uint8_t[]){0x5f}, 1, 0},
    {0xc6, (uint8_t[]){0x21}, 1, 0},
    {0xc7, (uint8_t[]){0x15}, 1, 0},
    {0xf0,
    (uint8_t[]){0x1D, 0x38, 0x09, 0x4D, 0x92, 0x2F, 0x35, 0x52, 0x1E, 0x0C,
                0x04, 0x12, 0x14, 0x1f},
    14, 0},
    {0xf1,
    (uint8_t[]){0x16, 0x40, 0x1C, 0x54, 0xA9, 0x2D, 0x2E, 0x56, 0x10, 0x0D,
                0x0C, 0x1A, 0x14, 0x1E},
    14, 0},
    {0xf4, (uint8_t[]){0x00, 0x00, 0xFF}, 3, 0},
    {0xba, (uint8_t[]){0xFF, 0xFF}, 2, 0},
};
#endif
 
#define TAG "GoouuuCam"

#pragma message("GoouuuCam")

// LV_FONT_DECLARE(font_puhui_16_4);
// LV_FONT_DECLARE(font_awesome_16_4);

class GoouuuCamLcdDisplay : public SpiLcdDisplay {
public:
    GoouuuCamLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    virtual void SetupUI() override {
        // Call the parent's SetupUI to build the standard layout
        SpiLcdDisplay::SetupUI();
        
        // --- GoouuuCam Text Elevation Fix ---
        if (content_ != nullptr) {
            // WeChat style chat mode: add 30px safe area at the bottom
            lv_obj_set_style_pad_bottom(content_, 30, 0); 
        }
        
        if (bottom_bar_ != nullptr) {
            // Generic mode text bar: shift the entire bar upwards by 20 pixels
            lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -20);
        }
        
        if (low_battery_popup_ != nullptr) {
            // Also shift the low battery popup up so it doesn't overlap the new text position
            lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -45);
        }
    }

    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override {
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
        // --- WECHAT STYLE PREVIEW (Bubbles) ---
        DisplayLockGuard lock(this);
        if (content_ == nullptr || image == nullptr) {
            return;
        }
        
        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        lv_obj_t* img_bubble = lv_obj_create(content_);
        lv_obj_set_style_radius(img_bubble, 8, 0);
        lv_obj_set_scrollbar_mode(img_bubble, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_border_width(img_bubble, 0, 0);
        lv_obj_set_style_pad_all(img_bubble, lvgl_theme->spacing(4), 0);
        
        lv_obj_set_style_bg_color(img_bubble, lvgl_theme->assistant_bubble_color(), 0);
        lv_obj_set_style_bg_opa(img_bubble, LV_OPA_70, 0);
        lv_obj_set_user_data(img_bubble, (void*)"image");

        lv_obj_t* preview_image = lv_image_create(img_bubble);
        
        // Expand the image to strictly fill most of the 240x320 screen
        lv_coord_t max_width = LV_HOR_RES * 95 / 100;
        lv_coord_t max_height = LV_VER_RES * 65 / 100;
        
        auto img_dsc = image->image_dsc();
        lv_coord_t img_width = img_dsc->header.w;
        lv_coord_t img_height = img_dsc->header.h;
        if (img_width == 0 || img_height == 0) {
            img_width = max_width;
            img_height = max_height;
        }
        
        lv_coord_t zoom_w = (max_width * 256) / img_width;
        lv_coord_t zoom_h = (max_height * 256) / img_height;
        lv_coord_t zoom = (zoom_w < zoom_h) ? zoom_w : zoom_h;
        if (zoom > 256) zoom = 256;
        
        lv_image_set_src(preview_image, img_dsc);
        lv_image_set_scale(preview_image, zoom);
        
        LvglImage* raw_image = image.release();
        lv_obj_add_event_cb(preview_image, [](lv_event_t* e) {
            LvglImage* img = (LvglImage*)lv_event_get_user_data(e);
            if (img != nullptr) {
                delete img;
            }
        }, LV_EVENT_DELETE, (void*)raw_image);
        
        lv_coord_t scaled_width = (img_width * zoom) / 256;
        lv_coord_t scaled_height = (img_height * zoom) / 256;
        
        lv_obj_set_width(img_bubble, scaled_width + 16);
        lv_obj_set_height(img_bubble, scaled_height + 16);
        lv_obj_set_style_flex_grow(img_bubble, 0, 0);
        lv_obj_center(preview_image);
        lv_obj_align(img_bubble, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_scroll_to_view_recursive(img_bubble, LV_ANIM_ON);
#else
        // --- GENERIC STYLE PREVIEW (Centered Background) ---
        DisplayLockGuard lock(this);
        if (preview_image_ == nullptr) {
            ESP_LOGE(TAG, "Preview image is not initialized");
            return;
        }

        if (image == nullptr) {
            esp_timer_stop(preview_timer_);
            lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
            preview_image_cached_.reset();
            return;
        }

        preview_image_cached_ = std::move(image);
        auto img_dsc = preview_image_cached_->image_dsc();
        lv_image_set_src(preview_image_, img_dsc);

        if (img_dsc->header.w > 0 && img_dsc->header.h > 0) {
            lv_coord_t max_width = LV_HOR_RES * 95 / 100;
            // Native scaling logic: 256 means 100%. 
            // The original logic used `128 * width_ / img_dsc->header.w` (50% scale max)
            lv_coord_t zoom = (max_width * 256) / img_dsc->header.w;
            if (zoom > 256) zoom = 256;
            
            lv_image_set_scale(preview_image_, zoom);
            
            // Align it center, slightly elevated to stay away from bottom text
            lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, -20);
        }

        if (gif_controller_) {
            gif_controller_->Stop();
        }
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        esp_timer_stop(preview_timer_);
        ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
#endif
    }
};

class GoouuuCam : public WifiBoard {
private:
 
    Button boot_button_;
    Button capture_button_;
    TaskHandle_t preview_task_ = nullptr;
    volatile bool is_previewing_ = false;
    uint32_t preview_start_time_ = 0;

    LcdDisplay* display_;
    Camera* camera_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    void StartPreview() {
        if (is_previewing_) return;
        is_previewing_ = true;
        preview_start_time_ = esp_timer_get_time() / 1000;
        
        display_->SetChatMessage("system", "进入相机预览模式...");
        
        xTaskCreate([](void* arg) {
            GoouuuCam* self = static_cast<GoouuuCam*>(arg);
            while (self->is_previewing_) {
                if (self->camera_->Capture()) {
                    // Capture successfully updates the LCD
                } else {
                    vTaskDelay(pdMS_TO_TICKS(100)); // Delay on error
                }
                
                // Timeout after 60 seconds
                if ((esp_timer_get_time() / 1000) - self->preview_start_time_ > 60000) {
                    ESP_LOGI(TAG, "Preview timeout");
                    self->is_previewing_ = false;
                    break;
                }
                
                // Control frame rate
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            
            self->display_->SetChatMessage("system", ""); // Clear message
            self->preview_task_ = nullptr;
            vTaskDelete(NULL);
        }, "preview_task", 4096, this, 2, &preview_task_);
    }

    void StopPreview() {
        if (is_previewing_) {
            is_previewing_ = false;
        }
    }

    void ExecuteCaptureAndExplain() {
        StopPreview();
        
        // 延时150ms，确保后台的 is_previewing_ 循环执行完毕并释放摄像头硬件控制权
        vTaskDelay(pdMS_TO_TICKS(150)); 
        
        display_->SetChatMessage("system", "正在锁定画面...");
        
        // 触发全局语音助手链路，让它接管后续拍照上传和播报语音的流程
        // auto& app = Application::GetInstance();
        // 伪造语音指令让服务器主动调用摄像头工具
        // app.WakeWordInvoke("帮忙看下这是什么"); 
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }
    void InitializeCamera() {
        static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                [0] = CAMERA_PIN_D0,
                [1] = CAMERA_PIN_D1,
                [2] = CAMERA_PIN_D2,
                [3] = CAMERA_PIN_D3,
                [4] = CAMERA_PIN_D4,
                [5] = CAMERA_PIN_D5,
                [6] = CAMERA_PIN_D6,
                [7] = CAMERA_PIN_D7,
            },
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io = CAMERA_PIN_HREF,
            .pclk_io = CAMERA_PIN_PCLK,
            .xclk_io = CAMERA_PIN_XCLK,
        };

        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port = I2C_NUM_0,
                .scl_pin = CAMERA_PIN_SIOC,
                .sda_pin = CAMERA_PIN_SIOD,
            },
            .freq = 100000,
        };

        esp_video_init_dvp_config_t dvp_config = {
            .sccb_config = sccb_config,
            .reset_pin = CAMERA_PIN_RESET,
            .pwdn_pin = CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pin_config,
            .xclk_freq = XCLK_FREQ_HZ,
        };

        esp_video_init_config_t video_config = {
            .dvp = &dvp_config,
        };

        camera_ = new EspVideo(video_config);
        camera_->SetHMirror(false);
    }
    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_19);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_48);
        rtc_gpio_set_direction(GPIO_NUM_48, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level(GPIO_NUM_48, 1);

        power_save_timer_ = new PowerSaveTimer(-1, 60, 3600);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            display_->SetChatMessage("system", "");
            display_->SetEmotion("sleepy");
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            rtc_gpio_set_level(GPIO_NUM_48, 0);
            // 启用保持功能，确保睡眠期间电平不变
            rtc_gpio_hold_en(GPIO_NUM_48);
            esp_lcd_panel_disp_on_off(panel_, false); //关闭显示
            esp_deep_sleep_start();
            // pmic_->PowerOff();
        });
        power_save_timer_->SetEnabled(true);
    }
    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
#if defined(LCD_TYPE_ILI9341_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
#elif defined(LCD_TYPE_GC9A01_SERIAL)
        ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(panel_io, &panel_config, &panel));
        gc9a01_vendor_config_t gc9107_vendor_config = {
            .init_cmds = gc9107_lcd_init_cmds,
            .init_cmds_size = sizeof(gc9107_lcd_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
        };        
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
#endif
        
        esp_lcd_panel_reset(panel);
 

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
#ifdef  LCD_TYPE_GC9A01_SERIAL
        panel_config.vendor_config = &gc9107_vendor_config;
#endif
        display_ = new GoouuuCamLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }


 
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        capture_button_.OnClick([this]() {
            if (!is_previewing_) {
                ESP_LOGI(TAG, "Button 2 pressed: Starting preview");
                StartPreview();
            } else {
                ESP_LOGI(TAG, "Button 2 pressed: capture and explain");
                ExecuteCaptureAndExplain();
            }
        });
        
        capture_button_.OnLongPress([this]() {
            if (is_previewing_) {
                ESP_LOGI(TAG, "Button 2 long pressed: Stopping preview");
                StopPreview();
                display_->SetChatMessage("system", "已退出预览模式");
            }
        });
    }


    // 获取软件版本的函数
    std::string GetSoftwareVersion() {
        const esp_app_desc_t* app_desc = esp_app_get_description();
        ESP_LOGI(TAG, "Get software version: %s", app_desc->version);
        return std::string(app_desc->version);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeTools() {
        // static LampController lamp(LAMP_GPIO);

        auto& mcp_server = McpServer::GetInstance();
        
        // 定义设备的属性
        mcp_server.AddTool("self.goouuu.get_software_version", "获取设备当前软件版本", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            return GetSoftwareVersion();
        });
        
        // === 电池状态查询工具 ===
        // 获取电池电量百分比
        mcp_server.AddTool("self.goouuu.get_battery_level", "获取设备自身电池电量百分比，当前电池电量。", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            uint8_t level = power_manager_->GetBatteryLevel();
            return std::to_string(level) + "%";
        });
        
        // 获取电池电压
        mcp_server.AddTool("self.goouuu.get_battery_voltage", "获取设备自身电池电压", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            float voltage = power_manager_->GetBatteryVoltage();
            char voltage_str[32];
            snprintf(voltage_str, sizeof(voltage_str), "%.3fV", voltage);
            return std::string(voltage_str);
        });
        
        // 获取原始ADC值
        mcp_server.AddTool("self.goouuu.get_battery_adc", "获取设备自身电池ADC原始值", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            uint32_t adc_value = power_manager_->GetRawAdcValue();
            return std::to_string(adc_value);
        });
        
        // 获取充电状态
        mcp_server.AddTool("self.goouuu.get_charging_status", "获取充电状态", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            bool is_charging = power_manager_->IsCharging();
            return is_charging ? "正在充电" : "未充电";
        });
        
        // 获取放电状态
        mcp_server.AddTool("self.goouuu.get_discharging_status", "获取放电状态", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            bool is_discharging = power_manager_->IsDischarging();
            return is_discharging ? "正在放电" : "未放电";
        });
        
        // 获取完整电池状态信息
        mcp_server.AddTool("self.goouuu.get_battery_status", "获取完整的设备自身电池状态信息", PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
            uint8_t level = power_manager_->GetBatteryLevel();
            float voltage = power_manager_->GetBatteryVoltage();
            uint32_t adc_value = power_manager_->GetRawAdcValue();
            bool is_charging = power_manager_->IsCharging();
            bool is_discharging = power_manager_->IsDischarging();
            
            char status_str[256];
            snprintf(status_str, sizeof(status_str), 
                "电池状态信息:\n"
                "电量: %d%%\n"
                "电压: %.3fV\n"
                "ADC值: %lu\n"
                "充电状态: %s\n"
                "放电状态: %s",
                level, voltage, adc_value,
                is_charging ? "充电中" : "未充电",
                is_discharging ? "放电中" : "未放电"
            );
            return std::string(status_str);
        });

        // 设置 UDP 日志开关
        mcp_server.AddTool("self.goouuu.set_udp_log_enabled", "开启或关闭 UDP 日志广播", 
            PropertyList({
                Property("enabled", kPropertyTypeBoolean)
            }), 
            [this](const PropertyList& properties) -> ReturnValue {
            bool enabled = properties["enabled"].value<bool>();
            power_manager_->SetUdpLogEnabled(enabled);
            return enabled ? "UDP日志已开启" : "UDP日志已关闭";
        });
    }

public:
    GoouuuCam() :
        boot_button_(BOOT_BUTTON_GPIO),
        capture_button_(CAPTURE_BUTTON_GPIO) {
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeTools();
        InitializeCamera();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
        

    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }
    virtual Camera* GetCamera() override {
        return camera_;
    }
    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    // virtual void SetPowerSaveMode(bool enabled) override {
    //     if (!enabled) {
    //         power_save_timer_->WakeUp();
    //     }
    //     WifiBoard::SetPowerSaveMode(enabled);
    // }
};

DECLARE_BOARD(GoouuuCam);
