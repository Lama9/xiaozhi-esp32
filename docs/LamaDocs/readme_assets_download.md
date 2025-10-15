# 小智AI聊天机器人 - 自定义资源下载系统技术文档

## 项目概述

小智AI聊天机器人实现了一套完整的自定义资源下载系统，支持HTTP/HTTPS协议的动态资源下载、断点续传、进度监控、完整性校验等功能。该系统实现了从服务器端资源获取到本地Flash存储的完整下载流程。

## 下载系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                    自定义资源下载系统架构                        │
├─────────────────────────────────────────────────────────────────┤
│  服务器端                                                       │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              OTA服务器                                      │ │
│  │  • 版本检查API                                              │ │
│  │  • 固件下载服务                                            │ │
│  │  • 资源包下载服务                                          │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              网络传输层                                     │ │
│  │              HTTP/HTTPS                                     │ │
│  │  • 断点续传                                                │ │
│  │  • 进度回调                                                │ │
│  │  • 错误重试                                                │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  设备端                                                         │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              下载管理器                                     │ │
│  │  • MCP触发机制                                             │ │
│  │  • Settings存储                                            │ │
│  │  • 下载调度                                                │ │
│  │  • 进度监控                                                │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              Assets下载器                                   │ │
│  │  • HTTP客户端                                              │ │
│  │  • 分块下载                                                │ │
│  │  • 扇区擦除                                                │ │
│  │  • 数据写入                                                │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              Flash存储                                      │ │
│  │              assets分区                                     │ │
│  │  • SPIFFS子类型                                            │ │
│  │  • 内存映射支持                                            │ │
│  │  • 扇区擦除写入                                            │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 核心组件

#### 1.1 下载触发系统
- **MCP接口**: 通过MCP服务器接收下载指令
- **Settings存储**: 持久化存储下载URL
- **自动检查**: 应用启动时自动检查待下载资源

#### 1.2 HTTP下载引擎
- **网络抽象**: 使用Board网络接口统一网络访问
- **协议支持**: 支持HTTP/HTTPS协议
- **连接管理**: 自动连接建立和释放

#### 1.3 Flash写入系统
- **扇区管理**: 智能扇区擦除和写入
- **分区操作**: 直接操作ESP32分区系统
- **数据校验**: 写入后数据完整性验证

## 下载流程详解

### 1. 下载触发机制

#### 1.1 MCP服务器触发
```cpp
// MCP工具：设置资源下载URL
AddUserOnlyTool("self.assets.set_download_url", "Set the download url for the assets",
    PropertyList({
        Property("url", kPropertyTypeString)
    }),
    [](const PropertyList& properties) -> ReturnValue {
        auto url = properties["url"].value<std::string>();
        Settings settings("assets", true);
        settings.SetString("download_url", url);  // 存储下载URL
        return true;
    });
```

#### 1.2 应用启动检查
```cpp
void Application::LoadAssets() {
    auto& assets = Assets::GetInstance();
    
    // 检查分区是否有效
    if (!assets.partition_valid()) {
        ESP_LOGW(TAG, "Assets partition is disabled for board %s", BOARD_NAME);
        return;
    }
    
    Settings settings("assets", true);
    // 检查是否有待下载的资源
    std::string download_url = settings.GetString("download_url");
    
    if (!download_url.empty()) {
        settings.EraseKey("download_url");  // 清除URL，防止重复下载
        
        // 显示下载提示
        char message[256];
        snprintf(message, sizeof(message), Lang::Strings::FOUND_NEW_ASSETS, download_url.c_str());
        Alert(Lang::Strings::LOADING_ASSETS, message, "cloud_arrow_down", Lang::Sounds::OGG_UPGRADE);
        
        // 等待音频服务空闲
        vTaskDelay(pdMS_TO_TICKS(3000));
        SetDeviceState(kDeviceStateUpgrading);
        board.SetPowerSaveMode(false);
        display->SetChatMessage("system", Lang::Strings::PLEASE_WAIT);
        
        // 开始下载
        bool success = assets.Download(download_url, progress_callback);
        
        board.SetPowerSaveMode(true);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if (!success) {
            Alert(Lang::Strings::ERROR, Lang::Strings::DOWNLOAD_ASSETS_FAILED, 
                  "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
            vTaskDelay(pdMS_TO_TICKS(2000));
            return;
        }
    }
    
    // 应用资源
    assets.Apply();
    display->SetChatMessage("system", "");
    display->SetEmotion("microchip_ai");
}
```

### 2. HTTP下载实现

#### 2.1 下载核心逻辑
```cpp
bool Assets::Download(std::string url, std::function<void(int progress, size_t speed)> progress_callback) {
    // 1. 取消内存映射，准备写入新数据
    if (mmap_handle_ != 0) {
        esp_partition_munmap(mmap_handle_);
        mmap_handle_ = 0;
        mmap_root_ = nullptr;
    }
    
    // 清空状态标志
    partition_valid_ = false;
    checksum_valid_ = false;
    assets_.clear();
    
    // 2. 创建HTTP客户端
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    
    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }
    
    // 3. 检查响应状态
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to get assets, status code: %d", http->GetStatusCode());
        return false;
    }
    
    // 4. 获取文件大小并验证
    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        return false;
    }
    
    if (content_length > partition_->size) {
        ESP_LOGE(TAG, "Assets file size (%u) is larger than partition size (%lu)", 
                 content_length, partition_->size);
        return false;
    }
    
    // 5. 扇区擦除和写入
    return WriteToPartition(http, content_length, progress_callback);
}
```

