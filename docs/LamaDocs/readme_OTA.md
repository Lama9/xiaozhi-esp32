# 小智AI聊天机器人 - OTA固件升级系统技术文档

## 项目概述

小智AI聊天机器人实现了一套完整的OTA（Over-The-Air）固件升级系统，支持自动版本检查、语义化版本比较、HTTP固件下载、分区管理、回滚保护等功能。该系统确保设备能够安全、可靠地进行远程固件更新。

## OTA系统架构

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                    OTA固件升级系统架构                           │
├─────────────────────────────────────────────────────────────────┤
│  服务器端                                                       │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              OTA服务器                                      │ │
│  │  • 版本管理API                                              │ │
│  │  • 固件存储服务                                            │ │
│  │  • 设备认证系统                                            │ │
│  │  • 升级策略管理                                            │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              HTTP/HTTPS协议                                 │ │
│  │  • 版本检查请求                                            │ │
│  │  • 固件下载传输                                            │ │
│  │  • 设备信息上报                                            │ │
│  │  • 激活码验证                                              │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  设备端                                                         │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              OTA管理器                                      │ │
│  │  • 版本检查调度                                            │ │
│  │  • 语义化版本比较                                          │ │
│  │  • 升级流程控制                                            │ │
│  │  • 回滚保护机制                                            │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              ESP32 OTA分区系统                              │ │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐           │ │
│  │  │ Factory │ │  OTA_0  │ │  OTA_1  │ │ Boot    │           │ │
│  │  │ (出厂)  │ │ (主分区)│ │ (备分区)│ │ (引导)  │           │ │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘           │ │
│  └─────────────────────────────────────────────────────────────┘ │
│                           │                                     │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              应用层集成                                     │ │
│  │  • 用户界面通知                                            │ │
│  │  • 进度显示更新                                            │ │
│  │  • 系统状态管理                                            │ │
│  │  • 错误处理展示                                            │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 核心组件

#### 1.1 OTA类 - 升级管理器
```cpp
class Ota {
public:
    // 版本管理
    bool CheckVersion();                                    // 检查新版本
    bool HasNewVersion() { return has_new_version_; }       // 是否有新版本
    const std::string& GetFirmwareVersion() const;         // 获取固件版本
    const std::string& GetCurrentVersion() const;          // 获取当前版本
    
    // 升级操作
    bool StartUpgrade(std::function<void(int, size_t)> callback);        // 开始升级
    bool StartUpgradeFromUrl(const std::string& url, callback);          // 从URL升级
    void MarkCurrentVersionValid();                        // 标记当前版本有效
    
    // 配置管理
    std::string GetCheckVersionUrl();                       // 获取版本检查URL
    
private:
    bool Upgrade(const std::string& firmware_url);          // 核心升级逻辑
    std::vector<int> ParseVersion(const std::string& version);           // 版本解析
    bool IsNewVersionAvailable(const std::string& current, 
                              const std::string& newer);   // 版本比较
};
```

#### 1.2 版本管理系统
- **当前版本获取**: 从ESP32应用描述符读取
- **服务器版本检查**: HTTP请求获取最新版本信息
- **版本比较算法**: 语义化版本号比较
- **强制更新支持**: 服务器可强制推送更新

#### 1.3 分区管理系统
- **双分区设计**: OTA_0和OTA_1交替使用
- **回滚保护**: 升级失败自动回滚到前一版本
- **分区状态管理**: 跟踪分区有效性和启动状态

## OTA服务器部署与API规范

### 1. 服务器API规范

#### 1.1 API文档规范
- **官方API文档**: https://ccnphfhqs21z.feishu.cn/wiki/FjW6wZmisimNBBkov6OcmfvknVd
- **协议**: HTTP/HTTPS (强制HTTPS用于生产环境)
- **方法**: POST (推荐) / GET (兼容性)
- **内容类型**: application/json
- **字符编码**: UTF-8

#### 1.2 请求端点配置
```cpp
// 默认OTA服务器配置 (Kconfig)
config OTA_URL
    string "Default OTA URL"
    default "https://api.tenclass.net/xiaozhi/ota/"
    help
        The application will access this URL to check for new firmwares and server address.
```

**URL配置优先级**:
1. **运行时配置**: `Settings("wifi").GetString("ota_url")`
2. **编译时配置**: `CONFIG_OTA_URL` (Kconfig)
3. **默认配置**: `"https://api.tenclass.net/xiaozhi/ota/"`

### 2. 设备请求格式

#### 2.1 HTTP请求头
```http
POST /xiaozhi/ota/ HTTP/1.1
Host: api.tenclass.net
Content-Type: application/json
Accept-Language: zh-CN
User-Agent: BOARD_NAME/1.0.0
Activation-Version: 2
Device-Id: aa:bb:cc:dd:ee:ff
Client-Id: 12345678-1234-5678-9abc-123456789abc
Serial-Number: DEVICE_SERIAL_NUMBER_HERE
Authorization: HMAC <hmac_signature>
```

**请求头说明**:
- `User-Agent`: 板型名称/固件版本
- `Activation-Version`: 激活协议版本 (1或2)
- `Device-Id`: 设备MAC地址
- `Client-Id`: 设备UUID (软件生成)
- `Serial-Number`: 设备序列号 (从eFuse读取)
- `Authorization`: HMAC签名认证 (可选)

#### 2.2 设备系统信息JSON
```json
{
    "version": 2,
    "language": "zh-CN",
    "flash_size": 16777216,
    "minimum_free_heap_size": 123456,
    "mac_address": "aa:bb:cc:dd:ee:ff",
    "uuid": "12345678-1234-5678-9abc-123456789abc",
    "chip_model_name": "esp32s3",
    "chip_info": {
        "model": 9,
        "cores": 2,
        "revision": 0,
        "features": 50
    },
    "application": {
        "name": "xiaozhi-assistant",
        "version": "1.0.0",
        "compile_time": "2024-01-01T12:00:00Z",
        "idf_version": "5.1.2",
        "elf_sha256": "a1b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef123456"
    },
    "partition_table": [
        {
            "label": "nvs",
            "type": 1,
            "subtype": 2,
            "address": 36864,
            "size": 16384
        },
        {
            "label": "factory",
            "type": 0,
            "subtype": 0,
            "address": 65536,
            "size": 1048576
        },
        {
            "label": "ota_0",
            "type": 0,
            "subtype": 16,
            "address": 1114112,
            "size": 1048576
        },
        {
            "label": "ota_1",
            "type": 0,
            "subtype": 17,
            "address": 2162688,
            "size": 1048576
        }
    ],
    "ota": {
        "label": "ota_0"
    },
    "display": {
        "monochrome": false,
        "width": 320,
        "height": 240
    },
    "board": {
        "type": "esp32s3_korvo2_v3",
        "hardware_version": "1.0",
        "features": ["wifi", "bluetooth", "display", "microphone", "speaker"]
    }
}
```

**系统信息字段说明**:
- `version`: API版本号
- `language`: 设备语言设置
- `flash_size`: Flash存储大小 (字节)
- `minimum_free_heap_size`: 最小空闲堆内存
- `chip_info`: ESP32芯片详细信息
- `application`: 当前固件信息
- `partition_table`: 分区表信息
- `ota`: 当前运行的OTA分区
- `display`: 显示屏信息
- `board`: 板型和硬件信息

### 3. 服务器响应格式

