#include "server.h"

void hash_password(const char* password, char* hash) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, password, strlen(password));
    SHA256_Final(digest, &ctx);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&hash[i*2], "%02x", digest[i]);
    }
    hash[64] = '\0';
}

char* generate_token(int user_id) {
    char* token = malloc(TOKEN_SIZE);
    time_t now = time(NULL);
    
    // Simple token: base64(user_id:timestamp:random)
    srand(now);
    int random = rand();
    
    char temp[128];
    snprintf(temp, sizeof(temp), "%d:%ld:%d", user_id, now, random);
    
    // Simple base64-like encoding (for demo purposes)
    for (int i = 0; i < strlen(temp) && i < TOKEN_SIZE-1; i++) {
        token[i] = temp[i] + 1; // Simple shift
    }
    token[strlen(temp)] = '\0';
    
    return token;
}

int verify_token(const char* token, int* user_id) {
    if (!token || strlen(token) == 0) return 0;
    
    // Decode token (reverse of generate_token)
    char temp[128];
    for (int i = 0; i < strlen(token) && i < 127; i++) {
        temp[i] = token[i] - 1; // Reverse shift
    }
    temp[strlen(token)] = '\0';
    
    // Parse user_id:timestamp:random
    char* saveptr;
    char* user_str = strtok_r(temp, ":", &saveptr);
    char* time_str = strtok_r(NULL, ":", &saveptr);
    
    if (!user_str || !time_str) return 0;
    
    *user_id = atoi(user_str);
    time_t token_time = atol(time_str);
    time_t now = time(NULL);
    
    // Token expires after 24 hours
    return (now - token_time) < 86400;
}