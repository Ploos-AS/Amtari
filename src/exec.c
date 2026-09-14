#include <string.h>

#include "amtari.h"

#define CCR_X 0x10u
#define CCR_N 0x08u
#define CCR_Z 0x04u
#define CCR_V 0x02u
#define CCR_C 0x01u

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
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
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

static int pop32(struct amtari_context *ctx, uint32_t *value)
{
    if (amtari_guest_read32(ctx, ctx->cpu.a[7], value) != 0) return AMTARI_EFAULT;
    ctx->cpu.a[7] += 4u;
    return 0;
}

static void set_nz32(struct amtari_context *ctx, uint32_t value)
{
    uint16_t ccr = (uint16_t)(ctx->cpu.sr & CCR_X);
    if (value == 0u) ccr |= CCR_Z;
    if ((value & 0x80000000u) != 0u) ccr |= CCR_N;
    ctx->cpu.sr = (uint16_t)((ctx->cpu.sr & 0xff00u) | ccr);
}

static void set_cmp32(struct amtari_context *ctx, uint32_t dst, uint32_t src)
{
    uint32_t result = dst - src;
    uint16_t ccr = (uint16_t)(ctx->cpu.sr & CCR_X);
    if (result == 0u) ccr |= CCR_Z;
    if ((result & 0x80000000u) != 0u) ccr |= CCR_N;
    if ((((dst ^ src) & (dst ^ result)) & 0x80000000u) != 0u) ccr |= CCR_V;
    if (src > dst) ccr |= CCR_C;
    ctx->cpu.sr = (uint16_t)((ctx->cpu.sr & 0xff00u) | ccr);
}

static uint32_t add32(struct amtari_context *ctx, uint32_t dst, uint32_t src)
{
    uint32_t result = dst + src;
    uint16_t ccr = 0u;
    int carry = result < dst;
    int overflow = (((~(dst ^ src)) & (dst ^ result)) & 0x80000000u) != 0u;
    if (result == 0u) ccr |= CCR_Z;
    if ((result & 0x80000000u) != 0u) ccr |= CCR_N;
    if (overflow) ccr |= CCR_V;
    if (carry) ccr |= (CCR_C | CCR_X);
    ctx->cpu.sr = (uint16_t)((ctx->cpu.sr & 0xff00u) | ccr);
    return result;
}

