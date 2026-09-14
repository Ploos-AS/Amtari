/* M2.15 execution extension layer: add d16(PC) data effective addressing. */
#define amtari_exec_step amtari_exec_step_m213
#define amtari_exec_run amtari_exec_run_m213
#include "exec_m213.c"
#undef amtari_exec_step
#undef amtari_exec_run

static int m215_pc_relative_read(struct amtari_context *ctx, unsigned int size,
                                 uint32_t *ext_pc, uint32_t *value)
{
    uint16_t ext;
    uint32_t base = *ext_pc;
    uint32_t address;
    if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
    *ext_pc += 2u;
    address = (uint32_t)((int32_t)base + (int32_t)(int16_t)ext);
    return read_memory_value(ctx, address, size, value);
}

static int m215_pc_relative_instruction(struct amtari_context *ctx, uint16_t opcode, uint32_t pc)
{
    unsigned int mode = (unsigned int)((opcode >> 3) & 7u);
    unsigned int reg = opcode & 7u;

    if (mode != 7u || reg != 2u) return AMTARI_EILLEGAL;

    if ((opcode & 0xc000u) == 0u && (opcode & 0x3000u) != 0u) {
        unsigned int top = (unsigned int)((opcode >> 12) & 3u);
        unsigned int size = (top == 1u) ? 1u : (top == 2u) ? 4u : 2u;
        unsigned int dst_mode = (unsigned int)((opcode >> 6) & 7u);
        unsigned int dst_reg = (unsigned int)((opcode >> 9) & 7u);
        uint32_t ext_pc = pc + 2u, value;
        int rc = m215_pc_relative_read(ctx, size, &ext_pc, &value);
        if (rc != 0) return rc;
        if (dst_mode == 1u) {
            if (size == 1u) return AMTARI_EILLEGAL;
            ctx->cpu.a[dst_reg] = (size == 2u) ? (uint32_t)(int32_t)(int16_t)value : value;
        } else {
            rc = ea_write(ctx, dst_mode, dst_reg, size, &ext_pc, value);
            if (rc != 0) return rc;
            set_nz(ctx, value, size);
        }
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xff00u) == 0x4a00u) {
        unsigned int size;
        uint32_t ext_pc = pc + 2u, value;
        int rc;
        if (decode_size_bits(opcode, &size) != 0) return AMTARI_EILLEGAL;
        rc = m215_pc_relative_read(ctx, size, &ext_pc, &value);
        if (rc != 0) return rc;
        set_nz(ctx, value, size);
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf000u) == 0xb000u || (opcode & 0xf000u) == 0x8000u ||
        (opcode & 0xf000u) == 0xc000u || (opcode & 0xf000u) == 0xd000u ||
        (opcode & 0xf000u) == 0x9000u) {
        unsigned int family = opcode & 0xf000u;
        unsigned int opmode = (unsigned int)((opcode >> 6) & 7u);
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t ext_pc = pc + 2u, src, value;
        unsigned int size;
        int rc;
        if ((family == 0xd000u || family == 0x9000u) && (opmode == 3u || opmode == 7u)) {
            size = (opmode == 3u) ? 2u : 4u;
            rc = m215_pc_relative_read(ctx, size, &ext_pc, &src);
            if (rc != 0) return rc;
            if (size == 2u) src = (uint32_t)(int32_t)(int16_t)src;
            if (family == 0xd000u) ctx->cpu.a[dst] += src;
            else ctx->cpu.a[dst] -= src;
            ctx->cpu.pc = ext_pc;
            return AMTARI_EXEC_RUNNING;
        }
        if (opmode > 2u) return AMTARI_EILLEGAL;
        size = (opmode == 0u) ? 1u : (opmode == 1u) ? 2u : 4u;
        rc = m215_pc_relative_read(ctx, size, &ext_pc, &src);
        if (rc != 0) return rc;
        if (family == 0xb000u) set_cmp(ctx, dreg_value(ctx, dst, size), src, size);
        else if (family == 0x8000u || family == 0xc000u) {
            value = (family == 0x8000u) ? (dreg_value(ctx, dst, size) | src) :
                                         (dreg_value(ctx, dst, size) & src);
            dreg_write(ctx, dst, size, value);
            set_nz(ctx, value, size);
        } else {
            if (size != 4u) return AMTARI_EILLEGAL;
            value = (family == 0xd000u) ? add32(ctx, ctx->cpu.d[dst], src) :
                                         sub32(ctx, ctx->cpu.d[dst], src);
            ctx->cpu.d[dst] = value;
        }
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    return AMTARI_EILLEGAL;
}

int amtari_exec_step(struct amtari_context *ctx)
{
    uint16_t opcode;
    uint32_t pc;
    int rc;
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    pc = ctx->cpu.pc;
    if (amtari_guest_read16(ctx, pc, &opcode) != 0) return AMTARI_EFAULT;
    rc = m215_pc_relative_instruction(ctx, opcode, pc);
    if (rc != AMTARI_EILLEGAL) return rc;
    return amtari_exec_step_m213(ctx);
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
