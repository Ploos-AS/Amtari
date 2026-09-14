/* M2.13 execution extension layer.
 *
 * Keep the M2.12 core intact while adding the 68000 brief indexed EA mode
 * and MOVEM.  The core symbols are renamed locally, then the public step/run
 * entry points route M2.13 opcodes through the extension first.
 */
#ifndef AMTARI_M213_STEP_NAME
#define AMTARI_M213_STEP_NAME amtari_exec_step
#endif
#ifndef AMTARI_M213_RUN_NAME
#define AMTARI_M213_RUN_NAME amtari_exec_run
#endif

#define amtari_exec_step amtari_exec_step_m212
#define amtari_exec_run amtari_exec_run_m212
#include "exec.c"
#undef amtari_exec_step
#undef amtari_exec_run

static uint32_t m213_reg_get(const struct amtari_context *ctx, unsigned int logical)
{
    return (logical < 8u) ? ctx->cpu.d[logical] : ctx->cpu.a[logical - 8u];
}

static void m213_reg_set(struct amtari_context *ctx, unsigned int logical, uint32_t value)
{
    if (logical < 8u) ctx->cpu.d[logical] = value;
    else ctx->cpu.a[logical - 8u] = value;
}

static int m213_indexed_address(struct amtari_context *ctx, uint32_t base,
                                uint16_t ext, uint32_t *address)
{
    unsigned int index_reg;
    uint32_t index;
    int32_t displacement;

    if ((ext & 0x0700u) != 0u) return AMTARI_EILLEGAL;
    index_reg = (unsigned int)((ext >> 12) & 7u);
    index = (ext & 0x8000u) ? ctx->cpu.a[index_reg] : ctx->cpu.d[index_reg];
    if ((ext & 0x0800u) == 0u) index = (uint32_t)(int32_t)(int16_t)index;
    displacement = (int32_t)(int8_t)(ext & 0xffu);
    *address = (uint32_t)((int32_t)base + (int32_t)index + displacement);
    return 0;
}

static int m213_indexed_ea(struct amtari_context *ctx, unsigned int mode,
                           unsigned int reg, uint32_t *ext_pc, uint32_t *address)
{
    uint16_t ext;
    uint32_t base;
    if (mode == 6u) base = ctx->cpu.a[reg];
    else if (mode == 7u && reg == 3u) base = *ext_pc;
    else return AMTARI_EILLEGAL;
    if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
    *ext_pc += 2u;
    return m213_indexed_address(ctx, base, ext, address);
}

static int m213_ea_read(struct amtari_context *ctx, unsigned int mode, unsigned int reg,
                        unsigned int size, uint32_t *ext_pc, uint32_t *value)
{
    uint32_t address;
    if (mode == 6u || (mode == 7u && reg == 3u)) {
        int rc = m213_indexed_ea(ctx, mode, reg, ext_pc, &address);
        if (rc != 0) return rc;
        return read_memory_value(ctx, address, size, value);
    }
    return ea_read(ctx, mode, reg, size, ext_pc, value);
}

static int m213_ea_write(struct amtari_context *ctx, unsigned int mode, unsigned int reg,
                         unsigned int size, uint32_t *ext_pc, uint32_t value)
{
    uint32_t address;
    if (mode == 6u) {
        int rc = m213_indexed_ea(ctx, mode, reg, ext_pc, &address);
        if (rc != 0) return rc;
        return write_memory_value(ctx, address, size, value);
    }
    if (mode == 7u && reg == 3u) return AMTARI_EILLEGAL;
    return ea_write(ctx, mode, reg, size, ext_pc, value);
}

static int m213_control_ea(struct amtari_context *ctx, unsigned int mode, unsigned int reg,
                           uint32_t *ext_pc, uint32_t *address)
{
    if (mode == 6u || (mode == 7u && reg == 3u))
        return m213_indexed_ea(ctx, mode, reg, ext_pc, address);
    return ea_control_address(ctx, mode, reg, ext_pc, address);
}

