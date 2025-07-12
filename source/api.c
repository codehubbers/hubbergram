#include "server.h"

void api_register(client_t* client, json_object* data) {
    json_object* username_obj, *email_obj, *password_obj, *role_obj;
    
    if (!json_object_object_get_ex(data, "username", &username_obj) ||
        !json_object_object_get_ex(data, "email", &email_obj) ||
        !json_object_object_get_ex(data, "password", &password_obj)) {
        send_response(client->socket, 400, "application/json", "{\"error\":\"Missing required fields\"}");
        return;
    }

    const char* username = json_object_get_string(username_obj);
    const char* email = json_object_get_string(email_obj);
    const char* password = json_object_get_string(password_obj);
    
    user_role_t role = USER_REGULAR;
    if (json_object_object_get_ex(data, "role", &role_obj)) {
        role = json_object_get_int(role_obj);
    }

    int user_id = create_user(username, email, password, role);
    if (user_id < 0) {
        send_response(client->socket, 400, "application/json", "{\"error\":\"User creation failed\"}");
        return;
    }

    json_object* response = json_object_new_object();
    json_object* success = json_object_new_boolean(1);
    json_object* id = json_object_new_int(user_id);
    
    json_object_object_add(response, "success", success);
    json_object_object_add(response, "user_id", id);
    
    send_json_response(client->socket, 201, response);
    json_object_put(response);
}

void api_login(client_t* client, json_object* data) {
    json_object* username_obj, *password_obj;
    
    if (!json_object_object_get_ex(data, "username", &username_obj) ||
        !json_object_object_get_ex(data, "password", &password_obj)) {
        send_response(client->socket, 400, "application/json", "{\"error\":\"Missing credentials\"}");
        return;
    }

    const char* username = json_object_get_string(username_obj);
    const char* password = json_object_get_string(password_obj);

    user_t* user = authenticate_user(username, password);
    if (!user) {
        send_response(client->socket, 401, "application/json", "{\"error\":\"Invalid credentials\"}");
        return;
    }

    char* token = generate_token(user->id);
    client->user = *user;
    client->authenticated = 1;
    strcpy(client->token, token);

    json_object* response = json_object_new_object();
    json_object* success = json_object_new_boolean(1);
    json_object* token_obj = json_object_new_string(token);
    json_object* role_obj = json_object_new_int(user->role);
    
    json_object_object_add(response, "success", success);
    json_object_object_add(response, "token", token_obj);
    json_object_object_add(response, "role", role_obj);
    
    send_json_response(client->socket, 200, response);
    json_object_put(response);
    free(user);
    free(token);
}

void api_send_message(client_t* client, json_object* data) {
    if (!client->authenticated) {
        send_response(client->socket, 401, "application/json", "{\"error\":\"Not authenticated\"}");
        return;
    }

    json_object* content_obj, *target_obj;
    
    if (!json_object_object_get_ex(data, "content", &content_obj)) {
        send_response(client->socket, 400, "application/json", "{\"error\":\"Missing content\"}");
        return;
    }

    message_t msg = {0};
    msg.sender_id = client->user.id;
    strcpy(msg.content, json_object_get_string(content_obj));
    msg.timestamp = time(NULL);

    if (json_object_object_get_ex(data, "target_username", &target_obj)) {
        const char* target_username = json_object_get_string(target_obj);
        user_t* target_user = get_user_by_username(target_username);
        if (target_user) {
            msg.receiver_id = target_user->id;
            free(target_user);
        }
    }

    int msg_id = save_message(&msg);
    if (msg_id < 0) {
        send_response(client->socket, 500, "application/json", "{\"error\":\"Failed to save message\"}");
        return;
    }

    json_object* response = json_object_new_object();
    json_object* success = json_object_new_boolean(1);
    json_object* id = json_object_new_int(msg_id);
    
    json_object_object_add(response, "success", success);
    json_object_object_add(response, "message_id", id);
    
    send_json_response(client->socket, 201, response);
    json_object_put(response);
}

