# Windows 10 Optimizer - Makefile
# Build with: mingw32-make (MinGW) or nmake (MSVC)

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lshell32 -lgdi32
TARGET = win_optimizer.exe

# For MSVC: cl win_optimizer.c /Fe:win_optimizer.exe shell32.lib advapi32.lib /link /MANIFESTUAC:"level='requireAdministrator'"

all: $(TARGET)

$(TARGET): win_optimizer.c
	$(CC) $(CFLAGS) -o $(TARGET) win_optimizer.c $(LDFLAGS)

# Build with admin manifest for UAC elevation (optional - run as admin manually works too)
admin: win_optimizer.c
	$(CC) $(CFLAGS) -o $(TARGET) win_optimizer.c $(LDFLAGS)
	@echo "Note: Run as Administrator for full functionality (memory, Explorer restart)"

clean:
	del /Q $(TARGET) 2>nul || rm -f $(TARGET)

.PHONY: all clean admin
