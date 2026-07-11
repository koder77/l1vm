CC      = gcc
CFLAGS  = -O2 -Wall -Wextra
LDFLAGS = -lm
PREFIX  = /usr/local

# Detect readline availability early so -DHAVE_READLINE is used
# during compilation of .o files, not just at link time.
HAVE_RL := $(shell pkg-config --exists readline 2>/dev/null && echo 1 || ( [ -f /usr/include/readline/readline.h ] && echo 1 || echo 0 ))
ifeq ($(HAVE_RL),1)
  CFLAGS  += -DHAVE_READLINE
  RL_LIBS := $(shell pkg-config --libs readline 2>/dev/null || echo -lreadline)
  LDFLAGS += $(RL_LIBS)
endif

BINARY  = brackets-code
SOURCE  = brackets-code.c
DSL     = dsl.c
EMBED   = embed.c
LEARN   = learn.c
HEADER  = brackets-code.h
DSL_H   = dsl.h
SYNONYM = synonyms.txt
MANPAGE = brackets-code.1
TEST_SH = tests/run_tests.sh

.PHONY: all test clean install uninstall dist lint check

all: $(BINARY)

%.o: %.c $(HEADER) $(DSL_H)
	$(CC) $(CFLAGS) -c -o $@ $<

# embed.o and learn.o compile clean with -Wall -Wextra

$(BINARY): $(SOURCE:.c=.o) $(DSL:.c=.o) $(EMBED:.c=.o) $(LEARN:.c=.o)
	@if [ "$(HAVE_RL)" = "1" ]; then \
		echo "  readline found, enabling interactive history"; \
	else \
		echo "  readline not found, using plain fgets"; \
	fi
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

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
	rm -f ./*.l1com ./*.l1asm ./*.l1dbg ./*.l1obj
	rm -f tests/saved-failures/*.l1com tests/saved-failures/*_pp.l1com
	rm -f ./*.o

install: $(BINARY) $(SYNONYM) $(MANPAGE) $(HEADER) $(DSL_H)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/share/brackets-code
	install -d $(DESTDIR)$(PREFIX)/share/brackets-code/dsl
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 $(BINARY) $(DESTDIR)$(PREFIX)/bin/$(BINARY)
	install -m 644 $(SYNONYM) $(DESTDIR)$(PREFIX)/share/brackets-code/$(SYNONYM)
	install -m 644 $(HEADER) $(DESTDIR)$(PREFIX)/share/brackets-code/$(HEADER)
	install -m 644 $(DSL_H) $(DESTDIR)$(PREFIX)/share/brackets-code/$(DSL_H)
	install -m 644 dsl/*.l1dsl $(DESTDIR)$(PREFIX)/share/brackets-code/dsl/
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
	cp $(SOURCE) $(DSL) $(EMBED) $(LEARN) $(HEADER) $(DSL_H) $(SYNONYM) $(MANPAGE) Makefile README.md memory.txt /tmp/$$tardir/; \
	cp -r tests l1vm-example-code dsl /tmp/$$tardir/; \
	cd /tmp && tar czf brackets-code-$$VERSION.tar.gz $$tardir; \
	echo "Created /tmp/brackets-code-$$VERSION.tar.gz"
