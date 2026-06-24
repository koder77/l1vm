CC      = gcc
CFLAGS  = -O2 -Wall -Wextra
LDFLAGS = -lm

BINARY  = brackets-code
SOURCE  = brackets-code.c

TEST_SH = tests/run_tests.sh

.PHONY: all test clean install

all: $(BINARY)

$(BINARY): $(SOURCE)
	@if pkg-config --exists readline 2>/dev/null; then \
		echo "  readline found, enabling interactive history"; \
		$(CC) $(CFLAGS) -DHAVE_READLINE -o $@ $< $(LDFLAGS) $$(pkg-config --libs readline); \
	elif [ -f /usr/include/readline/readline.h ]; then \
		echo "  readline headers found, enabling interactive history"; \
		$(CC) $(CFLAGS) -DHAVE_READLINE -o $@ $< $(LDFLAGS) -lreadline; \
	else \
		echo "  readline not found, using plain fgets"; \
		$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS); \
	fi

test: $(BINARY)
	@if [ -x "$(TEST_SH)" ]; then \
		$(TEST_SH); \
	else \
		echo "Test suite not found at $(TEST_SH)"; exit 1; \
	fi

clean:
	rm -f $(BINARY)
	rm -f *.l1com *.l1asm *.l1dbg *.l1obj
	rm -f tests/saved-failures/*.l1com tests/saved-failures/*_pp.l1com

install: $(BINARY)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(BINARY) $(DESTDIR)/usr/local/bin/$(BINARY)