#### 3.1 成功响应 (HTTP 200)
```json
{
    "firmware": {
        "version": "1.1.0",
        "url": "https://firmware.example.com/xiaozhi/v1.1.0/firmware.bin",
        "force": 0,
        "checksum": "sha256:abcd1234567890...",
        "size": 2097152,
        "release_notes": "修复了音频处理bug，增加了新的语音识别功能"
    },
    "server_time": 1704067200000,
    "activation_code": "ABC123DEF456",
    "activation_message": "设备激活成功",
    "activation_challenge": "challenge_string_for_hmac",
    "mqtt_config": {
        "host": "mqtt.example.com",
        "port": 8883,
        "username": "device_user",
        "password": "device_pass",
        "client_id": "xiaozhi_device_123",
        "use_ssl": true
    },
    "websocket_config": {
        "url": "wss://ws.example.com/xiaozhi",
        "protocols": ["xiaozhi-protocol-v1"],
        "heartbeat_interval": 30
    }
}
```

**响应字段说明**:
- `firmware`: 固件更新信息
  - `version`: 最新固件版本号
  - `url`: 固件下载URL (必须HTTPS)
  - `force`: 强制更新标志 (0=否, 1=是)
  - `checksum`: 固件校验和 (可选)
  - `size`: 固件文件大小 (字节)
  - `release_notes`: 版本更新说明
- `server_time`: 服务器时间戳 (毫秒)
- `activation_code`: 设备激活码
- `activation_message`: 激活消息
- `activation_challenge`: HMAC认证挑战字符串
- `mqtt_config`: MQTT连接配置
- `websocket_config`: WebSocket连接配置

#### 3.2 错误响应
```json
// HTTP 400 - 请求格式错误
{
    "error": "invalid_request",
    "message": "Invalid JSON format in request body",
    "code": 4001
}

// HTTP 401 - 认证失败
{
    "error": "authentication_failed",
    "message": "Invalid device credentials",
    "code": 4011
}

// HTTP 403 - 设备未授权
{
    "error": "device_not_authorized",
    "message": "Device not registered or suspended",
    "code": 4031
}

// HTTP 404 - 固件不存在
{
    "error": "firmware_not_found",
    "message": "No firmware available for this device",
    "code": 4041
}

// HTTP 500 - 服务器内部错误
{
    "error": "internal_server_error",
    "message": "Temporary service unavailable",
    "code": 5001
}
```

### 4. 服务器部署要求

#### 4.1 基础设施要求
```yaml
# 服务器配置要求
server:
  cpu: 2+ cores
  memory: 4GB+ RAM
  storage: 100GB+ SSD
  bandwidth: 100Mbps+
  
# 网络要求
network:
  protocol: HTTPS (TLS 1.2+)
  certificate: 有效SSL证书
  cdn: 建议使用CDN加速固件下载
  
# 数据库
database:
  type: PostgreSQL/MySQL/MongoDB
  size: 根据设备数量规划
  backup: 定期备份策略
```

#### 4.2 技术栈建议
```yaml
# 后端框架选择
backend:
  nodejs: Express.js / Koa.js / NestJS
  python: Django / FastAPI / Flask
  golang: Gin / Echo / Fiber
  java: Spring Boot
  
# 数据存储
storage:
  database: PostgreSQL (推荐) / MySQL / MongoDB
  cache: Redis (设备状态缓存)
  file_storage: AWS S3 / 阿里云OSS / 本地存储
  
# 部署方式
deployment:
  container: Docker + Docker Compose
  orchestration: Kubernetes (大规模部署)
  cloud: AWS / 阿里云 / 腾讯云
  monitoring: Prometheus + Grafana
```

#### 4.3 安全配置
```yaml
# SSL/TLS配置
ssl:
  protocol: TLS 1.2+
  cipher_suites: 强加密套件
  hsts: 启用HTTP严格传输安全
  
# 访问控制
access_control:
  rate_limiting: 每设备每分钟最多10次请求
  ip_whitelist: 可选IP白名单
  geo_blocking: 可选地理位置限制
  
# 数据安全
data_security:
  encryption: 数据库敏感信息加密
  audit_log: 完整的审计日志
  backup: 加密备份
```

### 5. 服务器实现示例

#### 5.1 Node.js + Express 实现
```javascript
const express = require('express');
const crypto = require('crypto');
const semver = require('semver');

const app = express();
app.use(express.json());

// 设备版本检查端点
app.post('/xiaozhi/ota/', async (req, res) => {
    try {
        // 1. 验证请求格式
        const deviceInfo = req.body;
        if (!deviceInfo || !deviceInfo.application) {
            return res.status(400).json({
                error: 'invalid_request',
                message: 'Missing device information'
            });
        }
        
        // 2. 提取设备信息
        const currentVersion = deviceInfo.application.version;
        const deviceId = req.headers['device-id'];
        const boardType = deviceInfo.board?.type || 'unknown';
        
        // 3. 查询最新固件版本
        const latestFirmware = await getFirmwareForDevice(boardType, currentVersion);
        
        // 4. 构建响应
        const response = {
            server_time: Date.now()
        };
        
        if (latestFirmware && semver.gt(latestFirmware.version, currentVersion)) {
            response.firmware = {
                version: latestFirmware.version,
                url: latestFirmware.downloadUrl,
                force: latestFirmware.forceUpdate ? 1 : 0,
                size: latestFirmware.fileSize,
                checksum: latestFirmware.checksum,
                release_notes: latestFirmware.releaseNotes
            };
        }
        
        // 5. 添加配置信息
        const deviceConfig = await getDeviceConfig(deviceId);
        if (deviceConfig.mqttConfig) {
            response.mqtt_config = deviceConfig.mqttConfig;
        }
        if (deviceConfig.websocketConfig) {
            response.websocket_config = deviceConfig.websocketConfig;
        }
        
        res.json(response);
        
    } catch (error) {
        console.error('OTA check error:', error);
        res.status(500).json({
            error: 'internal_server_error',
            message: 'Service temporarily unavailable'
        });
    }
});

// 固件查询函数
async function getFirmwareForDevice(boardType, currentVersion) {
    // 从数据库查询固件信息
    const firmware = await db.collection('firmwares')
        .findOne({
            boardType: boardType,
            status: 'published'
        }, {
            sort: { version: -1 }
        });
    
    return firmware;
}

// 设备配置查询函数
async function getDeviceConfig(deviceId) {
    const config = await db.collection('device_configs')
        .findOne({ deviceId: deviceId });
    
    return config || {};
}

app.listen(3000, () => {
    console.log('OTA server listening on port 3000');
});
```

#### 5.2 Python + FastAPI 实现
```python
from fastapi import FastAPI, HTTPException, Request
from pydantic import BaseModel
from typing import Optional, Dict, Any
import semver
import time

app = FastAPI(title="Xiaozhi OTA Server")

class DeviceInfo(BaseModel):
    version: int
    language: str
    mac_address: str
    uuid: str
    chip_model_name: str
    application: Dict[str, Any]
    board: Dict[str, Any]

class FirmwareInfo(BaseModel):
    version: str
    url: str
    force: int = 0
    size: Optional[int] = None
    checksum: Optional[str] = None
    release_notes: Optional[str] = None

@app.post("/xiaozhi/ota/")
async def check_version(device_info: DeviceInfo, request: Request):
    try:
        # 提取设备信息
        current_version = device_info.application.get("version")
        device_id = request.headers.get("device-id")
        board_type = device_info.board.get("type", "unknown")
        
        # 查询最新固件
        latest_firmware = await get_firmware_for_device(board_type, current_version)
        
        # 构建响应
        response = {
            "server_time": int(time.time() * 1000)
        }
        
        # 检查是否需要更新
        if latest_firmware and semver.compare(latest_firmware["version"], current_version) > 0:
            response["firmware"] = {
                "version": latest_firmware["version"],
                "url": latest_firmware["download_url"],
                "force": 1 if latest_firmware.get("force_update") else 0,
                "size": latest_firmware.get("file_size"),
                "checksum": latest_firmware.get("checksum"),
                "release_notes": latest_firmware.get("release_notes")
            }
        
        # 添加设备配置
        device_config = await get_device_config(device_id)
        if device_config.get("mqtt_config"):
            response["mqtt_config"] = device_config["mqtt_config"]
        if device_config.get("websocket_config"):
            response["websocket_config"] = device_config["websocket_config"]
            
        return response
        
    except Exception as e:
        raise HTTPException(status_code=500, detail="Internal server error")

async def get_firmware_for_device(board_type: str, current_version: str):
    # 数据库查询逻辑
    # 这里需要根据实际数据库实现
    pass

async def get_device_config(device_id: str):
    # 设备配置查询逻辑
    pass

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
```