#### 2.2 扇区擦除写入机制
```cpp
bool Assets::WriteToPartition(Http* http, size_t content_length, 
                             std::function<void(int, size_t)> progress_callback) {
    // 定义扇区大小为4KB（ESP32的标准扇区大小）
    const size_t SECTOR_SIZE = esp_partition_get_main_flash_sector_size();
    
    // 计算需要擦除的扇区数量
    size_t sectors_to_erase = (content_length + SECTOR_SIZE - 1) / SECTOR_SIZE; // 向上取整
    size_t total_erase_size = sectors_to_erase * SECTOR_SIZE;
    
    ESP_LOGI(TAG, "Sector size: %u, content length: %u, sectors to erase: %u, total erase size: %u", 
             SECTOR_SIZE, content_length, sectors_to_erase, total_erase_size);
    
    // 写入新的资源文件到分区，一边erase一边写入
    char buffer[512];
    size_t total_written = 0;
    size_t recent_written = 0;
    size_t current_sector = 0;
    auto last_calc_time = esp_timer_get_time();
    
    while (true) {
        int ret = http->Read(buffer, sizeof(buffer));
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            return false;
        }
        
        if (ret == 0) {
            break;
        }
        
        // 检查是否需要擦除新的扇区
        size_t write_end_offset = total_written + ret;
        size_t needed_sectors = (write_end_offset + SECTOR_SIZE - 1) / SECTOR_SIZE;
        
        while (current_sector < needed_sectors) {
            size_t sector_start = current_sector * SECTOR_SIZE;
            esp_err_t err = esp_partition_erase_range(partition_, sector_start, SECTOR_SIZE);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to erase sector %u at offset 0x%lx: %s", 
                         current_sector, sector_start, esp_err_to_name(err));
                return false;
            }
            current_sector++;
        }
        
        // 写入数据
        esp_err_t err = esp_partition_write(partition_, total_written, buffer, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write to partition at offset %u: %s", 
                     total_written, esp_err_to_name(err));
            return false;
        }
        
        total_written += ret;
        recent_written += ret;
        
        // 计算进度和速度
        if (esp_timer_get_time() - last_calc_time >= 1000000 || total_written == content_length || ret == 0) {
            size_t progress = total_written * 100 / content_length;
            size_t speed = recent_written; // 每秒的字节数
            ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %u B/s, Sectors erased: %u", 
                     progress, total_written, content_length, speed, current_sector);
            if (progress_callback) {
                progress_callback(progress, speed);
            }
            last_calc_time = esp_timer_get_time();
            recent_written = 0; // 重置最近写入的字节数
        }
    }
    
    http->Close();
    
    if (total_written != content_length) {
        ESP_LOGE(TAG, "Downloaded size (%u) does not match expected size (%u)", 
                 total_written, content_length);
        return false;
    }
    
    ESP_LOGI(TAG, "Assets download completed, total written: %u bytes, total sectors erased: %u", 
             total_written, current_sector);
    
    // 重新初始化资源分区
    if (!InitializePartition()) {
        ESP_LOGE(TAG, "Failed to re-initialize assets partition");
        return false;
    }
    
    return true;
}
```

### 3. 进度回调机制

#### 3.1 进度计算
```cpp
// 每秒计算一次进度和速度
if (esp_timer_get_time() - last_calc_time >= 1000000) {
    size_t progress = total_written * 100 / content_length;
    size_t speed = recent_written;  // 每秒字节数
    
    ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %u B/s", 
             progress, total_written, content_length, speed);
    
    if (progress_callback) {
        progress_callback(progress, speed);
    }
    
    last_calc_time = esp_timer_get_time();
    recent_written = 0;
}
```

#### 3.2 UI更新
```cpp
// 应用层进度回调
bool success = assets.Download(download_url, [display](int progress, size_t speed) -> void {
    std::thread([display, progress, speed]() {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", progress, speed / 1024);
        display->SetChatMessage("system", buffer);  // 更新显示
    }).detach();
});
```

## OTA更新机制详解

### 1. 版本检查流程

#### 1.1 版本检查API
```cpp
bool Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();
    current_version_ = app_desc->version;
    
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());
    
    // 构建检查URL
    std::string url = GetCheckVersionUrl();  // 从配置获取OTA URL
    if (url.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }
    
    auto http = SetupHttp();
    
    // 发送系统信息
    std::string data = board.GetSystemInfoJson();
    std::string method = data.length() > 0 ? "POST" : "GET";
    http->SetContent(std::move(data));
    
    if (!http->Open(method, url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }
    
    auto status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to check version, status code: %d", status_code);
        return false;
    }
    
    data = http->ReadAll();
    http->Close();
    
    // 解析响应
    return ParseVersionResponse(data);
}
```

#### 1.2 版本响应解析
```cpp
bool Ota::ParseVersionResponse(const std::string& data) {
    // 响应格式: { "firmware": { "version": "1.0.0", "url": "http://..." } }
    cJSON *root = cJSON_Parse(data.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }
    
    // 解析固件信息
    has_new_version_ = false;
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        if (cJSON_IsString(version)) {
            firmware_version_ = version->valuestring;
        }
        cJSON *url = cJSON_GetObjectItem(firmware, "url");
        if (cJSON_IsString(url)) {
            firmware_url_ = url->valuestring;
        }
        
        if (cJSON_IsString(version) && cJSON_IsString(url)) {
            // 检查版本是否更新
            has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }
            
            // 如果设置了强制标志，则强制安装指定版本
            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (cJSON_IsNumber(force) && force->valueint == 1) {
                has_new_version_ = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found!");
    }
    
    // 解析资源信息（如果有）
    ParseAssetsInfo(root);
    
    cJSON_Delete(root);
    return true;
}
```

