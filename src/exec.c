#include <string.h>

#include "amtari.h"

static int guest_write32(struct amtari_context *ctx, uint32_t address, uint32_t value)
{
    uint8_t *p;

    if (!amtari_guest_range_valid(ctx, address, 4u)) {
        return AMTARI_EFAULT;
    }
    p = &ctx->memory.data[address];
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
    return 0;
}

int amtari_exec_prepare(struct amtari_context *ctx, uint32_t basepage, uint32_t stack_top)
{
    uint32_t tbase;

    if (ctx == 0 || !ctx->initialized || ctx->memory.data == 0) {
        return AMTARI_EINVAL;
    }
    if (amtari_guest_read32(ctx, basepage + 0x08u, &tbase) != 0) {
        return AMTARI_EFAULT;
    }
    if (!amtari_guest_range_valid(ctx, tbase, 2u) || stack_top < 4u ||
        !amtari_guest_range_valid(ctx, stack_top - 4u, 4u)) {
        return AMTARI_EFAULT;
    }

    memset(&ctx->cpu, 0, sizeof(ctx->cpu));
    ctx->current_basepage = basepage;
    ctx->cpu.a[0] = basepage;
    ctx->cpu.a[7] = stack_top - 4u;
    ctx->cpu.pc = tbase;
    ctx->cpu.sr = 0u;

    return guest_write32(ctx, ctx->cpu.a[7], 0u);
}

int amtari_exec_step(struct amtari_context *ctx)
{
    uint16_t opcode;

    if (ctx == 0 || !ctx->initialized) {
        return AMTARI_EINVAL;
    }
    if (amtari_guest_read16(ctx, ctx->cpu.pc, &opcode) != 0) {
        return AMTARI_EFAULT;
    }

    /* NOP */
    if (opcode == 0x4e71u) {
        ctx->cpu.pc += 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* RTS. A zero return address is our host handoff sentinel. */
    if (opcode == 0x4e75u) {
        uint32_t target;
        if (amtari_guest_read32(ctx, ctx->cpu.a[7], &target) != 0) {
            return AMTARI_EFAULT;
        }
        ctx->cpu.a[7] += 4u;
        if (target == 0u) {
            return AMTARI_EXEC_HALTED;
        }
        if (!amtari_guest_range_valid(ctx, target, 2u)) {
            return AMTARI_EFAULT;
        }
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVEQ #imm8,Dn */
    if ((opcode & 0xf100u) == 0x7000u) {
        unsigned int reg = (unsigned int)((opcode >> 9) & 7u);
        int32_t value = (int32_t)(int8_t)(opcode & 0xffu);
        ctx->cpu.d[reg] = (uint32_t)value;
        ctx->cpu.pc += 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* TRAP #1: GEMDOS. The function word is at the guest stack pointer. */
    if (opcode == 0x4e41u) {
        uint16_t function;
        int32_t result;
        if (amtari_guest_read16(ctx, ctx->cpu.a[7], &function) != 0) {
            return AMTARI_EFAULT;
        }
        result = amtari_gemdos_dispatch(ctx, function);
        ctx->cpu.d[0] = (uint32_t)result;
        ctx->cpu.pc += 2u;
        return AMTARI_EXEC_RUNNING;
    }

    return AMTARI_EILLEGAL;
}

int amtari_exec_run(struct amtari_context *ctx, uint32_t max_steps, uint32_t *steps_executed)
{
    uint32_t steps = 0u;

    if (ctx == 0 || max_steps == 0u) {
        return AMTARI_EINVAL;
    }

    while (steps < max_steps) {
        int rc = amtari_exec_step(ctx);
        ++steps;
        if (rc != AMTARI_EXEC_RUNNING) {
            if (steps_executed != 0) {
                *steps_executed = steps;
            }
            return rc;
        }
    }

    if (steps_executed != 0) {
        *steps_executed = steps;
    }
    return AMTARI_EXEC_RUNNING;
}
