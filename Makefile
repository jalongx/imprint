CC := gcc

# Architecture-safe baseline for maximum compatibility
CFLAGS := -Wall -Wextra -O2 -march=x86-64-v2 -mtune=generic
CFLAGS += -frecord-gcc-switches
CFLAGS += -DIMPRINT_BUILD_FLAGS="\"$(CFLAGS)\""

LDFLAGS := -lcrypto -lzstd -llz4

SRC_DIR := src
BUILD_DIR := build

SRCS_COMMON := \
    $(SRC_DIR)/utils.c \
    $(SRC_DIR)/ui.c \
    $(SRC_DIR)/config.c

SRCS_BACKUP := \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/backup.c

SRCS_RESTORE := \
    $(SRC_DIR)/main_restore.c \
    $(SRC_DIR)/restore.c

# New: sniffer sources
SRCS_SNIFFER := \
    $(SRC_DIR)/sniffer.c \
    $(SRC_DIR)/imprint-sniffer.c

OBJS_COMMON   := $(SRCS_COMMON:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_BACKUP   := $(SRCS_BACKUP:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_RESTORE  := $(SRCS_RESTORE:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS_SNIFFER  := $(SRCS_SNIFFER:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

TARGET_BACKUP   := imprintb
TARGET_RESTORE  := imprintr
TARGET_SNIFFER  := imprint-sniffer

all: $(TARGET_BACKUP) $(TARGET_RESTORE) $(TARGET_SNIFFER)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_BACKUP): $(OBJS_COMMON) $(OBJS_BACKUP)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_RESTORE): $(OBJS_COMMON) $(OBJS_RESTORE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# New: sniffer binary
$(TARGET_SNIFFER): $(OBJS_COMMON) $(OBJS_SNIFFER)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET_BACKUP) $(TARGET_RESTORE) $(TARGET_SNIFFER)

# Quick sanity check: ensure no v3/v4 instructions slipped in
verify-isa:
	@echo "Checking for AVX/AVX2/FMA/BMI/MOVBE instructions..."
	@if objdump -d $(TARGET_BACKUP) | grep -E 'avx|avx2|fma|bmi|movbe' ; then \
        echo "ERROR: v3/v4 instructions detected!" ; \
        exit 1 ; \
    else \
        echo "OK: No v3/v4 instructions found in $(TARGET_BACKUP)" ; \
    fi

.PHONY: all clean verify-isa