static int m213_movem_base(struct amtari_context *ctx, unsigned int mode, unsigned int reg,
                           uint32_t *ext_pc, uint32_t *address)
{
    uint16_t ext;
    if (mode == 2u || mode == 3u || mode == 4u) {
        *address = ctx->cpu.a[reg];
        return 0;
    }
    if (mode == 5u) {
        if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
        *ext_pc += 2u;
        *address = (uint32_t)((int32_t)ctx->cpu.a[reg] + (int32_t)(int16_t)ext);
        return 0;
    }
    if (mode == 6u || (mode == 7u && reg == 3u))
        return m213_indexed_ea(ctx, mode, reg, ext_pc, address);
    if (mode == 7u && reg == 0u) {
        if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
        *ext_pc += 2u;
        *address = (uint32_t)(int32_t)(int16_t)ext;
        return 0;
    }
    if (mode == 7u && reg == 1u) {
        if (amtari_guest_read32(ctx, *ext_pc, address) != 0) return AMTARI_EFAULT;
        *ext_pc += 4u;
        return 0;
    }
    if (mode == 7u && reg == 2u) {
        uint32_t base = *ext_pc;
        if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
        *ext_pc += 2u;
        *address = (uint32_t)((int32_t)base + (int32_t)(int16_t)ext);
        return 0;
    }
    return AMTARI_EILLEGAL;
}

static int m213_movem(struct amtari_context *ctx, uint16_t opcode, uint32_t pc)
{
    unsigned int mode, reg, size, direction, bit;
    uint16_t mask;
    uint32_t ext_pc, address;
    int rc;

    if ((opcode & 0xfb80u) != 0x4880u) return AMTARI_EILLEGAL;
    mode = (unsigned int)((opcode >> 3) & 7u);
    reg = opcode & 7u;
    if (mode == 0u || mode == 1u) return AMTARI_EILLEGAL;
    direction = (opcode & 0x0400u) != 0u;
    size = (opcode & 0x0040u) ? 4u : 2u;
    if (!direction && mode == 3u) return AMTARI_EILLEGAL;
    if (direction && mode == 4u) return AMTARI_EILLEGAL;
    if (amtari_guest_read16(ctx, pc + 2u, &mask) != 0) return AMTARI_EFAULT;
    ext_pc = pc + 4u;
    rc = m213_movem_base(ctx, mode, reg, &ext_pc, &address);
    if (rc != 0) return rc;

    if (!direction && mode == 4u) {
        for (bit = 0u; bit < 16u; ++bit) {
            if ((mask & (uint16_t)(1u << bit)) != 0u) {
                unsigned int logical = 15u - bit;
                uint32_t value = m213_reg_get(ctx, logical);
                if (address < size) return AMTARI_EFAULT;
                address -= size;
                rc = write_memory_value(ctx, address, size, value);
                if (rc != 0) return rc;
            }
        }
        ctx->cpu.a[reg] = address;
    } else if (!direction) {
        for (bit = 0u; bit < 16u; ++bit) {
            if ((mask & (uint16_t)(1u << bit)) != 0u) {
                rc = write_memory_value(ctx, address, size, m213_reg_get(ctx, bit));
                if (rc != 0) return rc;
                address += size;
            }
        }
    } else {
        for (bit = 0u; bit < 16u; ++bit) {
            if ((mask & (uint16_t)(1u << bit)) != 0u) {
                uint32_t value;
                rc = read_memory_value(ctx, address, size, &value);
                if (rc != 0) return rc;
                if (size == 2u) value = (uint32_t)(int32_t)(int16_t)value;
                m213_reg_set(ctx, bit, value);
                address += size;
            }
        }
        if (mode == 3u) ctx->cpu.a[reg] = address;
    }
    ctx->cpu.pc = ext_pc;
    return AMTARI_EXEC_RUNNING;
}

