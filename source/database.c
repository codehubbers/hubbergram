#include "server.h"
#include "db_security.h"

static sqlite3* db = NULL;

int init_database(void) {
    char db_password[256];
    get_db_password(db_password, sizeof(db_password));
    
    int rc = sqlite3_open("telegram_clone.db", &db);
    if (rc == SQLITE_OK) {
        char pragma_cmd[512];
        snprintf(pragma_cmd, sizeof(pragma_cmd), "PRAGMA key = '%s';", db_password);
        sqlite3_exec(db, pragma_cmd, NULL, NULL, NULL);
        memset(db_password, 0, sizeof(db_password));
        memset(pragma_cmd, 0, sizeof(pragma_cmd));
    }
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    // Create users table
    const char* create_users = 
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "username TEXT UNIQUE NOT NULL,"
        "email TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "role INTEGER DEFAULT 0,"
        "location_consent INTEGER DEFAULT 0,"
        "latitude REAL DEFAULT 0.0,"
        "longitude REAL DEFAULT 0.0,"
        "location_updated INTEGER DEFAULT 0,"
        "location_duration INTEGER DEFAULT 0"
        ");";

    // Create messages table
    const char* create_messages = 
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "sender_id INTEGER NOT NULL,"
        "receiver_id INTEGER,"
        "group_id INTEGER,"
        "content TEXT NOT NULL,"
        "media_path TEXT,"
        "timestamp INTEGER NOT NULL,"
        "encrypted INTEGER DEFAULT 0,"
        "FOREIGN KEY(sender_id) REFERENCES users(id)"
        ");";

    // Create groups table
    const char* create_groups = 
        "CREATE TABLE IF NOT EXISTS groups ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "admin_id INTEGER NOT NULL,"
        "created_at INTEGER NOT NULL,"
        "FOREIGN KEY(admin_id) REFERENCES users(id)"
        ");";

    char* err_msg = 0;
    rc = sqlite3_exec(db, create_users, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    rc = sqlite3_exec(db, create_messages, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    rc = sqlite3_exec(db, create_groups, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    return 0;
}

int create_user(const char* username, const char* email, const char* password, user_role_t role) {
    char hash[HASH_SIZE];
    hash_password(password, hash);

    const char* sql = "INSERT INTO users (username, email, password_hash, role) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, email, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, hash, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, role);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? sqlite3_last_insert_rowid(db) : -1;
}

user_t* authenticate_user(const char* username, const char* password) {
    char hash[HASH_SIZE];
    hash_password(password, hash);

    const char* sql = "SELECT * FROM users WHERE username = ? AND password_hash = ?;";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    user_t* user = malloc(sizeof(user_t));
    user->id = sqlite3_column_int(stmt, 0);
    strcpy(user->username, (char*)sqlite3_column_text(stmt, 1));
    strcpy(user->email, (char*)sqlite3_column_text(stmt, 2));
    strcpy(user->password_hash, (char*)sqlite3_column_text(stmt, 3));
    user->role = sqlite3_column_int(stmt, 4);
    user->location_consent = sqlite3_column_int(stmt, 5);
    user->latitude = sqlite3_column_double(stmt, 6);
    user->longitude = sqlite3_column_double(stmt, 7);
    user->location_updated = sqlite3_column_int64(stmt, 8);
    user->location_duration = sqlite3_column_int(stmt, 9);

    sqlite3_finalize(stmt);
    return user;
}

int save_message(message_t* msg) {
    const char* sql = "INSERT INTO messages (sender_id, receiver_id, group_id, content, media_path, timestamp, encrypted) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, msg->sender_id);
    sqlite3_bind_int(stmt, 2, msg->receiver_id);
    sqlite3_bind_int(stmt, 3, msg->group_id);
    sqlite3_bind_text(stmt, 4, msg->content, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, msg->media_path, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, msg->timestamp);
    sqlite3_bind_int(stmt, 7, msg->encrypted);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? sqlite3_last_insert_rowid(db) : -1;
}

int update_user_location(int user_id, double lat, double lng, int duration) {
    const char* sql = "UPDATE users SET latitude = ?, longitude = ?, location_updated = ?, location_duration = ?, location_consent = 1 WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_double(stmt, 1, lat);
    sqlite3_bind_double(stmt, 2, lng);
    sqlite3_bind_int64(stmt, 3, time(NULL));
    sqlite3_bind_int(stmt, 4, duration);
    sqlite3_bind_int(stmt, 5, user_id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int get_user_locations(user_t** users, int* count) {
    time_t now = time(NULL);
    const char* sql = "SELECT * FROM users WHERE location_consent = 1 AND (location_updated + location_duration * 60) > ?;";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, now);

    *count = 0;
    *users = NULL;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        *users = realloc(*users, sizeof(user_t) * (*count + 1));
        user_t* user = &(*users)[*count];
        
        user->id = sqlite3_column_int(stmt, 0);
        strcpy(user->username, (char*)sqlite3_column_text(stmt, 1));
        strcpy(user->email, (char*)sqlite3_column_text(stmt, 2));
        user->role = sqlite3_column_int(stmt, 4);
        user->location_consent = sqlite3_column_int(stmt, 5);
        user->latitude = sqlite3_column_double(stmt, 6);
        user->longitude = sqlite3_column_double(stmt, 7);
        user->location_updated = sqlite3_column_int64(stmt, 8);
        user->location_duration = sqlite3_column_int(stmt, 9);
        
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

user_t* get_user_by_username(const char* username) {
    const char* sql = "SELECT * FROM users WHERE username = ?;";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    user_t* user = malloc(sizeof(user_t));
    user->id = sqlite3_column_int(stmt, 0);
    strcpy(user->username, (char*)sqlite3_column_text(stmt, 1));
    strcpy(user->email, (char*)sqlite3_column_text(stmt, 2));
    strcpy(user->password_hash, (char*)sqlite3_column_text(stmt, 3));
    user->role = sqlite3_column_int(stmt, 4);
    user->location_consent = sqlite3_column_int(stmt, 5);
    user->latitude = sqlite3_column_double(stmt, 6);
    user->longitude = sqlite3_column_double(stmt, 7);
    user->location_updated = sqlite3_column_int64(stmt, 8);
    user->location_duration = sqlite3_column_int(stmt, 9);

    sqlite3_finalize(stmt);
    return user;
}

user_t* get_user_by_id(int user_id) {
    const char* sql = "SELECT * FROM users WHERE id = ?;";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    sqlite3_bind_int(stmt, 1, user_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return NULL;
    }

    user_t* user = malloc(sizeof(user_t));
    user->id = sqlite3_column_int(stmt, 0);
    strcpy(user->username, (char*)sqlite3_column_text(stmt, 1));
    strcpy(user->email, (char*)sqlite3_column_text(stmt, 2));
    strcpy(user->password_hash, (char*)sqlite3_column_text(stmt, 3));
    user->role = sqlite3_column_int(stmt, 4);
    user->location_consent = sqlite3_column_int(stmt, 5);
    user->latitude = sqlite3_column_double(stmt, 6);
    user->longitude = sqlite3_column_double(stmt, 7);
    user->location_updated = sqlite3_column_int64(stmt, 8);
    user->location_duration = sqlite3_column_int(stmt, 9);

    sqlite3_finalize(stmt);
    return user;
}

int get_user_messages(int user_id, message_t** messages, int* count) {
    const char* sql = "SELECT * FROM messages WHERE receiver_id = ? OR sender_id = ? ORDER BY timestamp DESC LIMIT 50;";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_int(stmt, 2, user_id);

    *count = 0;
    *messages = NULL;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        *messages = realloc(*messages, sizeof(message_t) * (*count + 1));
        message_t* msg = &(*messages)[*count];
        
        msg->id = sqlite3_column_int(stmt, 0);
        msg->sender_id = sqlite3_column_int(stmt, 1);
        msg->receiver_id = sqlite3_column_int(stmt, 2);
        msg->group_id = sqlite3_column_int(stmt, 3);
        strcpy(msg->content, (char*)sqlite3_column_text(stmt, 4));
        if (sqlite3_column_text(stmt, 5)) {
            strcpy(msg->media_path, (char*)sqlite3_column_text(stmt, 5));
        }
        msg->timestamp = sqlite3_column_int64(stmt, 6);
        msg->encrypted = sqlite3_column_int(stmt, 7);
        
        (*count)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}