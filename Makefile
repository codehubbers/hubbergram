# Detect MSYS2 environment
ifeq ($(MSYSTEM),MINGW64)
    CC = gcc
    CFLAGS = -Wall -Wextra -std=c99 -pthread -D_GNU_SOURCE -I/mingw64/include
    LIBS = -lsqlite3 -ljson-c -lssl -lcrypto -lpthread -L/mingw64/lib
    LDFLAGS = -lmingw32_extended
else
    CC = gcc
    CFLAGS = -Wall -Wextra -std=c99 -pthread -D_GNU_SOURCE
    LIBS = -lsqlite3 -ljson-c -lssl -lcrypto -lpthread
    LDFLAGS = -lmingw32_extended
endif

SRCDIR = source
INCDIR = include
BUILDDIR = build
SOURCES = $(filter-out $(SRCDIR)/gen_db_key.c, $(wildcard $(SRCDIR)/*.c))
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
TARGET = $(BUILDDIR)/telegram_clone

.PHONY: all clean install install-msys2 deps-msys2 install-libmingw32 gen-db-key $(BUILDDIR)

all: install-libmingw32 $(BUILDDIR) gen-db-key $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

gen-db-key: $(BUILDDIR)
	@echo "Generating secure database key..."
	$(CC) -o $(BUILDDIR)/gen_db_key $(SRCDIR)/gen_db_key.c
	cd $(BUILDDIR) && ./gen_db_key
	rm -f $(BUILDDIR)/gen_db_key $(BUILDDIR)/gen_db_key.exe
	@echo "Database key generated and obfuscated"

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS) $(LDFLAGS)
	@echo "Build complete with embedded security"

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(BUILDDIR) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) telegram_clone.db

clean-all: clean
	rm -rf libmingw32_extended

install:
	@echo "Installing dependencies..."
	@echo "On Ubuntu/Debian: sudo apt-get install libsqlite3-dev libjson-c-dev libssl-dev build-essential git"
	@echo "On CentOS/RHEL: sudo yum install sqlite-devel json-c-devel openssl-devel gcc make git"
	@echo "On macOS: brew install sqlite json-c openssl"
	@echo "On MSYS2: make install-msys2"
	@echo "Note: libmingw32_extended will be automatically downloaded and installed when building"

install-msys2:
	@echo "Installing MSYS2 dependencies..."
	pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-json-c mingw-w64-x86_64-openssl mingw-w64-x86_64-make git

install-libmingw32:
	@echo "Installing libmingw32_extended dependency..."
	@if [ ! -d "libmingw32_extended" ]; then \
		echo "Cloning libmingw32_extended..."; \
		git clone https://github.com/CoderRC/libmingw32_extended.git; \
	fi
	@if [ ! -f "/mingw64/lib/libmingw32_extended.a" ] && [ ! -f "/usr/local/lib/libmingw32_extended.a" ]; then \
		echo "Building and installing libmingw32_extended..."; \
		cd libmingw32_extended && \
		mkdir -p build && \
		cd build && \
		../configure && \
		make && \
		make install; \
	else \
		echo "libmingw32_extended already installed"; \
	fi

deps-msys2:
	@echo "Checking MSYS2 dependencies..."
	@pacman -Q mingw-w64-x86_64-gcc mingw-w64-x86_64-sqlite3 mingw-w64-x86_64-json-c mingw-w64-x86_64-openssl || echo "Run 'make install-msys2' to install missing dependencies"

run: $(TARGET)
	$(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

help:
	@echo "Available targets:"
	@echo "  all          - Build the telegram clone server (auto-installs libmingw32_extended)"
	@echo "  clean        - Remove build files and database"
	@echo "  clean-all    - Remove build files, database, and downloaded dependencies"
	@echo "  install      - Show dependency installation commands"
	@echo "  install-msys2 - Install dependencies via pacman (MSYS2)"
	@echo "  install-libmingw32 - Download, build and install libmingw32_extended"
	@echo "  deps-msys2   - Check MSYS2 dependencies"
	@echo "  run          - Build and run the server"
	@echo "  debug        - Build with debug symbols"
	@echo "  help         - Show this help message"