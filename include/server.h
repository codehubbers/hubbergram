#ifndef SERVER_H
#define SERVER_H

// Prevent Windows socket headers from being included
#define _WINSOCKAPI_
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define NOMINMAX

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <json-c/json.h>
#include <sqlite3.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>

#define PORT SERVER_PORT
#define MAX_CLIENTS MAX_CONNECTIONS
#define TOKEN_SIZE 256
#define HASH_SIZE 65

typedef enum {
    USER_REGULAR = 0,
    USER_ADMIN = 1
} user_role_t;

typedef struct {
    int id;
    char username[50];
    char email[100];
    char password_hash[HASH_SIZE];
    user_role_t role;
    int location_consent;
    double latitude;
    double longitude;
    time_t location_updated;
    int location_duration; // in minutes
} user_t;

typedef struct {
    int id;
    int sender_id;
    int receiver_id;
    int group_id;
    char content[2048];
    char media_path[256];
    time_t timestamp;
    int encrypted;
} message_t;

typedef struct {
    int socket;
    struct sockaddr_in address;
    user_t user;
    char token[TOKEN_SIZE];
    int authenticated;
} client_t;

typedef struct {
    int id;
    char name[100];
    int admin_id;
    time_t created_at;
} group_t;

// Database functions
int init_database(void);
int create_user(const char* username, const char* email, const char* password, user_role_t role);
user_t* authenticate_user(const char* username, const char* password);
int save_message(message_t* msg);
int update_user_location(int user_id, double lat, double lng, int duration);
int get_user_locations(user_t** users, int* count);
user_t* get_user_by_username(const char* username);
user_t* get_user_by_id(int user_id);
int get_user_messages(int user_id, message_t** messages, int* count);

// Server functions
void start_server(void);
void* handle_client(void* arg);
void handle_http_request(client_t* client, const char* request);
void handle_websocket(client_t* client);

// Auth functions
char* generate_token(int user_id);
int verify_token(const char* token, int* user_id);
void hash_password(const char* password, char* hash);

// API endpoints
void api_register(client_t* client, json_object* data);
void api_login(client_t* client, json_object* data);
void api_send_message(client_t* client, json_object* data);
void api_update_location(client_t* client, json_object* data);
void api_get_locations(client_t* client); // Admin only
void api_get_users(client_t* client); // Admin only
void api_get_messages(client_t* client);

// Utility functions
void send_response(int socket, int status, const char* content_type, const char* body);
void send_json_response(int socket, int status, json_object* json);
int is_admin(client_t* client);

#endif