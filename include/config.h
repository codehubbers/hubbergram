#ifndef CONFIG_H
#define CONFIG_H

// Server Configuration
#define SERVER_PORT 8080
#define MAX_CONNECTIONS 100
#define BUFFER_SIZE 4096
#define MAX_MESSAGE_SIZE 2048
#define MAX_MEDIA_SIZE (2 * 1024 * 1024 * 1024) // 2GB

// Security Configuration
#define TOKEN_EXPIRY_HOURS 24
#define MAX_LOGIN_ATTEMPTS 5
#define PASSWORD_MIN_LENGTH 6

// Location Service Configuration
#define DEFAULT_LOCATION_DURATION 60 // minutes
#define MIN_LOCATION_DURATION 15     // minutes
#define MAX_LOCATION_DURATION 480    // minutes (8 hours)

// Database Security
#define DB_ENCRYPTION 1
#define DB_PASSWORD_SIZE 32

// Database Configuration
#define DB_FILE "telegram_clone.db"
#define DB_BACKUP_INTERVAL 3600 // seconds

// Privacy Settings
#define REQUIRE_LOCATION_CONSENT 1
#define AUTO_DELETE_EXPIRED_LOCATIONS 1
#define LOG_ADMIN_ACTIONS 1

// Feature Flags
#define ENABLE_FILE_UPLOAD 0
#define ENABLE_GROUP_CHAT 1
#define ENABLE_MESSAGE_ENCRYPTION 0
#define ENABLE_WEBSOCKET 0

// Logging Configuration
#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_ERROR 3

#define CURRENT_LOG_LEVEL LOG_LEVEL_INFO

// Rate Limiting
#define RATE_LIMIT_REQUESTS_PER_MINUTE 60
#define RATE_LIMIT_MESSAGES_PER_MINUTE 30

#endif