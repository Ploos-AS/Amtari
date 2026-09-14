CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS ?= -Iinclude

BUILD := build
COMMON_SRC := src/amtari.c src/guest.c src/trap.c src/gemdos.c src/fs.c
TEST_M0 := $(BUILD)/test_m0
TEST_M1 := $(BUILD)/test_m1
TEST_M2 := $(BUILD)/test_m2
TEST_M2_FS := $(BUILD)/test_m2_fs

.PHONY: all check clean

all: $(TEST_M0) $(TEST_M1) $(TEST_M2) $(TEST_M2_FS)

$(BUILD):
	mkdir -p $(BUILD)

$(TEST_M0): $(COMMON_SRC) tests/test_m0.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m0.c -o $(TEST_M0)

$(TEST_M1): $(COMMON_SRC) tests/test_m1.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m1.c -o $(TEST_M1)

$(TEST_M2): $(COMMON_SRC) tests/test_m2.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2.c -o $(TEST_M2)

$(TEST_M2_FS): $(COMMON_SRC) tests/test_m2_fs.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_fs.c -o $(TEST_M2_FS)

check: $(TEST_M0) $(TEST_M1) $(TEST_M2) $(TEST_M2_FS)
	./$(TEST_M0)
	./$(TEST_M1)
	./$(TEST_M2)
	./$(TEST_M2_FS)
	@echo "M2 filesystem host checks: PASS"

clean:
	rm -rf $(BUILD)
