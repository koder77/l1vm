CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=

BUILD   = build
TARGET  = $(BUILD)/brackets-llm

SRCS = src/main.c src/http.c src/lspclient.c src/json.c src/sb.c src/tools.c \
       src/inputline.c
OBJS = $(SRCS:src/%.c=$(BUILD)/%.o)

# Optional GNU readline (line editing + history for interactive input).
# A small program is compiled at make time; when that succeeds against
# -lreadline, the feature flag and link library are enabled. When the host
# OS has no readline, input falls back to plain fgets automatically.
READLINE_OK  := $(shell mkdir -p $(BUILD) && printf '#include <readline/readline.h>\n#include <readline/history.h>\n#include <stdlib.h>\nint main(void){char *l=readline(NULL); if(l) free(l); return 0;}\n' > $(BUILD)/rl-check.c && $(CC) $(BUILD)/rl-check.c -o $(BUILD)/rl-check -lreadline >/dev/null 2>&1; echo $$?)
READLINE_CFLAGS := $(if $(filter 0,$(READLINE_OK)),-DHAVE_LIBREADLINE,)
READLINE_LIBS   := $(if $(filter 0,$(READLINE_OK)),-lreadline,)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) $(READLINE_LIBS)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(READLINE_CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

install: $(TARGET)
	install -m 755 $(TARGET) ~/l1vm/bin/brackets-llm

.PHONY: all clean install