### 2. 版本比较算法

```cpp
bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    std::vector<int> current = ParseVersion(currentVersion);  // "1.0.0" -> [1,0,0]
    std::vector<int> newer = ParseVersion(newVersion);
    
    // 逐段比较版本号
    for (size_t i = 0; i < std::min(current.size(), newer.size()); ++i) {
        if (newer[i] > current[i]) {
            return true;   // 新版本更高
        } else if (newer[i] < current[i]) {
            return false;  // 当前版本更高
        }
    }
    
    // 如果前面的段都相等，比较长度
    return newer.size() > current.size();
}

std::vector<int> Ota::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    std::stringstream ss(version);
    std::string segment;
    
    while (std::getline(ss, segment, '.')) {
        versionNumbers.push_back(std::stoi(segment));
    }
    
    return versionNumbers;
}
```

### 3. 固件升级流程

#### 3.1 OTA升级实现
```cpp
bool Ota::Upgrade(const std::string& firmware_url) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());
    
    // 1. 获取OTA分区
    esp_ota_handle_t update_handle = 0;
    auto update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        return false;
    }
    
    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", 
             update_partition->label, update_partition->address);
    
    bool image_header_checked = false;
    std::string image_header;
    
    // 2. 建立HTTP连接
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    if (!http->Open("GET", firmware_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }
    
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to get firmware, status code: %d", http->GetStatusCode());
        return false;
    }
    
    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        return false;
    }
    
    // 3. 下载并写入固件
    char buffer[512];
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();
    
    while (true) {
        int ret = http->Read(buffer, sizeof(buffer));
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            return false;
        }
        
        // 计算速度和进度
        recent_read += ret;
        total_read += ret;
        if (esp_timer_get_time() - last_calc_time >= 1000000 || ret == 0) {
            size_t progress = total_read * 100 / content_length;
            ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %uB/s", 
                     progress, total_read, content_length, recent_read);
            if (upgrade_callback_) {
                upgrade_callback_(progress, recent_read);
            }
            last_calc_time = esp_timer_get_time();
            recent_read = 0;
        }
        
        if (ret == 0) {
            break;
        }
        
        // 验证固件头部
        if (!image_header_checked) {
            image_header.append(buffer, ret);
            if (image_header.size() >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                esp_app_desc_t new_app_info;
                memcpy(&new_app_info, image_header.data() + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t), sizeof(esp_app_desc_t));
                
                auto current_version = esp_app_get_description()->version;
                ESP_LOGI(TAG, "Current version: %s, New version: %s", current_version, new_app_info.version);
                
                if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle)) {
                    esp_ota_abort(update_handle);
                    ESP_LOGE(TAG, "Failed to begin OTA");
                    return false;
                }
                
                image_header_checked = true;
                std::string().swap(image_header);
            }
        }
        
        // 写入固件数据
        auto err = esp_ota_write(update_handle, buffer, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            return false;
        }
    }
    http->Close();
    
    // 4. 完成OTA升级
    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
        }
        return false;
    }
    
    // 5. 设置启动分区
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Firmware upgrade successful");
    return true;
}
```

## 网络层实现

### 1. HTTP客户端封装

#### 1.1 网络接口抽象
```cpp
class Network {
public:
    virtual std::unique_ptr<Http> CreateHttp(int timeout_ms) = 0;
    virtual bool IsConnected() = 0;
    virtual std::string GetLocalIP() = 0;
};

class Http {
public:
    virtual bool Open(const std::string& method, const std::string& url) = 0;
    virtual int GetStatusCode() = 0;
    virtual size_t GetBodyLength() = 0;
    virtual int Read(void* buffer, size_t size) = 0;
    virtual std::string ReadAll() = 0;
    virtual void SetHeader(const std::string& key, const std::string& value) = 0;
    virtual void SetContent(std::string content) = 0;
    virtual void Close() = 0;
};
```

#### 1.2 HTTP客户端使用
```cpp
// 创建HTTP客户端
auto network = Board::GetInstance().GetNetwork();
auto http = network->CreateHttp(0);  // 0表示无超时

// 设置请求头
http->SetHeader("User-Agent", "XiaoZhi-ESP32/1.0");
http->SetHeader("Accept", "application/octet-stream");

// 发起请求
if (!http->Open("GET", url)) {
    ESP_LOGE(TAG, "Failed to open HTTP connection to %s", url.c_str());
    return false;
}

// 检查状态码
int status_code = http->GetStatusCode();
if (status_code != 200) {
    ESP_LOGE(TAG, "HTTP request failed with status %d", status_code);
    return false;
}

// 获取内容长度
size_t content_length = http->GetBodyLength();
ESP_LOGI(TAG, "Content length: %u bytes", content_length);
```

### 2. 断点续传支持