#### 5.3 数据库设计
```sql
-- 固件版本表
CREATE TABLE firmwares (
    id SERIAL PRIMARY KEY,
    version VARCHAR(32) NOT NULL,
    board_type VARCHAR(64) NOT NULL,
    download_url TEXT NOT NULL,
    file_size BIGINT,
    checksum VARCHAR(128),
    force_update BOOLEAN DEFAULT FALSE,
    release_notes TEXT,
    status VARCHAR(32) DEFAULT 'published',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 设备表
CREATE TABLE devices (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(64) UNIQUE NOT NULL,
    mac_address VARCHAR(32),
    serial_number VARCHAR(64),
    board_type VARCHAR(64),
    current_version VARCHAR(32),
    last_check_time TIMESTAMP,
    last_upgrade_time TIMESTAMP,
    status VARCHAR(32) DEFAULT 'active',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 设备配置表
CREATE TABLE device_configs (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(64) REFERENCES devices(device_id),
    mqtt_config JSONB,
    websocket_config JSONB,
    activation_code VARCHAR(128),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 升级日志表
CREATE TABLE upgrade_logs (
    id SERIAL PRIMARY KEY,
    device_id VARCHAR(64) REFERENCES devices(device_id),
    from_version VARCHAR(32),
    to_version VARCHAR(32),
    status VARCHAR(32), -- 'started', 'completed', 'failed'
    error_message TEXT,
    started_at TIMESTAMP,
    completed_at TIMESTAMP
);

-- 索引
CREATE INDEX idx_firmwares_board_version ON firmwares(board_type, version);
CREATE INDEX idx_devices_device_id ON devices(device_id);
CREATE INDEX idx_upgrade_logs_device_time ON upgrade_logs(device_id, started_at);
```

### 6. 部署配置示例

#### 6.1 Docker Compose 部署
```yaml
# docker-compose.yml
version: '3.8'

services:
  ota-server:
    build: .
    ports:
      - "3000:3000"
    environment:
      - NODE_ENV=production
      - DATABASE_URL=postgresql://user:pass@postgres:5432/ota_db
      - REDIS_URL=redis://redis:6379
    depends_on:
      - postgres
      - redis
    volumes:
      - ./firmwares:/app/firmwares
      
  postgres:
    image: postgres:14
    environment:
      - POSTGRES_DB=ota_db
      - POSTGRES_USER=user
      - POSTGRES_PASSWORD=pass
    volumes:
      - postgres_data:/var/lib/postgresql/data
      
  redis:
    image: redis:7-alpine
    volumes:
      - redis_data:/data
      
  nginx:
    image: nginx:alpine
    ports:
      - "443:443"
      - "80:80"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
      - ./ssl:/etc/ssl/certs
    depends_on:
      - ota-server

volumes:
  postgres_data:
  redis_data:
```

#### 6.2 Nginx 配置
```nginx
# nginx.conf
server {
    listen 443 ssl http2;
    server_name api.example.com;
    
    ssl_certificate /etc/ssl/certs/server.crt;
    ssl_certificate_key /etc/ssl/certs/server.key;
    
    # SSL 安全配置
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256;
    ssl_prefer_server_ciphers off;
    
    # 安全头
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
    add_header X-Content-Type-Options nosniff;
    add_header X-Frame-Options DENY;
    
    # 速率限制
    limit_req_zone $binary_remote_addr zone=ota:10m rate=10r/m;
    
    location /xiaozhi/ota/ {
        limit_req zone=ota burst=5 nodelay;
        
        proxy_pass http://ota-server:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # 超时设置
        proxy_connect_timeout 30s;
        proxy_send_timeout 30s;
        proxy_read_timeout 30s;
    }
    
    # 固件下载
    location /firmwares/ {
        alias /var/www/firmwares/;
        expires 1d;
        add_header Cache-Control "public, immutable";
    }
}
```

### 7. 监控与运维

#### 7.1 监控指标
```yaml
# Prometheus 监控指标
metrics:
  - ota_requests_total: OTA检查请求总数
  - ota_requests_duration: 请求处理时间
  - firmware_downloads_total: 固件下载次数
  - upgrade_success_rate: 升级成功率
  - device_online_count: 在线设备数量
  - error_rate: 错误率
```

#### 7.2 告警规则
```yaml
# 告警规则示例
alerts:
  - name: HighErrorRate
    condition: error_rate > 0.05
    duration: 5m
    message: "OTA服务错误率过高"
    
  - name: SlowResponse
    condition: ota_requests_duration > 5s
    duration: 2m
    message: "OTA服务响应时间过长"
```

## 版本检查机制详解

### 1. 版本检查流程

#### 1.1 获取当前版本
```cpp
bool Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();
    
    // 获取当前固件版本
    current_version_ = app_desc->version;  // 例如: "1.0.0"
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());
    
    // 构建版本检查URL
    std::string url = GetCheckVersionUrl();
    if (url.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }
    
    // 设置HTTP客户端
    auto http = SetupHttp();
    std::string data = board.GetSystemInfoJson();
    std::string method = data.length() > 0 ? "POST" : "GET";
    http->SetContent(std::move(data));
    
    // 发送版本检查请求
    if (!http->Open(method, url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }
    
    auto status_code = http->GetStatusCode();
    if (status_code != 200) {
        ESP_LOGE(TAG, "Failed to check version, status code: %d", status_code);
        return false;
    }
    
    // 解析服务器响应
    data = http->ReadAll();
    http->Close();
    
    return ParseVersionResponse(data);
}
```

#### 1.2 URL配置机制
```cpp
std::string Ota::GetCheckVersionUrl() {
    Settings settings("wifi", false);
    std::string url = settings.GetString("ota_url");
    if (url.empty()) {
        url = CONFIG_OTA_URL;  // 默认: "https://api.tenclass.net/xiaozhi/ota/"
    }
    return url;
}
```

**配置优先级**:
1. **运行时配置**: Settings中存储的`ota_url`
2. **编译时配置**: Kconfig中的`CONFIG_OTA_URL`
3. **默认配置**: `"https://api.tenclass.net/xiaozhi/ota/"`

#### 1.3 HTTP客户端设置
```cpp
std::unique_ptr<Http> Ota::SetupHttp() {
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    auto http = network->CreateHttp(0);
    
    // 设置请求头
    auto user_agent = SystemInfo::GetUserAgent();
    http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());
    
    if (has_serial_number_) {
        http->SetHeader("Serial-Number", serial_number_.c_str());
    }
    
    http->SetHeader("User-Agent", user_agent);
    http->SetHeader("Accept-Language", Lang::CODE);
    http->SetHeader("Content-Type", "application/json");
    
    return http;
}
```

### 2. 版本响应解析

