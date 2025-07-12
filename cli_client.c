#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX_RESPONSE 4096
#define MAX_INPUT 512
#define SERVER_URL "http://localhost:8080"

typedef struct {
    char* data;
    size_t size;
} response_t;

static char auth_token[256] = {0};
static int is_admin = 0;

int make_request(const char* endpoint, const char* method, const char* json_data, response_t* response) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -1;
    }

    char request[2048];
    int content_len = json_data ? strlen(json_data) : 0;
    
    if (strlen(auth_token) > 0) {
        snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Type: application/json\r\n"
            "Authorization: Bearer %s\r\n"
            "Content-Length: %d\r\n"
            "\r\n%s",
            method, endpoint, auth_token, content_len,
            json_data ? json_data : "");
    } else {
        snprintf(request, sizeof(request),
            "%s %s HTTP/1.1\r\n"
            "Host: localhost:8080\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "\r\n%s",
            method, endpoint, content_len,
            json_data ? json_data : "");
    }

    send(sock, request, strlen(request), 0);
    
    response->data = malloc(4096);
    response->size = recv(sock, response->data, 4095, 0);
    response->data[response->size] = 0;
    
    char* body = strstr(response->data, "\r\n\r\n");
    if (body) {
        body += 4;
        memmove(response->data, body, strlen(body) + 1);
        response->size = strlen(response->data);
    }
    
    close(sock);
    return 0;
}

