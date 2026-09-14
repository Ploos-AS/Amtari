CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2
CPPFLAGS ?= -Iinclude
CROSS_CC ?= m68k-linux-gnu-gcc
CROSS_OBJCOPY ?= m68k-linux-gnu-objcopy
CROSS_OBJDUMP ?= m68k-linux-gnu-objdump

BUILD := build
COMMON_SRC := src/amtari.c src/guest.c src/trap.c src/gemdos_m216.c src/fs.c src/prg.c src/exec_m215.c
TEST_M0 := $(BUILD)/test_m0
TEST_M1 := $(BUILD)/test_m1
TEST_M2 := $(BUILD)/test_m2
TEST_M2_FS := $(BUILD)/test_m2_fs
TEST_M2_PRG := $(BUILD)/test_m2_prg
TEST_M2_EXEC := $(BUILD)/test_m2_exec
TEST_M2_E2E := $(BUILD)/test_m2_e2e
TEST_M2_COND := $(BUILD)/test_m2_cond
TEST_M2_ARITH := $(BUILD)/test_m2_arith
TEST_M2_ADDR := $(BUILD)/test_m2_addressing
TEST_M2_COMPILER := $(BUILD)/test_m2_compiler
TEST_M2_PROCESS := $(BUILD)/test_m2_process
TEST_M2_CROSS := $(BUILD)/test_m2_cross
CROSS_OBJ := $(BUILD)/m2_11_real.o
CROSS_TEXT := $(BUILD)/m2_11_real.text
CROSS_PRG := $(BUILD)/m2_11_real.prg
STRESS_OBJ := $(BUILD)/m2_12_stress.o
STRESS_ELF := $(BUILD)/m2_12_stress.elf
STRESS_TEXT := $(BUILD)/m2_12_stress.text
STRESS_PRG := $(BUILD)/m2_12_stress.prg
M213_OBJ := $(BUILD)/m2_13_indexed_movem.o
M213_ELF := $(BUILD)/m2_13_indexed_movem.elf
M213_TEXT := $(BUILD)/m2_13_indexed_movem.text
M213_PRG := $(BUILD)/m2_13_indexed_movem.prg
M213_INDEX_OBJ := $(BUILD)/m2_13_indexed.o
M213_INDEX_TEXT := $(BUILD)/m2_13_indexed.text
M213_INDEX_PRG := $(BUILD)/m2_13_indexed.prg
M214_C_OBJ := $(BUILD)/m2_14_globals.o
M214_START_OBJ := $(BUILD)/m2_14_start.o
M214_ELF := $(BUILD)/m2_14_globals.elf
M214_TEXT := $(BUILD)/m2_14_globals.text
M214_DATA := $(BUILD)/m2_14_globals.data
M214_PRG := $(BUILD)/m2_14_globals.prg
M215_OBJ := $(BUILD)/m2_15_reloc.o
M215_ELF := $(BUILD)/m2_15_reloc.elf
M215_TEXT := $(BUILD)/m2_15_reloc.text
M215_DATA := $(BUILD)/m2_15_reloc.data
M215_PRG := $(BUILD)/m2_15_reloc.prg

.PHONY: all check cross-check clean

all: $(TEST_M0) $(TEST_M1) $(TEST_M2) $(TEST_M2_FS) $(TEST_M2_PRG) $(TEST_M2_EXEC) $(TEST_M2_E2E) $(TEST_M2_COND) $(TEST_M2_ARITH) $(TEST_M2_ADDR) $(TEST_M2_COMPILER) $(TEST_M2_PROCESS)

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
$(TEST_M2_E2E): $(COMMON_SRC) tests/test_m2_e2e.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_e2e.c -o $(TEST_M2_E2E)
$(TEST_M2_COND): $(COMMON_SRC) tests/test_m2_cond.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_cond.c -o $(TEST_M2_COND)
$(TEST_M2_ARITH): $(COMMON_SRC) tests/test_m2_arith.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_arith.c -o $(TEST_M2_ARITH)
$(TEST_M2_ADDR): $(COMMON_SRC) tests/test_m2_addressing.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_addressing.c -o $(TEST_M2_ADDR)
$(TEST_M2_COMPILER): $(COMMON_SRC) tests/test_m2_compiler.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_compiler.c -o $(TEST_M2_COMPILER)
$(TEST_M2_PROCESS): $(COMMON_SRC) tests/test_m2_process.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_process.c -o $(TEST_M2_PROCESS)
$(TEST_M2_CROSS): $(COMMON_SRC) tests/test_m2_cross.c include/amtari.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COMMON_SRC) tests/test_m2_cross.c -o $(TEST_M2_CROSS)