static int m213_indexed_instruction(struct amtari_context *ctx, uint16_t opcode, uint32_t pc)
{
    unsigned int mode = (unsigned int)((opcode >> 3) & 7u);
    unsigned int reg = opcode & 7u;
    int source_indexed = (mode == 6u || (mode == 7u && reg == 3u));

    if ((opcode & 0xc000u) == 0u && (opcode & 0x3000u) != 0u) {
        unsigned int top = (unsigned int)((opcode >> 12) & 3u);
        unsigned int size = (top == 1u) ? 1u : (top == 2u) ? 4u : 2u;
        unsigned int dst_mode = (unsigned int)((opcode >> 6) & 7u);
        unsigned int dst_reg = (unsigned int)((opcode >> 9) & 7u);
        int dst_indexed = dst_mode == 6u;
        uint32_t ext_pc, value;
        int rc;
        if (!source_indexed && !dst_indexed) return AMTARI_EILLEGAL;
        ext_pc = pc + 2u;
        rc = m213_ea_read(ctx, mode, reg, size, &ext_pc, &value);
        if (rc != 0) return rc;
        if (dst_mode == 1u) {
            if (size == 1u) return AMTARI_EILLEGAL;
            ctx->cpu.a[dst_reg] = (size == 2u) ? (uint32_t)(int32_t)(int16_t)value : value;
        } else {
            rc = m213_ea_write(ctx, dst_mode, dst_reg, size, &ext_pc, value);
            if (rc != 0) return rc;
            set_nz(ctx, value, size);
        }
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xffc0u) == 0x4840u && source_indexed) {
        uint32_t ext_pc = pc + 2u, address;
        int rc = m213_control_ea(ctx, mode, reg, &ext_pc, &address);
        if (rc != 0) return rc;
        if (push32(ctx, address) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }
    if ((opcode & 0xf1c0u) == 0x41c0u && source_indexed) {
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t ext_pc = pc + 2u, address;
        int rc = m213_control_ea(ctx, mode, reg, &ext_pc, &address);
        if (rc != 0) return rc;
        ctx->cpu.a[dst] = address;
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }
    if ((opcode & 0xffc0u) == 0x4e80u && source_indexed) {
        uint32_t ext_pc = pc + 2u, target;
        int rc = m213_control_ea(ctx, mode, reg, &ext_pc, &target);
        if (rc != 0) return rc;
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        if (push32(ctx, ext_pc) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }
    if ((opcode & 0xff00u) == 0x4a00u && source_indexed) {
        unsigned int size;
        uint32_t ext_pc = pc + 2u, value;
        int rc;
        if (decode_size_bits(opcode, &size) != 0) return AMTARI_EILLEGAL;
        rc = m213_ea_read(ctx, mode, reg, size, &ext_pc, &value);
        if (rc != 0) return rc;
        set_nz(ctx, value, size);
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }
    if (source_indexed && ((opcode & 0xf000u) == 0xb000u ||
                           (opcode & 0xf000u) == 0x8000u ||
                           (opcode & 0xf000u) == 0xc000u ||
                           (opcode & 0xf000u) == 0xd000u ||
                           (opcode & 0xf000u) == 0x9000u)) {
        unsigned int family = opcode & 0xf000u;
        unsigned int opmode = (unsigned int)((opcode >> 6) & 7u);
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t ext_pc = pc + 2u, src, value;
        unsigned int size;
        int rc;
        if ((family == 0xd000u || family == 0x9000u) && (opmode == 3u || opmode == 7u)) {
            size = (opmode == 3u) ? 2u : 4u;
            rc = m213_ea_read(ctx, mode, reg, size, &ext_pc, &src);
            if (rc != 0) return rc;
            if (size == 2u) src = (uint32_t)(int32_t)(int16_t)src;
            if (family == 0xd000u) ctx->cpu.a[dst] += src;
            else ctx->cpu.a[dst] -= src;
            ctx->cpu.pc = ext_pc;
            return AMTARI_EXEC_RUNNING;
        }
        if (opmode > 2u) return AMTARI_EILLEGAL;
        size = (opmode == 0u) ? 1u : (opmode == 1u) ? 2u : 4u;
        rc = m213_ea_read(ctx, mode, reg, size, &ext_pc, &src);
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

int AMTARI_M213_STEP_NAME(struct amtari_context *ctx)
{
    uint16_t opcode;
    uint32_t pc;
    int rc;
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    pc = ctx->cpu.pc;
    if (amtari_guest_read16(ctx, pc, &opcode) != 0) return AMTARI_EFAULT;
    rc = m213_movem(ctx, opcode, pc);
    if (rc != AMTARI_EILLEGAL) return rc;
    rc = m213_indexed_instruction(ctx, opcode, pc);
    if (rc != AMTARI_EILLEGAL) return rc;
    return amtari_exec_step_m212(ctx);
}

int AMTARI_M213_RUN_NAME(struct amtari_context *ctx, uint32_t max_steps, uint32_t *steps_executed)
{
    uint32_t steps = 0u;
    if (ctx == 0 || max_steps == 0u) return AMTARI_EINVAL;
    while (steps < max_steps) {
        int rc = AMTARI_M213_STEP_NAME(ctx);
        ++steps;
        if (rc != AMTARI_EXEC_RUNNING) {
            if (steps_executed != 0) *steps_executed = steps;
            return rc;
        }
    }
    if (steps_executed != 0) *steps_executed = steps;
    return AMTARI_EXEC_RUNNING;
}
