#include "server.h"

static client_t clients[MAX_CLIENTS];
static int client_count = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_response(int socket, int status, const char* content_type, const char* body) {
    char response[BUFFER_SIZE];
    const char* status_text = (status == 200) ? "OK" : 
                             (status == 201) ? "Created" :
                             (status == 400) ? "Bad Request" :
                             (status == 401) ? "Unauthorized" :
                             (status == 403) ? "Forbidden" :
                             (status == 404) ? "Not Found" : "Internal Server Error";

    snprintf(response, sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
        "\r\n%s",
        status, status_text, content_type, strlen(body), body);

    send(socket, response, strlen(response), 0);
}

void send_json_response(int socket, int status, json_object* json) {
    const char* json_string = json_object_to_json_string(json);
    send_response(socket, status, "application/json", json_string);
}

int is_admin(client_t* client) {
    return client->authenticated && client->user.role == USER_ADMIN;
}

void handle_http_request(client_t* client, const char* request) {
    char method[16], path[256], version[16];
    sscanf(request, "%s %s %s", method, path, version);
    
    // Extract Authorization header
    char* auth_header = strstr(request, "Authorization: Bearer ");
    if (auth_header) {
        auth_header += 22; // Skip "Authorization: Bearer "
        char* end = strstr(auth_header, "\r\n");
        if (end) {
            int token_len = end - auth_header;
            if (token_len < sizeof(client->token)) {
                strncpy(client->token, auth_header, token_len);
                client->token[token_len] = '\0';
                int user_id;
                if (verify_token(client->token, &user_id) == 1) {
                    user_t* user = get_user_by_id(user_id);
                    if (user) {
                        client->authenticated = 1;
                        client->user = *user;
                        free(user);
                    }
                }
            }
        }
    }

    // Handle CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client->socket, 200, "text/plain", "");
        return;
    }

    // API-only server - no static files
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        send_response(client->socket, 200, "application/json", "{\"message\":\"Telegram Clone API Server\",\"version\":\"1.0\"}");
        return;
    }

    // API endpoints
    if (strcmp(method, "POST") == 0) {
        // Extract JSON body
        char* body = strstr(request, "\r\n\r\n");
        json_object* json = NULL;
        
        if (body) {
            body += 4; // Skip \r\n\r\n
            if (strlen(body) > 0) {
                json = json_tokener_parse(body);
            }
        }
        
        if (!json) {
            json = json_object_new_object();
        }

        if (strcmp(path, "/api/register") == 0) {
            api_register(client, json);
        } else if (strcmp(path, "/api/login") == 0) {
            api_login(client, json);
        } else if (strcmp(path, "/api/message") == 0) {
            api_send_message(client, json);
        } else if (strcmp(path, "/api/location") == 0) {
            api_update_location(client, json);
        } else {
            send_response(client->socket, 404, "application/json", "{\"error\":\"Endpoint not found\"}");
        }

        json_object_put(json);
    } else if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/api/locations") == 0) {
            api_get_locations(client);
        } else if (strcmp(path, "/api/users") == 0) {
            api_get_users(client);
        } else if (strcmp(path, "/api/messages") == 0) {
            api_get_messages(client);
        } else {
            send_response(client->socket, 404, "application/json", "{\"error\":\"Endpoint not found\"}");
        }
    } else {
        send_response(client->socket, 405, "text/plain", "Method not allowed");
    }
}

void* handle_client(void* arg) {
    client_t* client = (client_t*)arg;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        int bytes = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        
        buffer[bytes] = '\0';
        
        // Check if it's HTTP request
        if (strncmp(buffer, "GET", 3) == 0 || strncmp(buffer, "POST", 4) == 0 || 
            strncmp(buffer, "OPTIONS", 7) == 0) {
            handle_http_request(client, buffer);
        }
        // WebSocket handling would go here for real-time messaging
    }

    close(client->socket);
    
    // Remove client from list
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client->socket) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    return NULL;
}

void start_server(void) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Telegram Clone Server running on port %d\n", PORT);
    printf("Access the web interface at http://localhost:%d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) continue;

        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            clients[client_count].socket = client_socket;
            clients[client_count].address = client_addr;
            clients[client_count].authenticated = 0;
            memset(&clients[client_count].user, 0, sizeof(user_t));
            
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, &clients[client_count]);
            pthread_detach(thread);
            
            client_count++;
        } else {
            close(client_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    close(server_socket);
}

int main(void) {
    if (init_database() < 0) {
        fprintf(stderr, "Database initialization failed\n");
        return 1;
    }

    // Create default admin user
    create_user("admin", "admin@telegram.local", "admin123", USER_ADMIN);
    
    start_server();
    return 0;
}