$(CROSS_OBJ): tests/fixtures/m2_11_real.c | $(BUILD)
	$(CROSS_CC) -m68000 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fomit-frame-pointer -c $< -o $@
$(CROSS_TEXT): $(CROSS_OBJ)
	$(CROSS_OBJCOPY) -O binary -j .text $< $@
$(CROSS_PRG): $(CROSS_TEXT) tools/make_tos_prg.py
	python3 tools/make_tos_prg.py $(CROSS_TEXT) $@

$(STRESS_OBJ): tests/fixtures/m2_12_stress.c | $(BUILD)
	$(CROSS_CC) -m68000 -Os -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fomit-frame-pointer -fno-toplevel-reorder -c $< -o $@
$(STRESS_ELF): $(STRESS_OBJ)
	$(CROSS_CC) -m68000 -nostdlib -Wl,--relax -Wl,-Ttext=0 -Wl,-e,amtari_entry -Wl,--build-id=none $< -o $@
$(STRESS_TEXT): $(STRESS_ELF)
	$(CROSS_OBJCOPY) -O binary -j .text $< $@
$(STRESS_PRG): $(STRESS_TEXT) tools/make_tos_prg.py
	python3 tools/make_tos_prg.py $(STRESS_TEXT) $@

$(M213_OBJ): tests/fixtures/m2_13_indexed_movem.c | $(BUILD)
	$(CROSS_CC) -m68000 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fomit-frame-pointer -fno-toplevel-reorder -c $< -o $@
$(M213_ELF): $(M213_OBJ)
	$(CROSS_CC) -m68000 -nostdlib -Wl,--relax -Wl,-Ttext=0 -Wl,-e,amtari_entry -Wl,--build-id=none $< -o $@
$(M213_TEXT): $(M213_ELF)
	$(CROSS_OBJCOPY) -O binary -j .text $< $@
$(M213_PRG): $(M213_TEXT) tools/make_tos_prg.py
	python3 tools/make_tos_prg.py $(M213_TEXT) $@

$(M213_INDEX_OBJ): tests/fixtures/m2_13_indexed.S | $(BUILD)
	$(CROSS_CC) -m68000 -c $< -o $@
$(M213_INDEX_TEXT): $(M213_INDEX_OBJ)
	$(CROSS_OBJCOPY) -O binary -j .text $< $@
$(M213_INDEX_PRG): $(M213_INDEX_TEXT) tools/make_tos_prg.py
	python3 tools/make_tos_prg.py $(M213_INDEX_TEXT) $@

$(M214_C_OBJ): tests/fixtures/m2_14_globals.c | $(BUILD)
	$(CROSS_CC) -m68000 -Os -mpcrel -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fomit-frame-pointer -c $< -o $@
$(M214_START_OBJ): tests/fixtures/m2_14_start.S | $(BUILD)
	$(CROSS_CC) -m68000 -c $< -o $@
$(M214_ELF): $(M214_START_OBJ) $(M214_C_OBJ) tests/fixtures/m2_14.ld
	$(CROSS_CC) -m68000 -nostdlib -Wl,--relax -Wl,-T,tests/fixtures/m2_14.ld -Wl,-e,_start -Wl,--build-id=none $(M214_START_OBJ) $(M214_C_OBJ) -o $@
