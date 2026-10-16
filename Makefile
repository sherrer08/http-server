CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -g -MMD -MP

CPPFLAGS = -Iinclude

LDFLAGS = 

SAN_FLAGS = -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

SRCS = $(wildcard src/*.c)

OBJS = $(patsubst src/%.c,build/%.o,$(SRCS))

LIB_OBJS = $(filter-out build/main.o, $(OBJS))

LIB_SRCS = $(filter-out src/main.c, $(SRCS))

TEST_OBJS = build/test_util.o

.PHONY: all clean run test test-asan

all: build/server build/test_util

build/server: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

build/test_util: build/test_util.o $(LIB_OBJS)
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

test: build/test_util
	./build/test_util

test-asan: 
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) tests/test_util.c $(LIB_SRCS) $(LDFLAGS) -o build/test_util_asan
	./build/test_util_asan

-include $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

print:
	@echo "OBJS: $(OBJS)"
	@echo "LIB_OBJS: $(LIB_OBJS)"
	@echo "LIB_SRCS: $(LIB_SRCS)"