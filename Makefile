CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS ?= -Iinclude

BUILD := build
COMMON_SRC := src/amtari.c src/guest.c src/trap.c src/gemdos.c src/fs.c src/prg.c src/exec.c
TEST_M0 := $(BUILD)/test_m0
TEST_M1 := $(BUILD)/test_m1
TEST_M2 := $(BUILD)/test_m2
TEST_M2_FS := $(BUILD)/test_m2_fs
TEST_M2_PRG := $(BUILD)/test_m2_prg
TEST_M2_EXEC := $(BUILD)/test_m2_exec

.PHONY: all check clean

all: $(TEST_M0) $(TEST_M1) $(TEST_M2) $(TEST_M2_FS) $(TEST_M2_PRG) $(TEST_M2_EXEC)

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

$(TEST_M2_PRG): $(COMMON_SRC) tests/test_m2_prg.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_prg.c -o $(TEST_M2_PRG)

$(TEST_M2_EXEC): $(COMMON_SRC) tests/test_m2_exec.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_exec.c -o $(TEST_M2_EXEC)

check: $(TEST_M0) $(TEST_M1) $(TEST_M2) $(TEST_M2_FS) $(TEST_M2_PRG) $(TEST_M2_EXEC)
	./$(TEST_M0)
	./$(TEST_M1)
	./$(TEST_M2)
	./$(TEST_M2_FS)
	./$(TEST_M2_PRG)
	./$(TEST_M2_EXEC)
	@echo "M2.4 execution handoff host checks: PASS"

clean:
	rm -rf $(BUILD)