void register_user() {
    char username[64], email[128], password[64];
    printf("Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    printf("Email: ");
    fgets(email, sizeof(email), stdin);
    email[strcspn(email, "\n")] = 0;

    printf("Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    json_object* json = json_object_new_object();
    json_object_object_add(json, "username", json_object_new_string(username));
    json_object_object_add(json, "email", json_object_new_string(email));
    json_object_object_add(json, "password", json_object_new_string(password));

    response_t response = {0};
    if (make_request("/api/register", "POST", json_object_to_json_string(json), &response) == 0) {
        json_object* resp_json = json_tokener_parse(response.data);
        json_object* success;
        if (json_object_object_get_ex(resp_json, "success", &success) && json_object_get_boolean(success)) {
            printf("Registration successful!\n");
        } else {
            json_object* error;
            if (json_object_object_get_ex(resp_json, "error", &error)) {
                printf("Error: %s\n", json_object_get_string(error));
            }
        }
        json_object_put(resp_json);
    } else {
        printf("Request failed\n");
    }

    json_object_put(json);
    if (response.data) free(response.data);
}

void login() {
    char username[64], password[64];
    printf("Username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    printf("Password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    json_object* json = json_object_new_object();
    json_object_object_add(json, "username", json_object_new_string(username));
    json_object_object_add(json, "password", json_object_new_string(password));

    response_t response = {0};
    if (make_request("/api/login", "POST", json_object_to_json_string(json), &response) == 0) {
        json_object* resp_json = json_tokener_parse(response.data);
        json_object* success, *token, *role;
        
        if (json_object_object_get_ex(resp_json, "success", &success) && json_object_get_boolean(success)) {
            if (json_object_object_get_ex(resp_json, "token", &token)) {
                strcpy(auth_token, json_object_get_string(token));
                printf("Token stored: %.20s...\n", auth_token);
            }
            if (json_object_object_get_ex(resp_json, "role", &role)) {
                is_admin = json_object_get_int(role) == 1;
            }
            printf("Login successful! %s\n", is_admin ? "(Admin)" : "");
        } else {
            json_object* error;
            if (json_object_object_get_ex(resp_json, "error", &error)) {
                printf("Error: %s\n", json_object_get_string(error));
            }
        }
        json_object_put(resp_json);
    } else {
        printf("Request failed\n");
    }

    json_object_put(json);
    if (response.data) free(response.data);
}

void send_message() {
    if (strlen(auth_token) == 0) {
        printf("Please login first\n");
        return;
    }
    printf("Using token: %.20s...\n", auth_token);

    char target_user[64], content[512];
    printf("Target username: ");
    fgets(target_user, sizeof(target_user), stdin);
    target_user[strcspn(target_user, "\n")] = 0;

    printf("Message: ");
    fgets(content, sizeof(content), stdin);
    content[strcspn(content, "\n")] = 0;

    json_object* json = json_object_new_object();
    json_object_object_add(json, "content", json_object_new_string(content));
    json_object_object_add(json, "target_username", json_object_new_string(target_user));

    response_t response = {0};
    if (make_request("/api/message", "POST", json_object_to_json_string(json), &response) == 0) {
        json_object* resp_json = json_tokener_parse(response.data);
        json_object* success;
        if (json_object_object_get_ex(resp_json, "success", &success) && json_object_get_boolean(success)) {
            printf("Message sent to %s!\n", target_user);
        } else {
            json_object* error;
            if (json_object_object_get_ex(resp_json, "error", &error)) {
                printf("Error: %s\n", json_object_get_string(error));
            }
        }
        json_object_put(resp_json);
    } else {
        printf("Request failed\n");
    }

    json_object_put(json);
    if (response.data) free(response.data);
}

void view_locations() {
    if (!is_admin) {
        printf("Admin access required\n");
        return;
    }

    response_t response = {0};
    if (make_request("/api/locations", "GET", NULL, &response) == 0) {
        json_object* resp_json = json_tokener_parse(response.data);
        json_object* locations;
        
        if (json_object_object_get_ex(resp_json, "locations", &locations)) {
            int count = json_object_array_length(locations);
            printf("\nUser Locations (%d):\n", count);
            printf("%-15s %-12s %-12s %s\n", "Username", "Latitude", "Longitude", "Last Updated");
            printf("--------------------------------------------------------\n");
            
            for (int i = 0; i < count; i++) {
                json_object* loc = json_object_array_get_idx(locations, i);
                json_object* username, *lat, *lng, *updated;
                
                json_object_object_get_ex(loc, "username", &username);
                json_object_object_get_ex(loc, "latitude", &lat);
                json_object_object_get_ex(loc, "longitude", &lng);
                json_object_object_get_ex(loc, "last_updated", &updated);
                
                printf("%-15s %-12.6f %-12.6f %lld\n",
                    json_object_get_string(username),
                    json_object_get_double(lat),
                    json_object_get_double(lng),
                    (long long)json_object_get_int64(updated));
            }
        }
        json_object_put(resp_json);
    } else {
        printf("Request failed\n");
    }

    if (response.data) free(response.data);
}

void check_notifications() {
    if (strlen(auth_token) == 0) return;

    response_t response = {0};
    if (make_request("/api/messages", "GET", NULL, &response) == 0) {
        json_object* resp_json = json_tokener_parse(response.data);
        json_object* messages;
        if (json_object_object_get_ex(resp_json, "messages", &messages)) {
            int count = json_object_array_length(messages);
            if (count > 0) {
                printf("\n[%d new message(s)]\n", count);
            }
        }
        json_object_put(resp_json);
    }
    if (response.data) free(response.data);
}

void view_messages() {
    if (strlen(auth_token) == 0) {
        printf("Please login first\n");
        return;
    }

    response_t response = {0};
    if (make_request("/api/messages", "GET", NULL, &response) == 0) {
        json_object* resp_json = json_tokener_parse(response.data);
        json_object* messages;
        
        if (json_object_object_get_ex(resp_json, "messages", &messages)) {
            int count = json_object_array_length(messages);
            printf("\nMessages (%d):\n", count);
            printf("%-15s %-50s %s\n", "From", "Content", "Time");
            printf("------------------------------------------------------------------------\n");
            
            for (int i = 0; i < count; i++) {
                json_object* msg = json_object_array_get_idx(messages, i);
                json_object* sender, *content, *timestamp;
                
                json_object_object_get_ex(msg, "sender", &sender);
                json_object_object_get_ex(msg, "content", &content);
                json_object_object_get_ex(msg, "timestamp", &timestamp);
                
                printf("%-15s %-50s %lld\n",
                    json_object_get_string(sender),
                    json_object_get_string(content),
                    (long long)json_object_get_int64(timestamp));
            }
        } else {
            printf("No messages\n");
        }
        json_object_put(resp_json);
    } else {
        printf("Request failed\n");
    }

    if (response.data) free(response.data);
}

void show_menu() {
    check_notifications();
    printf("\n=== Telegram Clone CLI ===\n");
    if (strlen(auth_token) == 0) {
        printf("1. Register\n");
        printf("2. Login\n");
        printf("0. Exit\n");
    } else {
        printf("1. Send Message\n");
        printf("2. View Messages\n");
        if (is_admin) printf("3. View Locations (Admin)\n");
        printf("9. Logout\n");
        printf("0. Exit\n");
    }
    printf("Choice: ");
}

int main() {
    int choice;
    while (1) {
        show_menu();
        scanf("%d", &choice);
        getchar();

        if (strlen(auth_token) == 0) {
            switch (choice) {
                case 1: register_user(); break;
                case 2: login(); break;
                case 0: return 0;
                default: printf("Invalid choice\n");
            }
        } else {
            switch (choice) {
                case 1: send_message(); break;
                case 2: view_messages(); break;
                case 3: if (is_admin) view_locations(); break;
                case 9: 
                    memset(auth_token, 0, sizeof(auth_token));
                    is_admin = 0;
                    printf("Logged out\n");
                    break;
                case 0: return 0;
                default: printf("Invalid choice\n");
            }
        }
    }
}