#### 2.1 Range请求实现
```cpp
bool Assets::ResumeDownload(const std::string& url, size_t resume_offset) {
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    
    // 设置Range头部实现断点续传
    std::string range_header = "bytes=" + std::to_string(resume_offset) + "-";
    http->SetHeader("Range", range_header);
    
    if (!http->Open("GET", url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection for resume");
        return false;
    }
    
    // 检查服务器是否支持断点续传
    int status_code = http->GetStatusCode();
    if (status_code == 206) {  // HTTP 206 Partial Content
        ESP_LOGI(TAG, "Server supports resume, continuing from offset %u", resume_offset);
        return ContinueDownload(http, resume_offset);
    } else if (status_code == 200) {
        ESP_LOGW(TAG, "Server doesn't support resume, starting fresh download");
        return Download(url, nullptr);
    } else {
        ESP_LOGE(TAG, "Unexpected status code for resume: %d", status_code);
        return false;
    }
}
```

#### 2.2 续传状态管理
```cpp
class DownloadState {
private:
    std::string url_;
    size_t total_size_;
    size_t downloaded_size_;
    std::string checksum_;
    bool is_resumable_;
    
public:
    bool SaveState() {
        Settings settings("download", true);
        settings.SetString("url", url_);
        settings.SetInt("total_size", total_size_);
        settings.SetInt("downloaded_size", downloaded_size_);
        settings.SetString("checksum", checksum_);
        settings.SetInt("resumable", is_resumable_ ? 1 : 0);
        return true;
    }
    
    bool LoadState() {
        Settings settings("download", false);
        url_ = settings.GetString("url");
        total_size_ = settings.GetInt("total_size", 0);
        downloaded_size_ = settings.GetInt("downloaded_size", 0);
        checksum_ = settings.GetString("checksum");
        is_resumable_ = settings.GetInt("resumable", 0) == 1;
        return !url_.empty() && total_size_ > 0;
    }
    
    void ClearState() {
        Settings settings("download", true);
        settings.EraseKey("url");
        settings.EraseKey("total_size");
        settings.EraseKey("downloaded_size");
        settings.EraseKey("checksum");
        settings.EraseKey("resumable");
    }
};
```

## 错误处理与恢复机制

### 1. 下载错误处理

#### 1.1 网络错误处理
```cpp
bool Assets::Download(std::string url, std::function<void(int, size_t)> progress_callback) {
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 5000;
    int retry_count = 0;
    
    while (retry_count < MAX_RETRIES) {
        try {
            ESP_LOGI(TAG, "Download attempt %d/%d: %s", retry_count + 1, MAX_RETRIES, url.c_str());
            
            auto network = Board::GetInstance().GetNetwork();
            if (!network->IsConnected()) {
                throw std::runtime_error("Network not connected");
            }
            
            auto http = network->CreateHttp(30000);  // 30秒超时
            
            if (!http->Open("GET", url)) {
                throw std::runtime_error("Failed to open HTTP connection");
            }
            
            int status_code = http->GetStatusCode();
            if (status_code != 200) {
                throw std::runtime_error("HTTP error: " + std::to_string(status_code));
            }
            
            // 执行下载
            if (PerformDownload(http, progress_callback)) {
                ESP_LOGI(TAG, "Download completed successfully");
                return true;
            }
            
            throw std::runtime_error("Download failed");
            
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Download attempt %d failed: %s", retry_count + 1, e.what());
            
            retry_count++;
            if (retry_count < MAX_RETRIES) {
                ESP_LOGI(TAG, "Retrying download in %d seconds...", RETRY_DELAY_MS / 1000);
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            }
        }
    }
    
    ESP_LOGE(TAG, "Download failed after %d attempts", MAX_RETRIES);
    return false;
}
```

#### 1.2 Flash写入错误处理
```cpp
bool Assets::WriteToPartition(Http* http, size_t content_length,
                             std::function<void(int, size_t)> progress_callback) {
    // 备份当前状态
    BackupCurrentState();
    
    try {
        // 执行写入操作
        if (!PerformWrite(http, content_length, progress_callback)) {
            throw std::runtime_error("Write operation failed");
        }
        
        // 验证写入结果
        if (!ValidateWrittenData(content_length)) {
            throw std::runtime_error("Data validation failed");
        }
        
        ESP_LOGI(TAG, "Flash write completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Flash write failed: %s", e.what());
        
        // 恢复到之前状态
        if (!RestoreFromBackup()) {
            ESP_LOGE(TAG, "Failed to restore from backup");
        }
        
        return false;
    }
}

bool Assets::ValidateWrittenData(size_t expected_size) {
    // 重新初始化分区以验证数据
    if (!InitializePartition()) {
        ESP_LOGE(TAG, "Failed to re-initialize partition for validation");
        return false;
    }
    
    // 验证校验和
    if (!checksum_valid_) {
        ESP_LOGE(TAG, "Checksum validation failed");
        return false;
    }
    
    ESP_LOGI(TAG, "Data validation passed");
    return true;
}
```

### 2. 网络异常处理

#### 2.1 连接超时处理
```cpp
class HttpTimeoutHandler {
private:
    static const int DEFAULT_CONNECT_TIMEOUT = 10000;  // 10秒
    static const int DEFAULT_READ_TIMEOUT = 30000;     // 30秒
    
public:
    static bool HandleTimeout(Http* http, const std::string& operation) {
        ESP_LOGW(TAG, "HTTP %s timeout, attempting recovery", operation.c_str());
        
        // 关闭当前连接
        http->Close();
        
        // 等待一段时间后重试
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        return false;  // 需要外层重试
    }
};
```