#### 2.1 JSON响应格式
```json
{
    "firmware": {
        "version": "1.1.0",
        "url": "https://example.com/firmware/v1.1.0.bin",
        "force": 0
    },
    "server_time": 1640995200000,
    "activation_code": "ABC123",
    "activation_challenge": "challenge_string",
    "mqtt_config": {
        "host": "mqtt.example.com",
        "port": 1883
    },
    "websocket_config": {
        "url": "wss://ws.example.com"
    }
}
```

#### 2.2 响应解析实现
```cpp
bool Ota::ParseVersionResponse(const std::string& data) {
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
            // 版本比较
            has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
            
            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }
            
            // 强制更新检查
            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (cJSON_IsNumber(force) && force->valueint == 1) {
                has_new_version_ = true;
                ESP_LOGI(TAG, "Force update flag detected");
            }
        }
    }
    
    // 解析其他配置信息
    ParseServerTime(root);
    ParseActivationInfo(root);
    ParseMqttConfig(root);
    ParseWebsocketConfig(root);
    
    cJSON_Delete(root);
    return true;
}
```

### 3. 版本比较算法

#### 3.1 版本解析函数
```cpp
std::vector<int> Ota::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    std::stringstream ss(version);
    std::string segment;
    
    // 按"."分割版本字符串
    while (std::getline(ss, segment, '.')) {
        try {
            versionNumbers.push_back(std::stoi(segment));
        } catch (const std::exception& e) {
            ESP_LOGW(TAG, "Invalid version segment: %s", segment.c_str());
            versionNumbers.push_back(0);  // 默认值
        }
    }
    
    return versionNumbers;
}
```

**解析示例**:
- `"1.0.0"` → `[1, 0, 0]`
- `"2.1.3"` → `[2, 1, 3]`
- `"1.0"` → `[1, 0]`
- `"3.2.1.beta"` → `[3, 2, 1, 0]` (非数字段转换为0)

#### 3.2 版本比较核心算法
```cpp
bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    std::vector<int> current = ParseVersion(currentVersion);
    std::vector<int> newer = ParseVersion(newVersion);
    
    // 逐段比较版本号
    size_t max_length = std::max(current.size(), newer.size());
    
    for (size_t i = 0; i < max_length; ++i) {
        int current_segment = (i < current.size()) ? current[i] : 0;
        int newer_segment = (i < newer.size()) ? newer[i] : 0;
        
        if (newer_segment > current_segment) {
            return true;   // 新版本更高，需要更新
        } else if (newer_segment < current_segment) {
            return false;  // 当前版本更高，无需更新
        }
        // 相等则继续比较下一段
    }
    
    // 所有段都相等，无需更新
    return false;
}
```

#### 3.3 版本比较示例表

| 当前版本 | 服务器版本 | 比较过程 | 结果 | 说明 |
|---------|-----------|---------|------|------|
| "1.0.0" | "1.0.1" | [1,0,0] vs [1,0,1] | 需要更新 | 第三段: 1 > 0 |
| "1.0.0" | "1.1.0" | [1,0,0] vs [1,1,0] | 需要更新 | 第二段: 1 > 0 |
| "1.0.0" | "2.0.0" | [1,0,0] vs [2,0,0] | 需要更新 | 第一段: 2 > 1 |
| "2.0.0" | "1.9.9" | [2,0,0] vs [1,9,9] | 无需更新 | 第一段: 1 < 2 |
| "1.0.0" | "1.0.0" | [1,0,0] vs [1,0,0] | 无需更新 | 完全相同 |
| "1.0" | "1.0.0" | [1,0] vs [1,0,0] | 无需更新 | 补0后相同 |
| "1.0.0" | "1.0" | [1,0,0] vs [1,0] | 无需更新 | 补0后相同 |

### 4. 版本检查调度

#### 4.1 应用启动时检查
```cpp
void Application::CheckNewVersion(Ota& ota) {
    const int MAX_RETRY = 10;
    int retry_count = 0;
    int retry_delay = 10; // 初始重试延迟为10秒
    
    auto& board = Board::GetInstance();
    while (true) {
        SetDeviceState(kDeviceStateActivating);
        auto display = board.GetDisplay();
        display->SetStatus(Lang::Strings::CHECKING_NEW_VERSION);
        
        // 执行版本检查
        if (!ota.CheckVersion()) {
            retry_count++;
            if (retry_count >= MAX_RETRY) {
                ESP_LOGE(TAG, "Too many retries, exit version check");
                return;
            }
            
            // 显示错误信息
            char buffer[256];
            snprintf(buffer, sizeof(buffer), Lang::Strings::CHECK_NEW_VERSION_FAILED, 
                    retry_delay, ota.GetCheckVersionUrl().c_str());
            Alert(Lang::Strings::ERROR, buffer, "cloud_slash", Lang::Sounds::OGG_EXCLAMATION);
            
            ESP_LOGW(TAG, "Check new version failed, retry in %d seconds (%d/%d)", 
                    retry_delay, retry_count, MAX_RETRY);
            
            // 指数退避重试
            for (int i = 0; i < retry_delay; i++) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                if (device_state_ == kDeviceStateIdle) {
                    break;  // 用户中断
                }
            }
            retry_delay *= 2; // 每次重试后延迟时间翻倍
            continue;
        }
        
        // 重置重试计数器
        retry_count = 0;
        retry_delay = 10;
        
        // 检查是否有新版本
        if (ota.HasNewVersion()) {
            if (UpgradeFirmware(ota)) {
                return; // 升级成功，设备将重启
            }
            // 升级失败，继续正常运行
        }
        
        // 没有新版本，标记当前版本有效
        ota.MarkCurrentVersionValid();
        
        // 检查是否还有其他任务
        if (!ota.HasActivationCode() && !ota.HasActivationChallenge()) {
            xEventGroupSetBits(event_group_, MAIN_EVENT_CHECK_NEW_VERSION_DONE);
            break;
        }
        
        // 处理激活流程
        display->SetStatus(Lang::Strings::ACTIVATION);
        // ... 激活逻辑
    }
}
```

#### 4.2 重试策略
- **最大重试次数**: 10次
- **指数退避**: 10s → 20s → 40s → 80s → ...
- **用户中断**: 支持用户中断重试过程
- **错误提示**: 显示详细的错误信息和重试倒计时

## 固件升级机制详解

### 1. 升级流程概述

#### 1.1 升级触发条件
1. **自动触发**: 版本检查发现新版本
2. **强制更新**: 服务器设置force标志
3. **手动升级**: 通过MCP接口手动触发

#### 1.2 升级前准备
```cpp
bool Application::UpgradeFirmware(Ota& ota, const std::string& url) {
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    
    // 确定升级URL和版本信息
    std::string upgrade_url = url.empty() ? ota.GetFirmwareUrl() : url;
    std::string version_info = url.empty() ? ota.GetFirmwareVersion() : "(Manual upgrade)";
    
    // 关闭音频通道
    if (protocol_ && protocol_->IsAudioChannelOpened()) {
        ESP_LOGI(TAG, "Closing audio channel before firmware upgrade");
        protocol_->CloseAudioChannel();
    }
    
    ESP_LOGI(TAG, "Starting firmware upgrade from URL: %s", upgrade_url.c_str());
    
    // 显示升级提示
    Alert(Lang::Strings::OTA_UPGRADE, Lang::Strings::UPGRADING, "download", Lang::Sounds::OGG_UPGRADE);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 设置升级状态
    SetDeviceState(kDeviceStateUpgrading);
    std::string message = std::string(Lang::Strings::NEW_VERSION) + version_info;
    display->SetChatMessage("system", message.c_str());
    
    // 停止音频服务和省电模式
    board.SetPowerSaveMode(false);
    audio_service_.Stop();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 开始升级
    bool upgrade_success = ota.StartUpgradeFromUrl(upgrade_url, 
        [display](int progress, size_t speed) {
            std::thread([display, progress, speed]() {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", progress, speed / 1024);
                display->SetChatMessage("system", buffer);
            }).detach();
        });
    
    if (!upgrade_success) {
        // 升级失败，恢复正常运行
        ESP_LOGE(TAG, "Firmware upgrade failed, restarting audio service...");
        audio_service_.Start();
        board.SetPowerSaveMode(true);
        Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
        vTaskDelay(pdMS_TO_TICKS(3000));
        return false;
    } else {
        // 升级成功，重启设备
        ESP_LOGI(TAG, "Firmware upgrade successful, rebooting...");
        display->SetChatMessage("system", "Upgrade successful, rebooting...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        Reboot();
        return true;
    }
}
```

