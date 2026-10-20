CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wshadow -Wpointer-arith -fPIC -Iinclude -pthread
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Iinclude -pthread
LDLIBS  += -lm
LDFLAGS += -pthread

LIB     := libvecdb.so
SRC     := src/db.c src/text.c src/hnsw.c
OBJ     := $(SRC:.c=.o)

all: $(LIB)

$(LIB): $(OBJ) src/vecdb.map
	$(CC) -shared -o $@ $(OBJ) $(LDFLAGS) $(LDLIBS) -Wl,--version-script=src/vecdb.map

src/%.o: src/%.c src/internal.h include/vecdb.h
	$(CC) $(CFLAGS) -c $< -o $@

demo: examples/demo.c $(LIB)
	$(CC) $(CFLAGS) -o demo examples/demo.c -L. -lvecdb -Wl,-rpath,'$$ORIGIN' $(LDLIBS)

test: tests/test_vecdb.c $(LIB)
	$(CC) $(CFLAGS) -o test_vecdb tests/test_vecdb.c -L. -lvecdb -Wl,-rpath,'$$ORIGIN' $(LDLIBS)
	./test_vecdb

bench: examples/bench.c $(LIB)
	$(CC) $(CFLAGS) -o bench examples/bench.c -L. -lvecdb -Wl,-rpath,'$$ORIGIN' $(LDLIBS)

fuzz: tests/fuzz.c $(LIB)
	$(CC) $(CFLAGS) -o fuzz tests/fuzz.c -L. -lvecdb -Wl,-rpath,'$$ORIGIN' $(LDLIBS)
	./fuzz

cpp_demo: examples/cpp_demo.cpp include/vecdb.hpp $(LIB)
	$(CXX) $(CXXFLAGS) -o cpp_demo examples/cpp_demo.cpp -L. -lvecdb -Wl,-rpath,'$$ORIGIN' $(LDLIBS)

CLI := vecdb-cli
$(CLI): tools/vecdb-cli.c $(LIB)
	$(CC) $(CFLAGS) -o $@ tools/vecdb-cli.c -L. -lvecdb -Wl,-rpath,'$$ORIGIN' $(LDLIBS)

cli: $(CLI)

pytest: $(LIB) bindings/python/vecdb.py bindings/python/test_vecdb.py
	VECDB_LIBRARY=$$(pwd)/$(LIB) python3 bindings/python/test_vecdb.py

jstest: $(LIB) bindings/javascript/index.js bindings/javascript/test.js
	cd bindings/javascript && \
	  ([ -d node_modules/koffi ] || npm install --no-audit --no-fund) && \
	  node test.js

gotest: $(LIB)
	cd bindings/go && go test ./...

rusttest: $(LIB)
	cd bindings/rust && cargo test

javatest: $(LIB)
	cd bindings/java && ./run-tests.sh

bindings-test: pytest jstest gotest rusttest javatest

clean:
	rm -f $(OBJ) $(LIB) demo test_vecdb bench fuzz cpp_demo $(CLI)
	rm -rf bindings/java/build

.PHONY: all clean demo test bench fuzz cpp_demo cli pytest jstest gotest rusttest javatest bindings-test
