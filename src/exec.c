#include <string.h>

#include "amtari.h"

static int guest_write16(struct amtari_context *ctx, uint32_t address, uint16_t value)
{
    uint8_t *p;
    if (!amtari_guest_range_valid(ctx, address, 2u)) return AMTARI_EFAULT;
    p = &ctx->memory.data[address];
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
    return 0;
}

static int guest_write32(struct amtari_context *ctx, uint32_t address, uint32_t value)
{
    uint8_t *p;
    if (!amtari_guest_range_valid(ctx, address, 4u)) return AMTARI_EFAULT;
    p = &ctx->memory.data[address];
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
    return 0;
}

static int push16(struct amtari_context *ctx, uint16_t value)
{
    if (ctx->cpu.a[7] < 2u) return AMTARI_EFAULT;
    ctx->cpu.a[7] -= 2u;
    return guest_write16(ctx, ctx->cpu.a[7], value);
}

static int push32(struct amtari_context *ctx, uint32_t value)
{
    if (ctx->cpu.a[7] < 4u) return AMTARI_EFAULT;
    ctx->cpu.a[7] -= 4u;
    return guest_write32(ctx, ctx->cpu.a[7], value);
}

int amtari_exec_prepare(struct amtari_context *ctx, uint32_t basepage, uint32_t stack_top)
{
    uint32_t tbase;
    if (ctx == 0 || !ctx->initialized || ctx->memory.data == 0) return AMTARI_EINVAL;
    if (amtari_guest_read32(ctx, basepage + 0x08u, &tbase) != 0) return AMTARI_EFAULT;
    if (!amtari_guest_range_valid(ctx, tbase, 2u) || stack_top < 4u ||
        !amtari_guest_range_valid(ctx, stack_top - 4u, 4u)) return AMTARI_EFAULT;

    memset(&ctx->cpu, 0, sizeof(ctx->cpu));
    ctx->current_basepage = basepage;
    ctx->cpu.a[0] = basepage;
    ctx->cpu.a[7] = stack_top - 4u;
    ctx->cpu.pc = tbase;
    ctx->cpu.sr = 0u;
    return guest_write32(ctx, ctx->cpu.a[7], 0u);
}

static int branch_target_valid(struct amtari_context *ctx, uint32_t target)
{
    return amtari_guest_range_valid(ctx, target, 2u) ? 0 : AMTARI_EFAULT;
}

int amtari_exec_step(struct amtari_context *ctx)
{
    uint16_t opcode;
    uint32_t pc;

    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    pc = ctx->cpu.pc;
    if (amtari_guest_read16(ctx, pc, &opcode) != 0) return AMTARI_EFAULT;

    /* NOP */
    if (opcode == 0x4e71u) { ctx->cpu.pc = pc + 2u; return AMTARI_EXEC_RUNNING; }

    /* RTS */
    if (opcode == 0x4e75u) {
        uint32_t target;
        if (amtari_guest_read32(ctx, ctx->cpu.a[7], &target) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[7] += 4u;
        if (target == 0u) return AMTARI_EXEC_HALTED;
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVEQ #imm8,Dn */
    if ((opcode & 0xf100u) == 0x7000u) {
        unsigned int reg = (unsigned int)((opcode >> 9) & 7u);
        ctx->cpu.d[reg] = (uint32_t)(int32_t)(int8_t)(opcode & 0xffu);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.W #imm,-(SP) */
    if (opcode == 0x3f3cu) {
        uint16_t value;
        if (amtari_guest_read16(ctx, pc + 2u, &value) != 0) return AMTARI_EFAULT;
        if (push16(ctx, value) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L #imm,-(SP) */
    if (opcode == 0x2f3cu) {
        uint32_t value;
        if (amtari_guest_read32(ctx, pc + 2u, &value) != 0) return AMTARI_EFAULT;
        if (push32(ctx, value) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = pc + 6u;
        return AMTARI_EXEC_RUNNING;
    }

    /* ADDQ.[W/L] #n,An. Size does not affect address-register result. */
    if ((opcode & 0xf0f8u) == 0x5048u || (opcode & 0xf0f8u) == 0x5088u) {
        unsigned int reg = opcode & 7u;
        uint32_t amount = (uint32_t)((opcode >> 9) & 7u);
        if (amount == 0u) amount = 8u;
        ctx->cpu.a[reg] += amount;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* SUBQ.[W/L] #n,An. */
    if ((opcode & 0xf0f8u) == 0x5148u || (opcode & 0xf0f8u) == 0x5188u) {
        unsigned int reg = opcode & 7u;
        uint32_t amount = (uint32_t)((opcode >> 9) & 7u);
        if (amount == 0u) amount = 8u;
        ctx->cpu.a[reg] -= amount;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* BRA/BSR with 8-bit or 16-bit displacement. */
    if ((opcode & 0xff00u) == 0x6000u || (opcode & 0xff00u) == 0x6100u) {
        int32_t displacement;
        uint32_t next_pc;
        uint32_t target;
        uint8_t low = (uint8_t)opcode;
        int is_bsr = (opcode & 0xff00u) == 0x6100u;

        if (low == 0u) {
            uint16_t ext;
            if (amtari_guest_read16(ctx, pc + 2u, &ext) != 0) return AMTARI_EFAULT;
            displacement = (int32_t)(int16_t)ext;
            next_pc = pc + 4u;
        } else {
            displacement = (int32_t)(int8_t)low;
            next_pc = pc + 2u;
        }
        target = (uint32_t)((int32_t)next_pc + displacement);
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        if (is_bsr && push32(ctx, next_pc) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    /* JSR absolute long. */
    if (opcode == 0x4eb9u) {
        uint32_t target;
        if (amtari_guest_read32(ctx, pc + 2u, &target) != 0) return AMTARI_EFAULT;
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        if (push32(ctx, pc + 6u) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    /* TRAP #1: GEMDOS. Function word remains at guest SP for argument decoding. */
    if (opcode == 0x4e41u) {
        uint16_t function;
        int32_t result;
        if (amtari_guest_read16(ctx, ctx->cpu.a[7], &function) != 0) return AMTARI_EFAULT;
        result = amtari_gemdos_dispatch(ctx, function);
        ctx->cpu.d[0] = (uint32_t)result;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    return AMTARI_EILLEGAL;
}

int amtari_exec_run(struct amtari_context *ctx, uint32_t max_steps, uint32_t *steps_executed)
{
    uint32_t steps = 0u;
    if (ctx == 0 || max_steps == 0u) return AMTARI_EINVAL;
    while (steps < max_steps) {
        int rc = amtari_exec_step(ctx);
        ++steps;
        if (rc != AMTARI_EXEC_RUNNING) {
            if (steps_executed != 0) *steps_executed = steps;
            return rc;
        }
    }
    if (steps_executed != 0) *steps_executed = steps;
    return AMTARI_EXEC_RUNNING;
}