### 2. 核心升级实现

#### 2.1 OTA分区获取
```cpp
bool Ota::Upgrade(const std::string& firmware_url) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());
    
    // 1. 获取下一个可用的OTA分区
    esp_ota_handle_t update_handle = 0;
    auto update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        return false;
    }
    
    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", 
             update_partition->label, update_partition->address);
```

**分区选择逻辑**:
- ESP32自动选择非当前运行的OTA分区
- 双分区设计：OTA_0 ↔ OTA_1 交替使用
- 确保升级过程中当前固件仍可运行

#### 2.2 HTTP固件下载
```cpp
    // 2. 建立HTTP连接
    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    if (!http->Open("GET", firmware_url)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        return false;
    }
    
    // 检查HTTP响应
    if (http->GetStatusCode() != 200) {
        ESP_LOGE(TAG, "Failed to get firmware, status code: %d", http->GetStatusCode());
        return false;
    }
    
    // 获取固件大小
    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        return false;
    }
    
    ESP_LOGI(TAG, "Firmware size: %u bytes", content_length);
```

#### 2.3 固件头部验证
```cpp
    // 3. 下载并验证固件头部
    bool image_header_checked = false;
    std::string image_header;
    char buffer[512];
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();
    
    while (true) {
        int ret = http->Read(buffer, sizeof(buffer));
        if (ret < 0) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
            return false;
        }
        
        if (ret == 0) break; // 下载完成
        
        // 验证固件头部
        if (!image_header_checked) {
            image_header.append(buffer, ret);
            
            // 检查是否已收集足够的头部信息
            size_t required_header_size = sizeof(esp_image_header_t) + 
                                         sizeof(esp_image_segment_header_t) + 
                                         sizeof(esp_app_desc_t);
            
            if (image_header.size() >= required_header_size) {
                // 提取应用描述符
                esp_app_desc_t new_app_info;
                size_t offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
                memcpy(&new_app_info, image_header.data() + offset, sizeof(esp_app_desc_t));
                
                // 显示版本信息
                auto current_version = esp_app_get_description()->version;
                ESP_LOGI(TAG, "Current version: %s, New version: %s", 
                         current_version, new_app_info.version);
                
                // 开始OTA写入
                if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle) != ESP_OK) {
                    esp_ota_abort(update_handle);
                    ESP_LOGE(TAG, "Failed to begin OTA");
                    return false;
                }
                
                image_header_checked = true;
                std::string().swap(image_header); // 释放内存
            }
        }
```

#### 2.4 固件数据写入
```cpp
        // 4. 写入固件数据
        auto err = esp_ota_write(update_handle, buffer, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            return false;
        }
        
        // 更新进度统计
        recent_read += ret;
        total_read += ret;
        
        // 每秒计算一次进度和速度
        if (esp_timer_get_time() - last_calc_time >= 1000000 || ret == 0) {
            size_t progress = total_read * 100 / content_length;
            ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %uB/s", 
                     progress, total_read, content_length, recent_read);
            
            // 调用进度回调
            if (upgrade_callback_) {
                upgrade_callback_(progress, recent_read);
            }
            
            last_calc_time = esp_timer_get_time();
            recent_read = 0;
        }
    }
    
    http->Close();
```

#### 2.5 升级完成和验证
```cpp
    // 5. 完成OTA写入
    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
        }
        return false;
    }
    
    // 6. 设置启动分区
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Firmware upgrade successful");
    return true;
}
```

### 3. 进度监控系统

#### 3.1 进度计算
```cpp
// 进度计算公式
size_t progress = total_read * 100 / content_length;

// 速度计算（每秒）
size_t speed = recent_read; // 最近一秒内读取的字节数

// 时间窗口控制
if (esp_timer_get_time() - last_calc_time >= 1000000) { // 1秒
    // 更新进度显示
    // 重置计数器
}
```

#### 3.2 UI进度更新
```cpp
// 应用层进度回调
bool success = ota.StartUpgradeFromUrl(url, 
    [display](int progress, size_t speed) {
        // 使用线程更新UI，避免阻塞下载
        std::thread([display, progress, speed]() {
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d%% %uKB/s", 
                     progress, speed / 1024);
            display->SetChatMessage("system", buffer);
        }).detach();
    });
```

#### 3.3 进度显示格式
- **百分比**: 0% - 100%
- **速度**: KB/s 或 MB/s
- **状态**: "下载中..." / "验证中..." / "安装中..."
- **时间**: 预计剩余时间（可选）

## 分区管理系统

### 1. ESP32 OTA分区架构

#### 1.1 分区表结构
```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x4000,
phy_init, data, phy,     0xd000,  0x1000,
factory,  app,  factory, 0x10000, 0x100000,
ota_0,    app,  ota_0,   0x110000,0x100000,
ota_1,    app,  ota_1,   0x210000,0x100000,
```

#### 1.2 分区状态管理
```cpp
// 获取当前运行分区
auto current_partition = esp_ota_get_running_partition();
ESP_LOGI(TAG, "Current running partition: %s", current_partition->label);

// 获取下一个更新分区
auto update_partition = esp_ota_get_next_update_partition(NULL);
ESP_LOGI(TAG, "Next update partition: %s", update_partition->label);

// 检查分区状态
esp_ota_img_states_t state;
esp_ota_get_state_partition(current_partition, &state);
```

#### 1.3 分区状态类型
```cpp
typedef enum {
    ESP_OTA_IMG_NEW          = 0x0,  /*!< Monitor the first boot. In bootloader this state is changed to ESP_OTA_IMG_PENDING_VERIFY. */
    ESP_OTA_IMG_PENDING_VERIFY = 0x1,  /*!< First boot for this app was. If while the second boot this state is then it will be changed to ABORTED. */
    ESP_OTA_IMG_VALID        = 0x2,  /*!< App was confirmed as workable. App can boot and work without limits. */
    ESP_OTA_IMG_INVALID      = 0x3,  /*!< App was confirmed as non-workable. This app will not selected to boot at all. */
    ESP_OTA_IMG_ABORTED      = 0x4,  /*!< App could not confirm the workable or non-workable. In bootloader IMG_PENDING_VERIFY state will be changed to IMG_ABORTED. This app will not selected to boot at all. */
    ESP_OTA_IMG_UNDEFINED    = 0xFFFFFFFF,  /*!< Undefined. App can boot and work without limits. */
} esp_ota_img_states_t;
```

### 2. 回滚保护机制

#### 2.1 版本标记系统
```cpp
void Ota::MarkCurrentVersionValid() {
    auto partition = esp_ota_get_running_partition();
    
    // 跳过factory分区
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, skipping");
        return;
    }
    
    ESP_LOGI(TAG, "Running partition: %s", partition->label);
    
    // 获取分区状态
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get state of partition");
        return;
    }
    
    // 如果是待验证状态，标记为有效
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking firmware as valid");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}
```

