#include "server.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

static char db_password[DB_PASSWORD_SIZE + 1] = {0};

void generate_db_password() {
    srand(time(NULL));
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()";
    
    for (int i = 0; i < DB_PASSWORD_SIZE; i++) {
        int key = rand() % (sizeof(charset) - 1);
        db_password[i] = charset[key];
    }
    db_password[DB_PASSWORD_SIZE] = '\0';
}

const char* get_db_password() {
    if (db_password[0] == '\0') {
        generate_db_password();
    }
    return db_password;
}

void set_db_password(const char* password) {
    strncpy(db_password, password, DB_PASSWORD_SIZE);
    db_password[DB_PASSWORD_SIZE] = '\0';
}