static uint32_t sub32(struct amtari_context *ctx, uint32_t dst, uint32_t src)
{
    uint32_t result = dst - src;
    uint16_t ccr = 0u;
    int borrow = src > dst;
    int overflow = ((((dst ^ src) & (dst ^ result)) & 0x80000000u) != 0u);
    if (result == 0u) ccr |= CCR_Z;
    if ((result & 0x80000000u) != 0u) ccr |= CCR_N;
    if (overflow) ccr |= CCR_V;
    if (borrow) ccr |= (CCR_C | CCR_X);
    ctx->cpu.sr = (uint16_t)((ctx->cpu.sr & 0xff00u) | ccr);
    return result;
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

static int branch_displacement(struct amtari_context *ctx, uint32_t pc, uint16_t opcode,
                               int32_t *displacement, uint32_t *next_pc)
{
    uint8_t low = (uint8_t)opcode;
    if (low == 0u) {
        uint16_t ext;
        if (amtari_guest_read16(ctx, pc + 2u, &ext) != 0) return AMTARI_EFAULT;
        *displacement = (int32_t)(int16_t)ext;
        *next_pc = pc + 4u;
    } else {
        *displacement = (int32_t)(int8_t)low;
        *next_pc = pc + 2u;
    }
    return 0;
}

static int condition_true(const struct amtari_context *ctx, unsigned int condition)
{
    int n = (ctx->cpu.sr & CCR_N) != 0u;
    int z = (ctx->cpu.sr & CCR_Z) != 0u;
    int v = (ctx->cpu.sr & CCR_V) != 0u;
    switch (condition) {
    case 6u: return !z;
    case 7u: return z;
    case 12u: return n == v;
    case 13u: return n != v;
    case 14u: return !z && n == v;
    case 15u: return z || n != v;
    default: return 0;
    }
}

static int displacement_address(struct amtari_context *ctx, uint32_t pc, unsigned int reg,
                                uint32_t *address)
{
    uint16_t ext;
    if (amtari_guest_read16(ctx, pc + 2u, &ext) != 0) return AMTARI_EFAULT;
    *address = (uint32_t)((int32_t)ctx->cpu.a[reg] + (int32_t)(int16_t)ext);
    return 0;
}

int amtari_exec_step(struct amtari_context *ctx)
{
    uint16_t opcode;
    uint32_t pc;
    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    pc = ctx->cpu.pc;
    if (amtari_guest_read16(ctx, pc, &opcode) != 0) return AMTARI_EFAULT;

    if (opcode == 0x4e71u) { ctx->cpu.pc = pc + 2u; return AMTARI_EXEC_RUNNING; }

    if (opcode == 0x4e75u) {
        uint32_t target;
        if (pop32(ctx, &target) != 0) return AMTARI_EFAULT;
        if (target == 0u) return AMTARI_EXEC_HALTED;
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    /* LINK An,#disp16 */
    if ((opcode & 0xfff8u) == 0x4e50u) {
        unsigned int reg = opcode & 7u;
        uint16_t ext;
        if (amtari_guest_read16(ctx, pc + 2u, &ext) != 0) return AMTARI_EFAULT;
        if (push32(ctx, ctx->cpu.a[reg]) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[reg] = ctx->cpu.a[7];
        ctx->cpu.a[7] = (uint32_t)((int32_t)ctx->cpu.a[7] + (int32_t)(int16_t)ext);
        if (!amtari_guest_range_valid(ctx, ctx->cpu.a[7], 1u)) return AMTARI_EFAULT;
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    /* UNLK An */
    if ((opcode & 0xfff8u) == 0x4e58u) {
        unsigned int reg = opcode & 7u;
        uint32_t old_a;
        ctx->cpu.a[7] = ctx->cpu.a[reg];
        if (pop32(ctx, &old_a) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[reg] = old_a;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* LEA d16(An),Am */
    if ((opcode & 0xf1f8u) == 0x41e8u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t address;
        if (displacement_address(ctx, pc, src, &address) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[dst] = address;
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVEA.L #imm,An */
    if ((opcode & 0xf1ffu) == 0x207cu) {
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (amtari_guest_read32(ctx, pc + 2u, &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[dst] = value;
        ctx->cpu.pc = pc + 6u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVEA.L Dn,An */
    if ((opcode & 0xf1f8u) == 0x2040u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        ctx->cpu.a[dst] = ctx->cpu.d[src];
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVEA.L (An),Am */
    if ((opcode & 0xf1f8u) == 0x2050u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (amtari_guest_read32(ctx, ctx->cpu.a[src], &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[dst] = value;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVEA.L d16(An),Am */
    if ((opcode & 0xf1f8u) == 0x2068u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t address, value;
        if (displacement_address(ctx, pc, src, &address) != 0) return AMTARI_EFAULT;
        if (amtari_guest_read32(ctx, address, &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[dst] = value;
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVEQ #imm8,Dn */
    if ((opcode & 0xf100u) == 0x7000u) {
        unsigned int reg = (unsigned int)((opcode >> 9) & 7u);
        ctx->cpu.d[reg] = (uint32_t)(int32_t)(int8_t)(opcode & 0xffu);
        set_nz32(ctx, ctx->cpu.d[reg]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L #imm,Dn */
    if ((opcode & 0xf1ffu) == 0x203cu) {
        unsigned int reg = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (amtari_guest_read32(ctx, pc + 2u, &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.d[reg] = value;
        set_nz32(ctx, value);
        ctx->cpu.pc = pc + 6u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L Dm,Dn */
    if ((opcode & 0xf1f8u) == 0x2000u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        ctx->cpu.d[dst] = ctx->cpu.d[src];
        set_nz32(ctx, ctx->cpu.d[dst]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L (An),Dn */
    if ((opcode & 0xf1f8u) == 0x2010u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (amtari_guest_read32(ctx, ctx->cpu.a[src], &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.d[dst] = value;
        set_nz32(ctx, value);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L (An)+,Dn */
    if ((opcode & 0xf1f8u) == 0x2018u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (amtari_guest_read32(ctx, ctx->cpu.a[src], &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[src] += 4u;
        ctx->cpu.d[dst] = value;
        set_nz32(ctx, value);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L -(An),Dn */
    if ((opcode & 0xf1f8u) == 0x2020u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (ctx->cpu.a[src] < 4u) return AMTARI_EFAULT;
        ctx->cpu.a[src] -= 4u;
        if (amtari_guest_read32(ctx, ctx->cpu.a[src], &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.d[dst] = value;
        set_nz32(ctx, value);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L d16(An),Dn */
    if ((opcode & 0xf1f8u) == 0x2028u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t address, value;
        if (displacement_address(ctx, pc, src, &address) != 0) return AMTARI_EFAULT;
        if (amtari_guest_read32(ctx, address, &value) != 0) return AMTARI_EFAULT;
        ctx->cpu.d[dst] = value;
        set_nz32(ctx, value);
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L Dm,(An) */
    if ((opcode & 0xf1f8u) == 0x2080u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        if (guest_write32(ctx, ctx->cpu.a[dst], ctx->cpu.d[src]) != 0) return AMTARI_EFAULT;
        set_nz32(ctx, ctx->cpu.d[src]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L Dm,(An)+ */
    if ((opcode & 0xf1f8u) == 0x20c0u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        if (guest_write32(ctx, ctx->cpu.a[dst], ctx->cpu.d[src]) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[dst] += 4u;
        set_nz32(ctx, ctx->cpu.d[src]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L Dm,-(An) */
    if ((opcode & 0xf1f8u) == 0x2100u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        if (ctx->cpu.a[dst] < 4u) return AMTARI_EFAULT;
        ctx->cpu.a[dst] -= 4u;
        if (guest_write32(ctx, ctx->cpu.a[dst], ctx->cpu.d[src]) != 0) return AMTARI_EFAULT;
        set_nz32(ctx, ctx->cpu.d[src]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.L Dm,d16(An) */
    if ((opcode & 0xf1f8u) == 0x2140u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t address;
        if (displacement_address(ctx, pc, dst, &address) != 0) return AMTARI_EFAULT;
        if (guest_write32(ctx, address, ctx->cpu.d[src]) != 0) return AMTARI_EFAULT;
        set_nz32(ctx, ctx->cpu.d[src]);
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xfff8u) == 0x4a80u) {
        set_nz32(ctx, ctx->cpu.d[opcode & 7u]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xfff8u) == 0x0c80u) {
        uint32_t value;
        unsigned int reg = opcode & 7u;
        if (amtari_guest_read32(ctx, pc + 2u, &value) != 0) return AMTARI_EFAULT;
        set_cmp32(ctx, ctx->cpu.d[reg], value);
        ctx->cpu.pc = pc + 6u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1f8u) == 0xd080u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        ctx->cpu.d[dst] = add32(ctx, ctx->cpu.d[dst], ctx->cpu.d[src]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1f8u) == 0x9080u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        ctx->cpu.d[dst] = sub32(ctx, ctx->cpu.d[dst], ctx->cpu.d[src]);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if (opcode == 0x3f3cu) {
        uint16_t value;
        if (amtari_guest_read16(ctx, pc + 2u, &value) != 0) return AMTARI_EFAULT;
        if (push16(ctx, value) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    if (opcode == 0x2f3cu) {
        uint32_t value;
        if (amtari_guest_read32(ctx, pc + 2u, &value) != 0) return AMTARI_EFAULT;
        if (push32(ctx, value) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = pc + 6u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1f8u) == 0x5080u) {
        unsigned int reg = opcode & 7u;
        uint32_t amount = (uint32_t)((opcode >> 9) & 7u);
        if (amount == 0u) amount = 8u;
        ctx->cpu.d[reg] = add32(ctx, ctx->cpu.d[reg], amount);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1f8u) == 0x5180u) {
        unsigned int reg = opcode & 7u;
        uint32_t amount = (uint32_t)((opcode >> 9) & 7u);
        if (amount == 0u) amount = 8u;
        ctx->cpu.d[reg] = sub32(ctx, ctx->cpu.d[reg], amount);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1f8u) == 0x5048u || (opcode & 0xf1f8u) == 0x5088u) {
        unsigned int reg = opcode & 7u;
        uint32_t amount = (uint32_t)((opcode >> 9) & 7u);
        if (amount == 0u) amount = 8u;
        ctx->cpu.a[reg] += amount;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1f8u) == 0x5148u || (opcode & 0xf1f8u) == 0x5188u) {
        unsigned int reg = opcode & 7u;
        uint32_t amount = (uint32_t)((opcode >> 9) & 7u);
        if (amount == 0u) amount = 8u;
        ctx->cpu.a[reg] -= amount;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xfff8u) == 0x51c8u) {
        unsigned int reg = opcode & 7u;
        uint16_t ext;
        uint16_t low;
        uint32_t next_pc = pc + 4u;
        uint32_t target;
        if (amtari_guest_read16(ctx, pc + 2u, &ext) != 0) return AMTARI_EFAULT;
        low = (uint16_t)ctx->cpu.d[reg];
        low = (uint16_t)(low - 1u);
        ctx->cpu.d[reg] = (ctx->cpu.d[reg] & 0xffff0000u) | low;
        if (low == 0xffffu) {
            ctx->cpu.pc = next_pc;
            return AMTARI_EXEC_RUNNING;
        }
        target = (uint32_t)((int32_t)next_pc + (int32_t)(int16_t)ext);
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xff00u) == 0x6000u || (opcode & 0xff00u) == 0x6100u) {
        int32_t displacement;
        uint32_t next_pc, target;
        int is_bsr = (opcode & 0xff00u) == 0x6100u;
        if (branch_displacement(ctx, pc, opcode, &displacement, &next_pc) != 0) return AMTARI_EFAULT;
        target = (uint32_t)((int32_t)next_pc + displacement);
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        if (is_bsr && push32(ctx, next_pc) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf000u) == 0x6000u) {
        unsigned int condition = (unsigned int)((opcode >> 8) & 0x0fu);
        if (condition == 6u || condition == 7u || condition == 12u ||
            condition == 13u || condition == 14u || condition == 15u) {
            int32_t displacement;
            uint32_t next_pc, target;
            if (branch_displacement(ctx, pc, opcode, &displacement, &next_pc) != 0) return AMTARI_EFAULT;
            if (!condition_true(ctx, condition)) {
                ctx->cpu.pc = next_pc;
                return AMTARI_EXEC_RUNNING;
            }
            target = (uint32_t)((int32_t)next_pc + displacement);
            if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
            ctx->cpu.pc = target;
            return AMTARI_EXEC_RUNNING;
        }
    }

    if (opcode == 0x4eb9u) {
        uint32_t target;
        if (amtari_guest_read32(ctx, pc + 2u, &target) != 0) return AMTARI_EFAULT;
        if (branch_target_valid(ctx, target) != 0) return AMTARI_EFAULT;
        if (push32(ctx, pc + 6u) != 0) return AMTARI_EFAULT;
        ctx->cpu.pc = target;
        return AMTARI_EXEC_RUNNING;
    }

    if (opcode == 0x4e41u) {
        uint16_t function;
        int32_t result;
        if (amtari_guest_read16(ctx, ctx->cpu.a[7], &function) != 0) return AMTARI_EFAULT;
        result = amtari_gemdos_dispatch(ctx, function);
        ctx->cpu.d[0] = (uint32_t)result;
        set_nz32(ctx, ctx->cpu.d[0]);
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
