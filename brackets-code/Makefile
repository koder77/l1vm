CC      = gcc
CFLAGS  = -O2 -Wall -Wextra
LDFLAGS = -lm
PREFIX  = /usr/local

BINARY  = brackets-code
SOURCE  = brackets-code.c
EMBED   = embed.c
LEARN   = learn.c
HEADER  = brackets-code.h
SYNONYM = synonyms.txt
MANPAGE = brackets-code.1
TEST_SH = tests/run_tests.sh

.PHONY: all test clean install uninstall dist lint check

all: $(BINARY)

%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c -o $@ $<

embed.o: CFLAGS += -Wno-unused-function
learn.o: CFLAGS += -Wno-unused-function

$(BINARY): $(SOURCE:.c=.o) $(EMBED:.c=.o) $(LEARN:.c=.o)
	@if pkg-config --exists readline 2>/dev/null; then \
		echo "  readline found, enabling interactive history"; \
		$(CC) $(CFLAGS) -DHAVE_READLINE -o $@ $^ $(LDFLAGS) $$(pkg-config --libs readline); \
	elif [ -f /usr/include/readline/readline.h ]; then \
		echo "  readline headers found, enabling interactive history"; \
		$(CC) $(CFLAGS) -DHAVE_READLINE -o $@ $^ $(LDFLAGS) -lreadline; \
	else \
		echo "  readline not found, using plain fgets"; \
		$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS); \
	fi

# Static analysis
lint: $(SOURCE)
	@which cppcheck >/dev/null 2>&1 && cppcheck --enable=all --inconclusive --suppress=missingIncludeSystem $(SOURCE) 2>&1 || echo "cppcheck not found, skipping"
	@which flawfinder >/dev/null 2>&1 && flawfinder $(SOURCE) 2>&1 || echo "flawfinder not found, skipping"

# Check for common issues
check: $(SOURCE)
	@echo "=== Checking for unsafe functions ==="
	@! grep -n 'sprintf(\|strcpy(\|strcat(' $(SOURCE) || echo "WARNING: unsafe functions found"
	@echo "=== Checking for goto ==="
	@! grep -n 'goto ' $(SOURCE) | grep -v '//\|#\|fprintf\|printf' || echo "WARNING: goto found"
	@echo "=== Checking global non-static functions ==="
	@grep -n '^int \|^void \|^char \|^float \|^const.*\*' $(SOURCE) | grep -v 'static \|main\|//\|/\*\|#define\|typedef\|struct ' || echo "OK"
	@echo "=== Done ==="

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
	rm -f *.o

install: $(BINARY) $(SYNONYM) $(MANPAGE) $(HEADER)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/brackets-code
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 $(BINARY) $(DESTDIR)$(PREFIX)/bin/$(BINARY)
	install -m 644 $(SYNONYM) $(DESTDIR)$(PREFIX)/share/brackets-code/$(SYNONYM)
	install -m 644 $(HEADER) $(DESTDIR)$(PREFIX)/share/brackets-code/$(HEADER)
	install -m 644 $(MANPAGE) $(DESTDIR)$(PREFIX)/share/man/man1/$(MANPAGE)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BINARY)
	rm -f $(DESTDIR)$(PREFIX)/share/brackets-code/$(SYNONYM)
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(MANPAGE)

# Source distribution tarball
dist:
	@VERSION="$$(grep 'VERSION_TXT' $(SOURCE) | cut -d'"' -f2)"; \
	tardir="brackets-code-$$VERSION"; \
	mkdir -p /tmp/$$tardir; \
	cp $(SOURCE) $(EMBED) $(LEARN) $(HEADER) $(SYNONYM) $(MANPAGE) Makefile README.md memory.txt /tmp/$$tardir/; \
	cp -r tests l1vm-example-code /tmp/$$tardir/; \
	cd /tmp && tar czf brackets-code-$$VERSION.tar.gz $$tardir; \
	echo "Created /tmp/brackets-code-$$VERSION.tar.gz"
