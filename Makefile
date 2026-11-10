CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -g -MMD -MP -Wshadow -Wmissing-prototypes

CPPFLAGS = -Iinclude -D_XOPEN_SOURCE=700

LDFLAGS = 

SAN_FLAGS = -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

SRCS = $(wildcard src/*.c)

OBJS = $(patsubst src/%.c,build/%.o,$(SRCS))

LIB_OBJS = $(filter-out build/main.o, $(OBJS))

LIB_SRCS = $(filter-out src/main.c, $(SRCS))

TEST_SRCS = $(wildcard tests/test_*.c)

TEST_OBJS = $(patsubst tests/%.c,build/%.o,$(TEST_SRCS))

TEST_BINS = $(patsubst tests/%.c,build/%,$(TEST_SRCS))

.PHONY: all clean run run-asan test test-asan smoke smoke-asan print

all: build/server $(TEST_BINS)

build/server: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(TEST_BINS): build/%: build/%.o $(LIB_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/%.o: tests/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

run: build/server
	./build/server

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "== $$t"; ./$$t || exit 1; done

test-asan: 
	@mkdir -p build
	@for src in $(TEST_SRCS); do \
		name=$$(basename $$src .c); \
		echo "== $$name (sanitizers)"; \
		$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) $$src $(LIB_SRCS) $(LDFLAGS) -o build/$${name}_asan || exit 1; \
		./build/$${name}_asan || exit 1; \
	done

run-asan:
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) $(SRCS) $(LDFLAGS) -o build/server_asan
	./build/server_asan

smoke: build/server
	./tests/smoke.sh ./build/server

smoke-asan:
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) $(SRCS) $(LDFLAGS) -o build/server_asan
	./tests/smoke.sh ./build/server_asan

-include $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

print:
	@echo "TEST_SRCS: $(TEST_SRCS)"
	@echo "TEST_BINS: $(TEST_BINS)"