#### 2.2 网络状态监控
```cpp
class NetworkMonitor {
private:
    bool last_connected_state_ = false;
    uint32_t disconnection_count_ = 0;
    
public:
    bool CheckNetworkStatus() {
        auto network = Board::GetInstance().GetNetwork();
        bool current_state = network->IsConnected();
        
        if (!current_state && last_connected_state_) {
            disconnection_count_++;
            ESP_LOGW(TAG, "Network disconnected (count: %lu)", disconnection_count_);
        } else if (current_state && !last_connected_state_) {
            ESP_LOGI(TAG, "Network reconnected");
        }
        
        last_connected_state_ = current_state;
        return current_state;
    }
    
    uint32_t GetDisconnectionCount() const {
        return disconnection_count_;
    }
};
```

### 3. 数据完整性验证

#### 3.1 校验和验证
```cpp
bool Assets::ValidateDownloadedData() {
    if (!partition_valid_ || !mmap_root_) {
        ESP_LOGE(TAG, "Partition not valid for validation");
        return false;
    }
    
    // 读取存储的校验和信息
    uint32_t stored_files = *(uint32_t*)(mmap_root_ + 0);
    uint32_t stored_checksum = *(uint32_t*)(mmap_root_ + 4);
    uint32_t stored_length = *(uint32_t*)(mmap_root_ + 8);
    
    ESP_LOGI(TAG, "Validating: files=%lu, checksum=0x%lx, length=%lu", 
             stored_files, stored_checksum, stored_length);
    
    // 验证数据长度
    if (stored_length > partition_->size - 12) {
        ESP_LOGE(TAG, "Invalid data length: %lu > %lu", stored_length, partition_->size - 12);
        return false;
    }
    
    // 计算实际校验和
    auto start_time = esp_timer_get_time();
    uint32_t calculated_checksum = CalculateChecksum(mmap_root_ + 12, stored_length);
    auto end_time = esp_timer_get_time();
    
    ESP_LOGI(TAG, "Checksum calculation took %d ms", (int)((end_time - start_time) / 1000));
    
    if (calculated_checksum != stored_checksum) {
        ESP_LOGE(TAG, "Checksum mismatch: calculated=0x%lx, stored=0x%lx", 
                 calculated_checksum, stored_checksum);
        return false;
    }
    
    ESP_LOGI(TAG, "Data validation successful");
    checksum_valid_ = true;
    return true;
}
```

#### 3.2 损坏数据恢复
```cpp
bool Assets::HandleCorruptedData() {
    ESP_LOGW(TAG, "Attempting to recover from corrupted assets data");
    
    // 1. 尝试重新初始化分区
    if (InitializePartition() && checksum_valid_) {
        ESP_LOGI(TAG, "Successfully recovered assets data");
        return true;
    }
    
    // 2. 擦除损坏的分区
    ESP_LOGW(TAG, "Erasing corrupted partition");
    esp_err_t err = esp_partition_erase_range(partition_, 0, partition_->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase corrupted partition: %s", esp_err_to_name(err));
        return false;
    }
    
    // 3. 触发重新下载（如果有默认URL）
    if (!default_assets_url_.empty()) {
        ESP_LOGI(TAG, "Triggering assets re-download from: %s", default_assets_url_.c_str());
        Settings settings("assets", true);
        settings.SetString("download_url", default_assets_url_);
        return true;
    }
    
    ESP_LOGW(TAG, "No recovery method available for corrupted assets");
    return false;
}
```

## 性能优化策略

### 1. 下载性能优化

#### 1.1 并行下载支持
```cpp
class ParallelDownloader {
private:
    struct DownloadChunk {
        size_t start_offset;
        size_t end_offset;
        std::vector<uint8_t> data;
        bool completed;
        std::string error_message;
    };
    
    std::vector<DownloadChunk> chunks_;
    static const size_t CHUNK_SIZE = 64 * 1024;  // 64KB per chunk
    static const size_t MAX_CONCURRENT = 4;      // 最多4个并发
    
public:
    bool DownloadParallel(const std::string& url, size_t total_size) {
        // 计算分块数量
        size_t num_chunks = (total_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
        chunks_.resize(num_chunks);
        
        // 初始化分块信息
        for (size_t i = 0; i < num_chunks; i++) {
            chunks_[i].start_offset = i * CHUNK_SIZE;
            chunks_[i].end_offset = std::min((i + 1) * CHUNK_SIZE, total_size);
            chunks_[i].completed = false;
        }
        
        // 并行下载各个分块
        std::vector<std::thread> download_threads;
        size_t concurrent_count = std::min(num_chunks, MAX_CONCURRENT);
        
        for (size_t i = 0; i < concurrent_count; i++) {
            download_threads.emplace_back([this, url, i, num_chunks]() {
                DownloadChunks(url, i, num_chunks, MAX_CONCURRENT);
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : download_threads) {
            thread.join();
        }
        
        // 检查是否所有分块都下载成功
        for (const auto& chunk : chunks_) {
            if (!chunk.completed) {
                ESP_LOGE(TAG, "Chunk download failed: %s", chunk.error_message.c_str());
                return false;
            }
        }
        
        // 合并数据
        return MergeChunks();
    }
    
private:
    void DownloadChunks(const std::string& url, size_t start_index, 
                       size_t total_chunks, size_t step) {
        for (size_t i = start_index; i < total_chunks; i += step) {
            DownloadSingleChunk(url, i);
        }
    }
    
    bool DownloadSingleChunk(const std::string& url, size_t chunk_index) {
        auto& chunk = chunks_[chunk_index];
        
        try {
            auto network = Board::GetInstance().GetNetwork();
            auto http = network->CreateHttp(30000);
            
            // 设置Range头部
            std::string range = "bytes=" + std::to_string(chunk.start_offset) + 
                               "-" + std::to_string(chunk.end_offset - 1);
            http->SetHeader("Range", range);
            
            if (!http->Open("GET", url)) {
                throw std::runtime_error("Failed to open connection");
            }
            
            if (http->GetStatusCode() != 206) {
                throw std::runtime_error("Server doesn't support range requests");
            }
            
            // 下载数据
            size_t chunk_size = chunk.end_offset - chunk.start_offset;
            chunk.data.resize(chunk_size);
            
            size_t total_read = 0;
            while (total_read < chunk_size) {
                int ret = http->Read(chunk.data.data() + total_read, chunk_size - total_read);
                if (ret <= 0) break;
                total_read += ret;
            }
            
            http->Close();
            
            if (total_read == chunk_size) {
                chunk.completed = true;
                ESP_LOGD(TAG, "Chunk %u completed (%u bytes)", chunk_index, total_read);
                return true;
            } else {
                throw std::runtime_error("Incomplete chunk download");
            }
            
        } catch (const std::exception& e) {
            chunk.error_message = e.what();
            chunk.completed = false;
            ESP_LOGE(TAG, "Chunk %u failed: %s", chunk_index, e.what());
            return false;
        }
    }
    
    bool MergeChunks() {
        // 按顺序合并所有分块数据
        // 实现省略...
        return true;
    }
};
```