$(M214_TEXT): $(M214_ELF)
	$(CROSS_OBJCOPY) -O binary -j .text $< $@
$(M214_DATA): $(M214_ELF)
	$(CROSS_OBJCOPY) -O binary -j .data $< $@
$(M214_PRG): $(M214_TEXT) $(M214_DATA) tools/make_tos_prg_sections.py
	python3 tools/make_tos_prg_sections.py $(M214_TEXT) $(M214_DATA) $@ --bss 4

$(M215_OBJ): tests/fixtures/m2_15_reloc.S | $(BUILD)
	$(CROSS_CC) -m68000 -c $< -o $@
$(M215_ELF): $(M215_OBJ) tests/fixtures/m2_15.ld
	$(CROSS_CC) -m68000 -nostdlib -Wl,-T,tests/fixtures/m2_15.ld -Wl,-e,_start -Wl,--build-id=none $< -o $@
$(M215_TEXT): $(M215_ELF)
	$(CROSS_OBJCOPY) -O binary -j .text $< $@
$(M215_DATA): $(M215_ELF)
	$(CROSS_OBJCOPY) -O binary -j .data $< $@
$(M215_PRG): $(M215_TEXT) $(M215_DATA) tools/make_tos_prg_sections.py
	python3 tools/make_tos_prg_sections.py $(M215_TEXT) $(M215_DATA) $@ --reloc 2

check: $(TEST_M0) $(TEST_M1) $(TEST_M2) $(TEST_M2_FS) $(TEST_M2_PRG) $(TEST_M2_EXEC) $(TEST_M2_E2E) $(TEST_M2_COND) $(TEST_M2_ARITH) $(TEST_M2_ADDR) $(TEST_M2_COMPILER) $(TEST_M2_PROCESS)
	./$(TEST_M0)
	./$(TEST_M1)
	./$(TEST_M2)
	./$(TEST_M2_FS)
	./$(TEST_M2_PRG)
	./$(TEST_M2_EXEC)
	./$(TEST_M2_E2E)
	./$(TEST_M2_COND)
	./$(TEST_M2_ARITH)
	./$(TEST_M2_ADDR)
	./$(TEST_M2_COMPILER)
	./$(TEST_M2_PROCESS)
	@echo "M2.16 host regression + Pexec lifecycle checks: PASS"

cross-check: $(CROSS_PRG) $(STRESS_PRG) $(M213_PRG) $(M213_INDEX_PRG) $(M214_PRG) $(M215_PRG) $(TEST_M2_CROSS)
	@echo "--- M2.11 GCC-generated m68k code ---"
	$(CROSS_OBJDUMP) -dr $(CROSS_OBJ)
	./$(TEST_M2_CROSS) $(CROSS_PRG) 42
	@echo "--- M2.12 linked GCC compiler-stress code ---"
	$(CROSS_OBJDUMP) -dr $(STRESS_ELF)
	./$(TEST_M2_CROSS) $(STRESS_PRG) 42
	@echo "--- M2.13 GCC MOVEM code ---"
	$(CROSS_OBJDUMP) -dr $(M213_ELF)
	./$(TEST_M2_CROSS) $(M213_PRG) 42
	@echo "--- M2.13 deterministic 68000 indexed-EA code ---"
	$(CROSS_OBJDUMP) -dr $(M213_INDEX_OBJ)
	./$(TEST_M2_CROSS) $(M213_INDEX_PRG) 42
	@echo "--- M2.15 GCC direct PC-relative .data/.bss code ---"
	$(CROSS_OBJDUMP) -dr $(M214_ELF)
	./$(TEST_M2_CROSS) $(M214_PRG) 42
	@echo "--- M2.15 linked absolute-long relocation code ---"
	$(CROSS_OBJDUMP) -dr $(M215_ELF)
	./$(TEST_M2_CROSS) $(M215_PRG) 42
	@echo "M2.16 cross regressions through M2.15: PASS"

clean:
	rm -rf $(BUILD)
