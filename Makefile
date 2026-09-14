CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS ?= -Iinclude

BUILD := build
COMMON_SRC := src/amtari.c src/guest.c src/trap.c
TEST_M0 := $(BUILD)/test_m0
TEST_M1 := $(BUILD)/test_m1

.PHONY: all check clean

all: $(TEST_M0) $(TEST_M1)

$(BUILD):
	mkdir -p $(BUILD)

$(TEST_M0): $(COMMON_SRC) tests/test_m0.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m0.c -o $(TEST_M0)

$(TEST_M1): $(COMMON_SRC) tests/test_m1.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m1.c -o $(TEST_M1)

check: $(TEST_M0) $(TEST_M1)
	./$(TEST_M0)
	./$(TEST_M1)
	@echo "M1 host checks: PASS"

clean:
	rm -rf $(BUILD)
