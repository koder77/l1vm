CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=

BUILD   = build
TARGET  = $(BUILD)/brackets-llm

SRCS = src/main.c src/http.c src/lspclient.c src/json.c src/sb.c src/tools.c
OBJS = $(SRCS:src/%.c=$(BUILD)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

install: $(TARGET)
	install -m 755 $(TARGET) ~/l1vm/bin/brackets-llm

.PHONY: all clean install