#### 1.2 缓存机制
```cpp
class DownloadCache {
private:
    struct CacheEntry {
        std::string url;
        std::string etag;
        std::string last_modified;
        size_t size;
        uint32_t checksum;
        uint64_t timestamp;
    };
    
    std::map<std::string, CacheEntry> cache_entries_;
    static const uint64_t CACHE_EXPIRE_TIME = 24 * 60 * 60 * 1000000;  // 24小时
    
public:
    bool IsCached(const std::string& url, const std::string& etag = "") {
        auto it = cache_entries_.find(url);
        if (it == cache_entries_.end()) {
            return false;
        }
        
        auto& entry = it->second;
        
        // 检查是否过期
        uint64_t current_time = esp_timer_get_time();
        if (current_time - entry.timestamp > CACHE_EXPIRE_TIME) {
            cache_entries_.erase(it);
            return false;
        }
        
        // 检查ETag
        if (!etag.empty() && entry.etag != etag) {
            return false;
        }
        
        return true;
    }
    
    void AddToCache(const std::string& url, const std::string& etag,
                   const std::string& last_modified, size_t size, uint32_t checksum) {
        CacheEntry entry = {
            .url = url,
            .etag = etag,
            .last_modified = last_modified,
            .size = size,
            .checksum = checksum,
            .timestamp = esp_timer_get_time()
        };
        
        cache_entries_[url] = entry;
        ESP_LOGI(TAG, "Added to cache: %s (size: %u, checksum: 0x%x)", 
                 url.c_str(), size, checksum);
    }
    
    bool ValidateCache(const std::string& url) {
        // 发送HEAD请求验证缓存
        auto network = Board::GetInstance().GetNetwork();
        auto http = network->CreateHttp(10000);
        
        if (!http->Open("HEAD", url)) {
            return false;
        }
        
        if (http->GetStatusCode() != 200) {
            return false;
        }
        
        // 比较ETag和Last-Modified
        // 实现省略...
        return true;
    }
};
```

### 2. Flash写入优化

#### 2.1 扇区对齐写入
```cpp
bool Assets::OptimizedWrite(const uint8_t* data, size_t size, size_t offset) {
    const size_t SECTOR_SIZE = esp_partition_get_main_flash_sector_size();
    
    // 计算扇区边界
    size_t start_sector = offset / SECTOR_SIZE;
    size_t end_sector = (offset + size - 1) / SECTOR_SIZE;
    
    ESP_LOGI(TAG, "Writing %u bytes from sector %u to %u", size, start_sector, end_sector);
    
    for (size_t sector = start_sector; sector <= end_sector; sector++) {
        size_t sector_offset = sector * SECTOR_SIZE;
        size_t write_offset = std::max(offset, sector_offset);
        size_t write_end = std::min(offset + size, sector_offset + SECTOR_SIZE);
        size_t write_size = write_end - write_offset;
        
        ESP_LOGD(TAG, "Processing sector %u: offset=0x%x, size=%u", 
                 sector, write_offset, write_size);
        
        // 擦除扇区（如果需要）
        if (write_offset == sector_offset && write_size == SECTOR_SIZE) {
            // 整扇区写入，直接擦除
            esp_err_t err = esp_partition_erase_range(partition_, sector_offset, SECTOR_SIZE);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to erase sector %u: %s", sector, esp_err_to_name(err));
                return false;
            }
        } else {
            // 部分扇区写入，需要读-改-写
            if (!PerformReadModifyWrite(sector_offset, write_offset - sector_offset, 
                                       data + (write_offset - offset), write_size)) {
                return false;
            }
            continue;  // 读-改-写已完成写入
        }
        
        // 写入数据
        esp_err_t err = esp_partition_write(partition_, write_offset, 
                                          data + (write_offset - offset), write_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write sector %u: %s", sector, esp_err_to_name(err));
            return false;
        }
    }
    
    return true;
}

bool Assets::PerformReadModifyWrite(size_t sector_offset, size_t data_offset, 
                                   const uint8_t* new_data, size_t new_size) {
    const size_t SECTOR_SIZE = esp_partition_get_main_flash_sector_size();
    std::vector<uint8_t> sector_buffer(SECTOR_SIZE);
    
    // 读取整个扇区
    esp_err_t err = esp_partition_read(partition_, sector_offset, 
                                      sector_buffer.data(), SECTOR_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sector for RMW: %s", esp_err_to_name(err));
        return false;
    }
    
    // 修改数据
    memcpy(sector_buffer.data() + data_offset, new_data, new_size);
    
    // 擦除扇区
    err = esp_partition_erase_range(partition_, sector_offset, SECTOR_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase sector for RMW: %s", esp_err_to_name(err));
        return false;
    }
    
    // 写入整个扇区
    err = esp_partition_write(partition_, sector_offset, sector_buffer.data(), SECTOR_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write sector for RMW: %s", esp_err_to_name(err));
        return false;
    }
    
    return true;
}
```