void api_update_location(client_t* client, json_object* data) {
    if (!client->authenticated) {
        send_response(client->socket, 401, "application/json", "{\"error\":\"Not authenticated\"}");
        return;
    }

    json_object* lat_obj, *lng_obj, *duration_obj, *consent_obj;
    
    if (!json_object_object_get_ex(data, "latitude", &lat_obj) ||
        !json_object_object_get_ex(data, "longitude", &lng_obj) ||
        !json_object_object_get_ex(data, "consent", &consent_obj)) {
        send_response(client->socket, 400, "application/json", "{\"error\":\"Missing location data or consent\"}");
        return;
    }

    if (!json_object_get_boolean(consent_obj)) {
        send_response(client->socket, 400, "application/json", "{\"error\":\"Location sharing requires explicit consent\"}");
        return;
    }

    double lat = json_object_get_double(lat_obj);
    double lng = json_object_get_double(lng_obj);
    int duration = 60; // Default 1 hour

    if (json_object_object_get_ex(data, "duration", &duration_obj)) {
        duration = json_object_get_int(duration_obj);
    }

    if (update_user_location(client->user.id, lat, lng, duration) < 0) {
        send_response(client->socket, 500, "application/json", "{\"error\":\"Failed to update location\"}");
        return;
    }

    json_object* response = json_object_new_object();
    json_object* success = json_object_new_boolean(1);
    json_object* msg = json_object_new_string("Location updated successfully. Your location will be shared for the specified duration.");
    
    json_object_object_add(response, "success", success);
    json_object_object_add(response, "message", msg);
    
    send_json_response(client->socket, 200, response);
    json_object_put(response);
}

void api_get_locations(client_t* client) {
    if (!client->authenticated || client->user.role != USER_ADMIN) {
        send_response(client->socket, 403, "application/json", "{\"error\":\"Admin access required\"}");
        return;
    }

    user_t* users;
    int count;
    
    if (get_user_locations(&users, &count) < 0) {
        send_response(client->socket, 500, "application/json", "{\"error\":\"Failed to retrieve locations\"}");
        return;
    }

    json_object* response = json_object_new_object();
    json_object* locations = json_object_new_array();

    for (int i = 0; i < count; i++) {
        json_object* user_loc = json_object_new_object();
        json_object* username = json_object_new_string(users[i].username);
        json_object* lat = json_object_new_double(users[i].latitude);
        json_object* lng = json_object_new_double(users[i].longitude);
        json_object* updated = json_object_new_int64(users[i].location_updated);
        
        json_object_object_add(user_loc, "username", username);
        json_object_object_add(user_loc, "latitude", lat);
        json_object_object_add(user_loc, "longitude", lng);
        json_object_object_add(user_loc, "last_updated", updated);
        
        json_object_array_add(locations, user_loc);
    }

    json_object_object_add(response, "locations", locations);
    send_json_response(client->socket, 200, response);
    
    json_object_put(response);
    if (users) free(users);
}

void api_get_users(client_t* client) {
    if (!client->authenticated || client->user.role != USER_ADMIN) {
        send_response(client->socket, 403, "application/json", "{\"error\":\"Admin access required\"}");
        return;
    }

    send_response(client->socket, 200, "application/json", "{\"message\":\"User list endpoint - implementation depends on requirements\"}");
}

void api_get_messages(client_t* client) {
    if (!client->authenticated) {
        send_response(client->socket, 401, "application/json", "{\"error\":\"Not authenticated\"}");
        return;
    }

    message_t* messages;
    int count;
    
    if (get_user_messages(client->user.id, &messages, &count) < 0) {
        send_response(client->socket, 500, "application/json", "{\"error\":\"Failed to retrieve messages\"}");
        return;
    }

    json_object* response = json_object_new_object();
    json_object* msg_array = json_object_new_array();

    for (int i = 0; i < count; i++) {
        json_object* msg_obj = json_object_new_object();
        json_object* sender = json_object_new_string("Unknown");
        json_object* content = json_object_new_string(messages[i].content);
        json_object* timestamp = json_object_new_int64(messages[i].timestamp);
        
        user_t* sender_user = get_user_by_id(messages[i].sender_id);
        if (sender_user) {
            json_object_put(sender);
            sender = json_object_new_string(sender_user->username);
            free(sender_user);
        }
        
        json_object_object_add(msg_obj, "sender", sender);
        json_object_object_add(msg_obj, "content", content);
        json_object_object_add(msg_obj, "timestamp", timestamp);
        
        json_object_array_add(msg_array, msg_obj);
    }

    json_object_object_add(response, "messages", msg_array);
    send_json_response(client->socket, 200, response);
    
    json_object_put(response);
    if (messages) free(messages);
}