#### 2.2 回滚机制工作流程
1. **升级完成**: 新固件写入备用分区，设置为启动分区
2. **首次启动**: 新固件状态为`ESP_OTA_IMG_PENDING_VERIFY`
3. **验证成功**: 应用调用`MarkCurrentVersionValid()`标记为有效
4. **验证失败**: 看门狗超时或崩溃，bootloader自动回滚

#### 2.3 回滚触发条件
- **启动失败**: 新固件无法正常启动
- **运行异常**: 应用运行时发生严重错误
- **验证超时**: 在规定时间内未标记版本有效
- **手动回滚**: 通过API手动触发回滚

### 3. 分区安全机制

#### 3.1 固件验证
```cpp
// 在esp_ota_end()中自动进行的验证：
// 1. 固件头部校验
// 2. 应用描述符验证
// 3. 固件完整性检查
// 4. 数字签名验证（如果启用）

esp_err_t err = esp_ota_end(update_handle);
if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    ESP_LOGE(TAG, "Image validation failed, image is corrupted");
    return false;
}
```

#### 3.2 分区保护
- **写保护**: 运行中的分区不能被写入
- **完整性检查**: 升级前后的校验和验证
- **原子操作**: 分区切换是原子性的
- **回滚保护**: 自动回滚到稳定版本

## 错误处理与恢复机制

### 1. 网络错误处理

#### 1.1 连接错误
```cpp
// HTTP连接失败处理
if (!http->Open("GET", firmware_url)) {
    ESP_LOGE(TAG, "Failed to open HTTP connection");
    
    // 记录错误
    LogUpgradeError("HTTP_CONNECTION_FAILED", firmware_url);
    
    // 通知用户
    NotifyUpgradeError("网络连接失败，请检查网络设置");
    
    return false;
}
```

#### 2.2 下载中断处理
```cpp
// 数据读取错误处理
int ret = http->Read(buffer, sizeof(buffer));
if (ret < 0) {
    ESP_LOGE(TAG, "Failed to read HTTP data: %s", esp_err_to_name(ret));
    
    // 清理OTA句柄
    if (update_handle) {
        esp_ota_abort(update_handle);
    }
    
    // 恢复系统状态
    RestoreSystemState();
    
    return false;
}
```

#### 1.3 重试机制
```cpp
class UpgradeRetryManager {
private:
    static const int MAX_RETRY_COUNT = 3;
    static const int RETRY_DELAY_MS = 5000;
    
public:
    bool RetryUpgrade(const std::string& url) {
        for (int attempt = 1; attempt <= MAX_RETRY_COUNT; attempt++) {
            ESP_LOGI(TAG, "Upgrade attempt %d/%d", attempt, MAX_RETRY_COUNT);
            
            if (PerformUpgrade(url)) {
                return true;
            }
            
            if (attempt < MAX_RETRY_COUNT) {
                ESP_LOGW(TAG, "Upgrade failed, retrying in %d seconds...", RETRY_DELAY_MS / 1000);
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            }
        }
        
        ESP_LOGE(TAG, "Upgrade failed after %d attempts", MAX_RETRY_COUNT);
        return false;
    }
};
```

### 2. 固件验证错误

#### 2.1 头部验证失败
```cpp
if (!ValidateImageHeader(buffer, ret)) {
    ESP_LOGE(TAG, "Invalid firmware header");
    
    // 详细错误信息
    LogImageHeaderError(buffer, ret);
    
    // 中止升级
    esp_ota_abort(update_handle);
    return false;
}
```

#### 2.2 完整性验证失败
```cpp
esp_err_t err = esp_ota_end(update_handle);
if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    ESP_LOGE(TAG, "Image validation failed, image is corrupted");
    
    // 记录详细错误
    LogValidationError(update_partition);
    
    // 通知用户
    NotifyUpgradeError("固件验证失败，请重新下载");
    
    return false;
}
```

### 3. 系统状态恢复

#### 3.1 升级失败恢复
```cpp
void Application::HandleUpgradeFailure() {
    ESP_LOGE(TAG, "Firmware upgrade failed, restoring system state");
    
    // 1. 恢复音频服务
    if (!audio_service_.IsRunning()) {
        audio_service_.Start();
        ESP_LOGI(TAG, "Audio service restarted");
    }
    
    // 2. 恢复省电模式
    auto& board = Board::GetInstance();
    board.SetPowerSaveMode(true);
    
    // 3. 恢复设备状态
    SetDeviceState(kDeviceStateIdle);
    
    // 4. 显示错误信息
    auto display = board.GetDisplay();
    Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, 
          "circle_xmark", Lang::Sounds::OGG_EXCLAMATION);
    
    // 5. 记录错误日志
    LogSystemEvent("UPGRADE_FAILED", "System state restored");
}
```

#### 3.2 资源清理
```cpp
class UpgradeResourceManager {
public:
    ~UpgradeResourceManager() {
        Cleanup();
    }
    
    void Cleanup() {
        // 关闭HTTP连接
        if (http_client_) {
            http_client_->Close();
            http_client_.reset();
        }
        
        // 中止OTA操作
        if (ota_handle_ != 0) {
            esp_ota_abort(ota_handle_);
            ota_handle_ = 0;
        }
        
        // 释放内存缓冲区
        if (download_buffer_) {
            free(download_buffer_);
            download_buffer_ = nullptr;
        }
        
        ESP_LOGI(TAG, "Upgrade resources cleaned up");
    }
    
private:
    std::unique_ptr<Http> http_client_;
    esp_ota_handle_t ota_handle_ = 0;
    char* download_buffer_ = nullptr;
};
```

## 安全机制

### 1. 固件完整性保护

#### 1.1 数字签名验证（可选）
```cpp
#ifdef CONFIG_SECURE_BOOT_ENABLED
bool ValidateDigitalSignature(const uint8_t* firmware_data, size_t size) {
    // 使用内置公钥验证固件签名
    esp_err_t err = esp_secure_boot_verify_signature(firmware_data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Digital signature verification failed: %s", esp_err_to_name(err));
        return false;
    }
    
    ESP_LOGI(TAG, "Digital signature verified successfully");
    return true;
}
#endif
```

#### 1.2 校验和验证
```cpp
uint32_t CalculateFirmwareChecksum(const uint8_t* data, size_t size) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum = (checksum << 1) ^ data[i];
    }
    return checksum;
}

bool ValidateFirmwareIntegrity(const std::string& expected_checksum) {
    uint32_t calculated_checksum = CalculateFirmwareChecksum(firmware_data, firmware_size);
    uint32_t expected = std::stoul(expected_checksum, nullptr, 16);
    
    if (calculated_checksum != expected) {
        ESP_LOGE(TAG, "Checksum mismatch: calculated=0x%x, expected=0x%x", 
                 calculated_checksum, expected);
        return false;
    }
    
    return true;
}
```

### 2. 网络安全

#### 2.1 HTTPS强制
```cpp
bool ValidateUpgradeUrl(const std::string& url) {
    // 强制使用HTTPS
    if (!url.starts_with("https://")) {
        ESP_LOGE(TAG, "Insecure URL not allowed: %s", url.c_str());
        return false;
    }
    
    // 域名白名单检查
    std::vector<std::string> allowed_domains = {
        "api.tenclass.net",
        "firmware.xiaozhi.ai",
        "update.espressif.com"
    };
    
    for (const auto& domain : allowed_domains) {
        if (url.find(domain) != std::string::npos) {
            return true;
        }
    }
    
    ESP_LOGE(TAG, "Domain not in whitelist: %s", url.c_str());
    return false;
}
```

