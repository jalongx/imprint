CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -lcrypto

SRC_DIR = src
BUILD_DIR = build

SRCS_COMMON = \
    $(SRC_DIR)/utils.c \
    $(SRC_DIR)/ui.c \
    $(SRC_DIR)/config.c

SRCS_BACKUP = \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/backup.c

SRCS_RESTORE = \
    $(SRC_DIR)/main_restore.c \
    $(SRC_DIR)/restore.c

OBJS_COMMON   = $(SRCS_COMMON:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_BACKUP   = $(SRCS_BACKUP:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_RESTORE  = $(SRCS_RESTORE:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TARGET_BACKUP  = ghostxb
TARGET_RESTORE = ghostxr

all: $(TARGET_BACKUP) $(TARGET_RESTORE)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_BACKUP): $(OBJS_COMMON) $(OBJS_BACKUP)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_RESTORE): $(OBJS_COMMON) $(OBJS_RESTORE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET_BACKUP) $(TARGET_RESTORE)

.PHONY: all clean
