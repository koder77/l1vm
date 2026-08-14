CC      ?= gcc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
LDFLAGS ?=

SRCDIR  := src
BUILDDIR:= build

SRCS := $(SRCDIR)/main.c $(SRCDIR)/lsp.c $(SRCDIR)/l1lang.c $(SRCDIR)/json.c $(SRCDIR)/sb.c
OBJS := $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

TARGET := $(BUILDDIR)/l1vm-lsp

all: $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

test: $(TARGET)
	@python3 tests/smoke.py

clean:
	rm -rf $(BUILDDIR)

.PHONY: all test clean
