#ifndef DB_SECURITY_H
#define DB_SECURITY_H

#include <string.h>

static void deobfuscate_key(unsigned char *key, int key_len, const unsigned char *obfus_keys, int layers) {
    for(int layer = layers-1; layer >= 0; layer--) {
        for(int i = 0; i < key_len; i++) {
            key[i] ^= obfus_keys[layer] + i;
        }
    }
}

static void get_db_password(char *password, int max_len) {
    #include "../build/db_key.h"
    
    unsigned char clean_key[64];
    memcpy(clean_key, DB_KEY, 64);
    deobfuscate_key(clean_key, 64, OBFUS_KEYS, 8);
    
    int len = 0;
    for(int i = 0; i < 64 && len < max_len-1; i++) {
        len += snprintf(password + len, max_len - len, "%02x", clean_key[i]);
    }
    password[len] = '\0';
}

#endif