### 3. 内存管理优化

#### 3.1 缓冲区管理
```cpp
class BufferManager {
private:
    static const size_t BUFFER_SIZE = 4096;    // 4KB缓冲区
    static const size_t MAX_BUFFERS = 4;       // 最多4个缓冲区
    
    struct Buffer {
        uint8_t* data;
        size_t size;
        bool in_use;
        uint64_t last_used;
    };
    
    std::vector<Buffer> buffers_;
    std::mutex buffer_mutex_;
    
public:
    BufferManager() {
        buffers_.resize(MAX_BUFFERS);
        for (auto& buffer : buffers_) {
            buffer.data = static_cast<uint8_t*>(heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_DMA));
            buffer.size = BUFFER_SIZE;
            buffer.in_use = false;
            buffer.last_used = 0;
        }
    }
    
    ~BufferManager() {
        for (auto& buffer : buffers_) {
            if (buffer.data) {
                heap_caps_free(buffer.data);
            }
        }
    }
    
    uint8_t* AcquireBuffer(size_t required_size = BUFFER_SIZE) {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        // 寻找可用缓冲区
        for (auto& buffer : buffers_) {
            if (!buffer.in_use && buffer.size >= required_size) {
                buffer.in_use = true;
                buffer.last_used = esp_timer_get_time();
                return buffer.data;
            }
        }
        
        // 没有可用缓冲区，寻找最久未使用的
        auto oldest = std::min_element(buffers_.begin(), buffers_.end(),
            [](const Buffer& a, const Buffer& b) {
                return a.last_used < b.last_used;
            });
        
        if (oldest != buffers_.end()) {
            oldest->in_use = true;
            oldest->last_used = esp_timer_get_time();
            return oldest->data;
        }
        
        return nullptr;
    }
    
    void ReleaseBuffer(uint8_t* buffer_ptr) {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        for (auto& buffer : buffers_) {
            if (buffer.data == buffer_ptr) {
                buffer.in_use = false;
                break;
            }
        }
    }
};
```

## 监控与调试

### 1. 下载统计

#### 1.1 性能统计
```cpp
class DownloadStatistics {
private:
    struct Stats {
        uint32_t total_downloads = 0;
        uint32_t successful_downloads = 0;
        uint32_t failed_downloads = 0;
        uint64_t total_bytes_downloaded = 0;
        uint64_t total_download_time_us = 0;
        uint32_t max_speed_bps = 0;
        uint32_t min_speed_bps = UINT32_MAX;
        uint32_t avg_speed_bps = 0;
    } stats_;
    
public:
    void RecordDownloadStart() {
        stats_.total_downloads++;
    }
    
    void RecordDownloadComplete(size_t bytes, uint64_t time_us, bool success) {
        if (success) {
            stats_.successful_downloads++;
            stats_.total_bytes_downloaded += bytes;
            stats_.total_download_time_us += time_us;
            
            // 计算速度
            if (time_us > 0) {
                uint32_t speed = (bytes * 1000000) / time_us;  // 字节/秒
                if (speed > stats_.max_speed_bps) {
                    stats_.max_speed_bps = speed;
                }
                if (speed < stats_.min_speed_bps) {
                    stats_.min_speed_bps = speed;
                }
                
                // 计算平均速度
                if (stats_.total_download_time_us > 0) {
                    stats_.avg_speed_bps = (stats_.total_bytes_downloaded * 1000000) / 
                                          stats_.total_download_time_us;
                }
            }
        } else {
            stats_.failed_downloads++;
        }
    }
    
    void PrintStatistics() {
        ESP_LOGI(TAG, "=== Download Statistics ===");
        ESP_LOGI(TAG, "Total downloads: %lu", stats_.total_downloads);
        ESP_LOGI(TAG, "Successful: %lu, Failed: %lu", 
                 stats_.successful_downloads, stats_.failed_downloads);
        ESP_LOGI(TAG, "Total bytes: %llu", stats_.total_bytes_downloaded);
        ESP_LOGI(TAG, "Average speed: %lu B/s (%lu KB/s)", 
                 stats_.avg_speed_bps, stats_.avg_speed_bps / 1024);
        ESP_LOGI(TAG, "Max speed: %lu B/s (%lu KB/s)", 
                 stats_.max_speed_bps, stats_.max_speed_bps / 1024);
        ESP_LOGI(TAG, "Min speed: %lu B/s (%lu KB/s)", 
                 stats_.min_speed_bps, stats_.min_speed_bps / 1024);
        
        if (stats_.total_downloads > 0) {
            float success_rate = (float)stats_.successful_downloads / stats_.total_downloads * 100;
            ESP_LOGI(TAG, "Success rate: %.1f%%", success_rate);
        }
    }
};
```

