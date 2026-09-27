CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=

BUILD   = build
TARGET  = $(BUILD)/brackets-llm

SRCS = src/main.c src/http.c src/lspclient.c src/json.c src/sb.c src/tools.c \
       src/inputline.c src/intr.c
OBJS = $(SRCS:src/%.c=$(BUILD)/%.o)

# Optional GNU readline (line editing + history for interactive input).
# A small program is compiled at make time; when that succeeds against
# -lreadline, the feature flag and link library are enabled. When the host
# OS has no readline, input falls back to plain fgets automatically.
READLINE_OK  := $(shell mkdir -p $(BUILD) && printf '#include <readline/readline.h>\n#include <readline/history.h>\n#include <stdlib.h>\nint main(void){char *l=readline(NULL); if(l) free(l); return 0;}\n' > $(BUILD)/rl-check.c && $(CC) $(BUILD)/rl-check.c -o $(BUILD)/rl-check -lreadline >/dev/null 2>&1; echo $$?)
READLINE_CFLAGS := $(if $(filter 0,$(READLINE_OK)),-DHAVE_LIBREADLINE,)
READLINE_LIBS   := $(if $(filter 0,$(READLINE_OK)),-lreadline,)

# Optional libcurl, only needed for https:// in the web_fetch tool. The same
# kind of compile probe as for readline decides: when it succeeds,
# -DHAVE_LIBCURL and the curl flags are used. Without libcurl the program
# still speaks plain http:// itself and calls the "curl" program for https.
# Build without libcurl:  make CURL_OK=1
CURL_CFLAGS := $(shell curl-config --cflags 2>/dev/null)
CURL_LIBS   := $(shell curl-config --libs 2>/dev/null)
CURL_OK     ?= $(shell mkdir -p $(BUILD) && printf '#include <curl/curl.h>\nint main(void){curl_global_init(0); curl_easy_cleanup(curl_easy_init()); return 0;}\n' > $(BUILD)/curl-check.c && $(CC) $(CURL_CFLAGS) $(BUILD)/curl-check.c -o $(BUILD)/curl-check $(CURL_LIBS) >/dev/null 2>&1; echo $$?)
CURL_DEFINE := $(if $(filter 0,$(CURL_OK)),-DHAVE_LIBCURL,)
CURL_LINK   := $(if $(filter 0,$(CURL_OK)),$(CURL_LIBS),)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) $(READLINE_LIBS) $(CURL_LINK)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(READLINE_CFLAGS) $(CURL_DEFINE) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

install: $(TARGET)
	install -m 755 $(TARGET) ~/l1vm/bin/brackets-llm

.PHONY: all clean install
