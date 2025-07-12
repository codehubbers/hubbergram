#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#define KEY_LEN 64
#define OBFUS_LAYERS 8

static void gen_random_bytes(unsigned char *buf, int len) {
#ifdef _WIN32
    srand(time(NULL) ^ GetCurrentProcessId());
#else
    srand(time(NULL) ^ getpid());
#endif
    for(int i = 0; i < len; i++) {
        buf[i] = rand() % 256;
    }
}

static void xor_obfuscate(unsigned char *data, int len, unsigned char key) {
    for(int i = 0; i < len; i++) {
        data[i] ^= key + i;
    }
}

int main() {
    unsigned char key[KEY_LEN];
    unsigned char obfus_keys[OBFUS_LAYERS];
    
    gen_random_bytes(key, KEY_LEN);
    gen_random_bytes(obfus_keys, OBFUS_LAYERS);
    
    // Apply obfuscation layers
    for(int layer = 0; layer < OBFUS_LAYERS; layer++) {
        xor_obfuscate(key, KEY_LEN, obfus_keys[layer]);
    }
    
    // Write to header file
    FILE *f = fopen("db_key.h", "w");
    fprintf(f, "#ifndef DB_KEY_H\n#define DB_KEY_H\n");
    fprintf(f, "static const unsigned char DB_KEY[%d] = {", KEY_LEN);
    for(int i = 0; i < KEY_LEN; i++) {
        fprintf(f, "0x%02x%s", key[i], i < KEY_LEN-1 ? "," : "");
    }
    fprintf(f, "};\n");
    fprintf(f, "static const unsigned char OBFUS_KEYS[%d] = {", OBFUS_LAYERS);
    for(int i = 0; i < OBFUS_LAYERS; i++) {
        fprintf(f, "0x%02x%s", obfus_keys[i], i < OBFUS_LAYERS-1 ? "," : "");
    }
    fprintf(f, "};\n#endif\n");
    fclose(f);
    
    // Write secret to file
    unsigned char clean_key[KEY_LEN];
    memcpy(clean_key, key, KEY_LEN);
    
    // Reverse obfuscation to get clean key
    for(int layer = OBFUS_LAYERS-1; layer >= 0; layer--) {
        xor_obfuscate(clean_key, KEY_LEN, obfus_keys[layer]);
    }
    
    f = fopen("db_secret.txt", "w");
    fprintf(f, "Database Key: ");
    for(int i = 0; i < KEY_LEN; i++) {
        fprintf(f, "%02x", clean_key[i]);
    }
    fprintf(f, "\n");
    fclose(f);
    
    return 0;
}