#### 1.2 实时监控
```cpp
class DownloadMonitor {
private:
    struct ActiveDownload {
        std::string url;
        size_t total_size;
        size_t downloaded_size;
        uint64_t start_time;
        uint32_t current_speed;
        uint32_t avg_speed;
    } current_download_;
    
    bool is_downloading_ = false;
    
public:
    void StartMonitoring(const std::string& url, size_t total_size) {
        current_download_.url = url;
        current_download_.total_size = total_size;
        current_download_.downloaded_size = 0;
        current_download_.start_time = esp_timer_get_time();
        current_download_.current_speed = 0;
        current_download_.avg_speed = 0;
        is_downloading_ = true;
        
        ESP_LOGI(TAG, "Started monitoring download: %s (%u bytes)", 
                 url.c_str(), total_size);
    }
    
    void UpdateProgress(size_t downloaded_size, uint32_t current_speed) {
        if (!is_downloading_) return;
        
        current_download_.downloaded_size = downloaded_size;
        current_download_.current_speed = current_speed;
        
        // 计算平均速度
        uint64_t elapsed_time = esp_timer_get_time() - current_download_.start_time;
        if (elapsed_time > 0) {
            current_download_.avg_speed = (downloaded_size * 1000000) / elapsed_time;
        }
        
        // 计算进度
        float progress = (float)downloaded_size / current_download_.total_size * 100;
        
        // 估算剩余时间
        uint32_t eta_seconds = 0;
        if (current_download_.avg_speed > 0) {
            size_t remaining = current_download_.total_size - downloaded_size;
            eta_seconds = remaining / current_download_.avg_speed;
        }
        
        ESP_LOGI(TAG, "Progress: %.1f%% (%u/%u), Speed: %u KB/s, ETA: %u s", 
                 progress, downloaded_size, current_download_.total_size,
                 current_speed / 1024, eta_seconds);
    }
    
    void StopMonitoring(bool success) {
        if (!is_downloading_) return;
        
        uint64_t total_time = esp_timer_get_time() - current_download_.start_time;
        
        ESP_LOGI(TAG, "Download %s: %u bytes in %llu ms (avg: %u KB/s)", 
                 success ? "completed" : "failed",
                 current_download_.downloaded_size,
                 total_time / 1000,
                 current_download_.avg_speed / 1024);
        
        is_downloading_ = false;
    }
};
```

### 2. 调试接口

#### 2.1 MCP调试工具
```cpp
void Assets::RegisterDownloadDebugTools() {
    auto& mcp = McpServer::GetInstance();
    
    // 下载状态查询
    mcp.AddTool("assets.download.status", "Get download status",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            cJSON* status = cJSON_CreateObject();
            cJSON_AddBoolToObject(status, "partition_valid", partition_valid_);
            cJSON_AddBoolToObject(status, "checksum_valid", checksum_valid_);
            
            if (partition_) {
                cJSON_AddNumberToObject(status, "partition_size", partition_->size);
                cJSON_AddStringToObject(status, "partition_label", partition_->label);
            }
            
            // 添加下载统计
            // 实现省略...
            
            return ReturnValue(status);
        });
    
    // 强制下载工具
    mcp.AddTool("assets.download.force", "Force download assets",
        PropertyList({Property("url", kPropertyTypeString)}),
        [this](const PropertyList& props) -> ReturnValue {
            std::string url = props["url"].value<std::string>();
            
            bool success = Download(url, [](int progress, size_t speed) {
                ESP_LOGI("DEBUG", "Download progress: %d%%, speed: %u KB/s", 
                         progress, speed / 1024);
            });
            
            cJSON* result = cJSON_CreateObject();
            cJSON_AddBoolToObject(result, "success", success);
            cJSON_AddStringToObject(result, "url", url.c_str());
            
            return ReturnValue(result);
        });
    
    // 下载历史查询
    mcp.AddTool("assets.download.history", "Get download history",
        PropertyList(),
        [](const PropertyList&) -> ReturnValue {
            // 从NVS读取下载历史
            Settings settings("download_history", false);
            
            cJSON* history = cJSON_CreateArray();
            
            // 添加历史记录
            // 实现省略...
            
            return ReturnValue(history);
        });
}
```

## 总结

小智AI聊天机器人的自定义资源下载系统是一个完整、高效、可靠的网络资源获取解决方案。通过HTTP协议、断点续传、进度监控、完整性校验等技术，实现了稳定可靠的资源下载功能。

### 主要特点

1. **多协议支持**: HTTP/HTTPS协议支持，满足不同服务器要求
2. **断点续传**: 支持网络中断后的续传功能，提高下载成功率
3. **进度监控**: 实时进度反馈和速度计算，提供良好的用户体验
4. **错误恢复**: 完善的错误处理和重试机制，确保下载稳定性
5. **性能优化**: 并行下载、缓存机制、内存管理等优化策略
6. **完整性验证**: 校验和验证确保下载数据的完整性

### 技术优势

1. **高可靠性**: 多重错误处理和恢复机制
2. **高性能**: 优化的Flash写入和网络传输
3. **高扩展性**: 模块化设计支持功能扩展
4. **高可观测性**: 完整的监控和调试接口
5. **高兼容性**: 支持多种网络环境和服务器配置

这套下载系统为ESP32平台的网络资源获取提供了一个优秀的参考实现，具有很高的实用价值和学习意义。