#### 2.2 证书验证
```cpp
void ConfigureHttpsSecurity(Http* http) {
    // 设置CA证书
    http->SetCACert(ca_cert_pem);
    
    // 启用证书验证
    http->SetVerifyPeer(true);
    http->SetVerifyHost(true);
    
    // 设置超时时间
    http->SetTimeout(30000); // 30秒
    
    ESP_LOGI(TAG, "HTTPS security configured");
}
```

### 3. 访问控制

#### 3.1 设备认证
```cpp
void SetupDeviceAuthentication(Http* http) {
    auto& board = Board::GetInstance();
    
    // 设备ID
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress());
    
    // 客户端ID
    http->SetHeader("Client-Id", board.GetUuid());
    
    // 序列号（如果有）
    if (has_serial_number_) {
        http->SetHeader("Serial-Number", serial_number_);
        
        // HMAC认证
        std::string hmac = CalculateHMAC(serial_number_, challenge_);
        http->SetHeader("Authorization", "HMAC " + hmac);
    }
    
    // 版本信息
    http->SetHeader("Firmware-Version", current_version_);
    http->SetHeader("Hardware-Version", board.GetHardwareVersion());
}
```

#### 3.2 权限验证
```cpp
bool ValidateUpgradePermission(const std::string& device_id, const std::string& firmware_version) {
    // 检查设备是否有升级权限
    if (!IsDeviceAuthorized(device_id)) {
        ESP_LOGE(TAG, "Device not authorized for upgrade: %s", device_id.c_str());
        return false;
    }
    
    // 检查固件版本是否允许
    if (!IsFirmwareVersionAllowed(firmware_version)) {
        ESP_LOGE(TAG, "Firmware version not allowed: %s", firmware_version.c_str());
        return false;
    }
    
    return true;
}
```

## 性能优化

### 1. 下载性能优化

#### 1.1 缓冲区优化
```cpp
class OptimizedDownloader {
private:
    static const size_t OPTIMAL_BUFFER_SIZE = 4096;  // 4KB缓冲区
    static const size_t MAX_CONCURRENT_REQUESTS = 1; // ESP32单线程下载
    
    uint8_t* download_buffer_;
    size_t buffer_size_;
    
public:
    OptimizedDownloader() {
        // 分配DMA兼容的缓冲区
        download_buffer_ = (uint8_t*)heap_caps_malloc(OPTIMAL_BUFFER_SIZE, 
                                                     MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
        buffer_size_ = download_buffer_ ? OPTIMAL_BUFFER_SIZE : 0;
    }
    
    bool DownloadWithOptimization(const std::string& url) {
        if (!download_buffer_) {
            ESP_LOGE(TAG, "Failed to allocate download buffer");
            return false;
        }
        
        // 使用优化后的缓冲区下载
        return PerformOptimizedDownload(url);
    }
};
```

#### 1.2 内存管理优化
```cpp
void OptimizeMemoryForUpgrade() {
    // 释放不必要的内存
    esp_log_level_set("*", ESP_LOG_WARN); // 减少日志输出
    
    // 强制垃圾回收
    heap_caps_malloc_extmem_enable(16 * 1024); // 使用外部内存
    
    // 检查可用内存
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    ESP_LOGI(TAG, "Memory before upgrade: free=%u, min_free=%u", free_heap, min_free_heap);
    
    if (free_heap < 100 * 1024) { // 小于100KB
        ESP_LOGW(TAG, "Low memory warning: %u bytes", free_heap);
    }
}
```

### 2. Flash写入优化

#### 2.1 扇区对齐写入
```cpp
bool OptimizedFlashWrite(esp_ota_handle_t handle, const void* data, size_t size) {
    const size_t FLASH_SECTOR_SIZE = 4096;
    static uint8_t sector_buffer[FLASH_SECTOR_SIZE];
    static size_t buffer_offset = 0;
    
    const uint8_t* src = (const uint8_t*)data;
    size_t remaining = size;
    
    while (remaining > 0) {
        size_t copy_size = std::min(remaining, FLASH_SECTOR_SIZE - buffer_offset);
        memcpy(sector_buffer + buffer_offset, src, copy_size);
        
        buffer_offset += copy_size;
        src += copy_size;
        remaining -= copy_size;
        
        // 缓冲区满时写入Flash
        if (buffer_offset == FLASH_SECTOR_SIZE) {
            esp_err_t err = esp_ota_write(handle, sector_buffer, FLASH_SECTOR_SIZE);
            if (err != ESP_OK) {
                return false;
            }
            buffer_offset = 0;
        }
    }
    
    return true;
}
```

### 3. 网络优化

#### 3.1 TCP参数调优
```cpp
void OptimizeTcpParameters(Http* http) {
    // 设置TCP缓冲区大小
    http->SetSocketOption(SOL_SOCKET, SO_RCVBUF, 8192);
    http->SetSocketOption(SOL_SOCKET, SO_SNDBUF, 8192);
    
    // 设置TCP_NODELAY
    int nodelay = 1;
    http->SetSocketOption(IPPROTO_TCP, TCP_NODELAY, nodelay);
    
    // 设置保活参数
    int keepalive = 1;
    http->SetSocketOption(SOL_SOCKET, SO_KEEPALIVE, keepalive);
    
    ESP_LOGI(TAG, "TCP parameters optimized for firmware download");
}
```

## 监控与调试

### 1. 升级状态监控

#### 1.1 状态跟踪
```cpp
enum class UpgradeState {
    IDLE,
    CHECKING_VERSION,
    DOWNLOADING,
    VALIDATING,
    INSTALLING,
    COMPLETING,
    FAILED,
    SUCCESS
};

class UpgradeMonitor {
private:
    UpgradeState current_state_ = UpgradeState::IDLE;
    uint64_t state_start_time_ = 0;
    std::map<UpgradeState, uint64_t> state_durations_;
    
public:
    void SetState(UpgradeState state) {
        uint64_t current_time = esp_timer_get_time();
        
        if (current_state_ != UpgradeState::IDLE) {
            state_durations_[current_state_] = current_time - state_start_time_;
        }
        
        current_state_ = state;
        state_start_time_ = current_time;
        
        ESP_LOGI(TAG, "Upgrade state changed to: %s", StateToString(state));
    }
    
    void PrintStatistics() {
        ESP_LOGI(TAG, "=== Upgrade Statistics ===");
        for (const auto& [state, duration] : state_durations_) {
            ESP_LOGI(TAG, "%s: %llu ms", StateToString(state), duration / 1000);
        }
    }
};
```

#### 1.2 性能指标
```cpp
struct UpgradeMetrics {
    size_t total_bytes = 0;
    uint64_t download_time_us = 0;
    uint64_t write_time_us = 0;
    uint64_t validation_time_us = 0;
    uint32_t average_speed_bps = 0;
    uint32_t peak_speed_bps = 0;
    uint32_t retry_count = 0;
    
    void CalculateAverageSpeed() {
        if (download_time_us > 0) {
            average_speed_bps = (total_bytes * 1000000) / download_time_us;
        }
    }
    
    void Print() {
        ESP_LOGI(TAG, "Total bytes: %u", total_bytes);
        ESP_LOGI(TAG, "Download time: %llu ms", download_time_us / 1000);
        ESP_LOGI(TAG, "Write time: %llu ms", write_time_us / 1000);
        ESP_LOGI(TAG, "Validation time: %llu ms", validation_time_us / 1000);
        ESP_LOGI(TAG, "Average speed: %u KB/s", average_speed_bps / 1024);
        ESP_LOGI(TAG, "Peak speed: %u KB/s", peak_speed_bps / 1024);
        ESP_LOGI(TAG, "Retry count: %u", retry_count);
    }
};
```

### 2. 错误诊断

