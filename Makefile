CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS ?= -Iinclude

BUILD := build
TEST := $(BUILD)/test_m0

.PHONY: all check clean

all: $(TEST)

$(BUILD):
	mkdir -p $(BUILD)

$(TEST): src/amtari.c tests/test_m0.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/amtari.c tests/test_m0.c -o $(TEST)

check: $(TEST)
	./$(TEST)
	@echo "M0 host checks: PASS"

clean:
	rm -rf $(BUILD)
