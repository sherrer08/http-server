CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -g -MMD -MP

CPPFLAGS = -Iinclude -D_POSIX_C_SOURCE=200809L

LDFLAGS = 

SAN_FLAGS = -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

SRCS = $(wildcard src/*.c)

OBJS = $(patsubst src/%.c,build/%.o,$(SRCS))

LIB_OBJS = $(filter-out build/main.o, $(OBJS))

LIB_SRCS = $(filter-out src/main.c, $(SRCS))

TEST_NAME = test_http

TEST_OBJS = build/$(TEST_NAME).o

.PHONY: all clean run test test-asan

all: build/server build/$(TEST_NAME)

build/server: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

build/$(TEST_NAME): build/$(TEST_NAME).o $(LIB_OBJS)
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

test: build/$(TEST_NAME)
	./build/$(TEST_NAME)

test-asan: 
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SAN_FLAGS) tests/$(TEST_NAME).c $(LIB_SRCS) $(LDFLAGS) -o build/$(TEST_NAME)_asan
	./build/$(TEST_NAME)_asan

-include $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

print:
	@echo "OBJS: $(OBJS)"
	@echo "LIB_OBJS: $(LIB_OBJS)"
	@echo "LIB_SRCS: $(LIB_SRCS)"