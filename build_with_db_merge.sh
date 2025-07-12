#!/bin/bash

DB_FILE="telegram_clone.db"
OLD_KEY_FILE="build/db_key.h"
BACKUP_DIR="db_backup"

echo "=== Database Merge Build Script ==="

# Check if database exists
if [ -f "$DB_FILE" ]; then
    echo "✓ Existing database found: $DB_FILE"
    
    # Check if old key exists
    if [ -f "$OLD_KEY_FILE" ]; then
        echo "✓ Previous database key found"
        
        # Create backup directory
        mkdir -p "$BACKUP_DIR"
        
        # Backup current database
        cp "$DB_FILE" "$BACKUP_DIR/telegram_clone_backup_$(date +%Y%m%d_%H%M%S).db"
        echo "✓ Database backed up to $BACKUP_DIR/"
        
        # Test database access with old key
        echo "Testing database access with existing key..."
        
        # Create test program
        cat > build/test_db_access.c << 'EOF'
#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include "../include/db_security.h"

int main() {
    sqlite3 *db;
    char password[256];
    char pragma_cmd[512];
    
    get_db_password(password, sizeof(password));
    
    int rc = sqlite3_open("telegram_clone.db", &db);
    if (rc) {
        printf("ERROR: Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    snprintf(pragma_cmd, sizeof(pragma_cmd), "PRAGMA key = '%s';", password);
    rc = sqlite3_exec(db, pragma_cmd, 0, 0, 0);
    if (rc != SQLITE_OK) {
        printf("ERROR: Key verification failed\n");
        sqlite3_close(db);
        return 1;
    }
    
    // Test query
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM users;", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        printf("SUCCESS: Database access granted\n");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 0;
    } else {
        printf("ERROR: Database query failed\n");
        sqlite3_close(db);
        return 1;
    }
}
EOF
        
        # Compile and test
        gcc -I include -I build -o build/test_db_access build/test_db_access.c -lsqlite3
        
        if ./build/test_db_access; then
            echo "✓ Database access verified - keeping existing key"
            KEEP_OLD_KEY=true
        else
            echo "✗ Database access failed - will generate new key"
            KEEP_OLD_KEY=false
        fi
        
        # Cleanup test files
        rm -f build/test_db_access build/test_db_access.c build/test_db_access.exe
        
    else
        echo "✗ No previous key found - will generate new key"
        KEEP_OLD_KEY=false
    fi
else
    echo "ℹ No existing database - will create new one"
    KEEP_OLD_KEY=false
fi

# Build process
echo ""
echo "=== Starting Build Process ==="

if [ "$KEEP_OLD_KEY" = true ]; then
    echo "Building with existing database key..."
    # Build without regenerating key
    make install-libmingw32
    mkdir -p build
    # Skip gen-db-key step and build directly
    make $(make -n all | grep -v gen-db-key | tail -1)
else
    echo "Building with new database key..."
    # Normal build process
    make all
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "=== Build Successful ==="
    
    if [ "$KEEP_OLD_KEY" = true ]; then
        echo "✓ Database preserved with existing access"
        echo "✓ All existing data remains accessible"
    else
        echo "✓ New database key generated"
        if [ -f "$DB_FILE" ]; then
            echo "⚠ Previous database will need re-initialization"
        fi
    fi
    
    echo ""
    echo "Database backups available in: $BACKUP_DIR/"
    echo "Run './build/telegram_clone' to start the server"
else
    echo ""
    echo "=== Build Failed ==="
    exit 1
fi