#### 2.1 错误分类
```cpp
enum class UpgradeError {
    NONE = 0,
    NETWORK_CONNECTION_FAILED,
    NETWORK_TIMEOUT,
    HTTP_ERROR,
    INVALID_FIRMWARE,
    INSUFFICIENT_SPACE,
    FLASH_WRITE_ERROR,
    VALIDATION_FAILED,
    PARTITION_ERROR,
    OUT_OF_MEMORY,
    UNKNOWN_ERROR
};

class UpgradeErrorHandler {
public:
    static std::string GetErrorDescription(UpgradeError error) {
        switch (error) {
            case UpgradeError::NETWORK_CONNECTION_FAILED:
                return "网络连接失败，请检查WiFi连接";
            case UpgradeError::HTTP_ERROR:
                return "服务器响应错误，请稍后重试";
            case UpgradeError::INVALID_FIRMWARE:
                return "固件文件无效或已损坏";
            case UpgradeError::INSUFFICIENT_SPACE:
                return "存储空间不足，无法完成升级";
            case UpgradeError::FLASH_WRITE_ERROR:
                return "写入Flash失败，设备可能存在硬件问题";
            case UpgradeError::VALIDATION_FAILED:
                return "固件验证失败，请重新下载";
            default:
                return "未知错误";
        }
    }
    
    static void LogError(UpgradeError error, const std::string& details = "") {
        ESP_LOGE(TAG, "Upgrade error: %s (%s)", 
                GetErrorDescription(error).c_str(), details.c_str());
        
        // 记录到NVS用于后续分析
        Settings settings("upgrade_errors", true);
        std::string error_key = "error_" + std::to_string(esp_timer_get_time());
        cJSON* error_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(error_json, "error_code", static_cast<int>(error));
        cJSON_AddStringToObject(error_json, "description", GetErrorDescription(error).c_str());
        cJSON_AddStringToObject(error_json, "details", details.c_str());
        cJSON_AddNumberToObject(error_json, "timestamp", esp_timer_get_time());
        
        char* json_string = cJSON_Print(error_json);
        settings.SetString(error_key, json_string);
        free(json_string);
        cJSON_Delete(error_json);
    }
};
```

### 3. 调试接口

#### 3.1 MCP调试工具
```cpp
void RegisterOtaDebugTools() {
    auto& mcp = McpServer::GetInstance();
    
    // OTA状态查询
    mcp.AddTool("ota.status", "Get OTA status information",
        PropertyList(),
        [](const PropertyList&) -> ReturnValue {
            Ota ota;
            cJSON* status = cJSON_CreateObject();
            
            // 当前版本信息
            auto app_desc = esp_app_get_description();
            cJSON_AddStringToObject(status, "current_version", app_desc->version);
            cJSON_AddStringToObject(status, "compile_time", app_desc->time);
            cJSON_AddStringToObject(status, "compile_date", app_desc->date);
            
            // 分区信息
            auto current_partition = esp_ota_get_running_partition();
            cJSON_AddStringToObject(status, "current_partition", current_partition->label);
            cJSON_AddNumberToObject(status, "partition_address", current_partition->address);
            cJSON_AddNumberToObject(status, "partition_size", current_partition->size);
            
            // 检查更新
            bool has_update = ota.CheckVersion();
            cJSON_AddBoolToObject(status, "update_available", has_update && ota.HasNewVersion());
            if (has_update && ota.HasNewVersion()) {
                cJSON_AddStringToObject(status, "new_version", ota.GetFirmwareVersion().c_str());
                cJSON_AddStringToObject(status, "firmware_url", ota.GetFirmwareUrl().c_str());
            }
            
            return ReturnValue(status);
        });
    
    // 手动升级工具
    mcp.AddTool("ota.upgrade", "Start firmware upgrade",
        PropertyList({Property("url", kPropertyTypeString, true)}),
        [](const PropertyList& props) -> ReturnValue {
            std::string url = props["url"].value<std::string>();
            
            // 验证URL
            if (!ValidateUpgradeUrl(url)) {
                throw std::runtime_error("Invalid upgrade URL");
            }
            
            // 开始升级
            Application& app = Application::GetInstance();
            Ota ota;
            bool success = app.UpgradeFirmware(ota, url);
            
            cJSON* result = cJSON_CreateObject();
            cJSON_AddBoolToObject(result, "success", success);
            cJSON_AddStringToObject(result, "url", url.c_str());
            
            return ReturnValue(result);
        });
    
    // 分区信息工具
    mcp.AddTool("ota.partitions", "Get partition information",
        PropertyList(),
        [](const PropertyList&) -> ReturnValue {
            cJSON* partitions = cJSON_CreateArray();
            
            esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, 
                                                           ESP_PARTITION_SUBTYPE_ANY, NULL);
            while (it != NULL) {
                const esp_partition_t* partition = esp_partition_get(it);
                
                cJSON* part_info = cJSON_CreateObject();
                cJSON_AddStringToObject(part_info, "label", partition->label);
                cJSON_AddStringToObject(part_info, "type", "app");
                cJSON_AddNumberToObject(part_info, "address", partition->address);
                cJSON_AddNumberToObject(part_info, "size", partition->size);
                
                // 获取分区状态
                esp_ota_img_states_t state;
                if (esp_ota_get_state_partition(partition, &state) == ESP_OK) {
                    const char* state_str = "unknown";
                    switch (state) {
                        case ESP_OTA_IMG_NEW: state_str = "new"; break;
                        case ESP_OTA_IMG_PENDING_VERIFY: state_str = "pending_verify"; break;
                        case ESP_OTA_IMG_VALID: state_str = "valid"; break;
                        case ESP_OTA_IMG_INVALID: state_str = "invalid"; break;
                        case ESP_OTA_IMG_ABORTED: state_str = "aborted"; break;
                        case ESP_OTA_IMG_UNDEFINED: state_str = "undefined"; break;
                    }
                    cJSON_AddStringToObject(part_info, "state", state_str);
                }
                
                cJSON_AddItemToArray(partitions, part_info);
                it = esp_partition_next(it);
            }
            esp_partition_iterator_release(it);
            
            return ReturnValue(partitions);
        });
}
```

## 总结

小智AI聊天机器人的OTA固件升级系统是一个完整、安全、高效的远程升级解决方案。该系统具有以下特点：

### 主要优势

1. **智能版本管理**: 语义化版本比较，支持强制更新
2. **安全可靠**: 数字签名验证、HTTPS传输、回滚保护
3. **用户友好**: 实时进度显示、错误提示、状态反馈
4. **高性能**: 优化的下载和写入机制，内存管理
5. **容错性强**: 完善的错误处理和恢复机制
6. **易于调试**: 丰富的调试接口和监控工具

### 技术亮点

1. **双分区设计**: OTA_0 ↔ OTA_1 交替使用，确保升级安全
2. **回滚保护**: 自动回滚到稳定版本，避免设备变砖
3. **流式升级**: 边下载边写入，节省内存空间
4. **进度监控**: 实时进度和速度反馈，提升用户体验
5. **错误恢复**: 多重错误处理机制，确保系统稳定
6. **性能优化**: 缓冲区优化、TCP调优、Flash对齐写入

### 系统价值

1. **降低维护成本**: 远程升级减少现场维护需求
2. **快速问题修复**: 及时推送安全补丁和Bug修复
3. **功能持续改进**: 支持新功能的快速部署
4. **设备生命周期管理**: 长期的固件维护和升级支持
5. **用户体验提升**: 无感知升级，设备功能持续优化

这套OTA系统为ESP32平台的IoT设备提供了一个优秀的固件升级参考实现，具有很高的实用价值和推广意义。通过完善的版本管理、安全机制和用户体验设计，确保了设备能够安全、可靠地进行远程固件更新。
