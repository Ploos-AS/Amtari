#include <string.h>

#include "amtari.h"

#define CCR_X 0x10u
#define CCR_N 0x08u
#define CCR_Z 0x04u
#define CCR_V 0x02u
#define CCR_C 0x01u

static int guest_read8(const struct amtari_context *ctx, uint32_t address, uint8_t *value)
{
    if (!amtari_guest_range_valid(ctx, address, 1u)) return AMTARI_EFAULT;
    *value = ctx->memory.data[address];
    return 0;
}

static int guest_write8(struct amtari_context *ctx, uint32_t address, uint8_t value)
{
    if (!amtari_guest_range_valid(ctx, address, 1u)) return AMTARI_EFAULT;
    ctx->memory.data[address] = value;
    return 0;
}

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

static uint32_t size_mask(unsigned int size)
{
    if (size == 1u) return 0xffu;
    if (size == 2u) return 0xffffu;
    return 0xffffffffu;
}

static uint32_t size_sign(unsigned int size)
{
    if (size == 1u) return 0x80u;
    if (size == 2u) return 0x8000u;
    return 0x80000000u;
}

static uint32_t dreg_value(const struct amtari_context *ctx, unsigned int reg, unsigned int size)
{
    return ctx->cpu.d[reg] & size_mask(size);
}

static void dreg_write(struct amtari_context *ctx, unsigned int reg, unsigned int size, uint32_t value)
{
    value &= size_mask(size);
    if (size == 1u) ctx->cpu.d[reg] = (ctx->cpu.d[reg] & 0xffffff00u) | value;
    else if (size == 2u) ctx->cpu.d[reg] = (ctx->cpu.d[reg] & 0xffff0000u) | value;
    else ctx->cpu.d[reg] = value;
}

static void set_nz(struct amtari_context *ctx, uint32_t value, unsigned int size)
{
    uint32_t mask = size_mask(size);
    uint16_t ccr = (uint16_t)(ctx->cpu.sr & CCR_X);
    value &= mask;
    if (value == 0u) ccr |= CCR_Z;
    if ((value & size_sign(size)) != 0u) ccr |= CCR_N;
    ctx->cpu.sr = (uint16_t)((ctx->cpu.sr & 0xff00u) | ccr);
}

static void set_cmp(struct amtari_context *ctx, uint32_t dst, uint32_t src, unsigned int size)
{
    uint32_t mask = size_mask(size);
    uint32_t sign = size_sign(size);
    uint32_t result;
    uint16_t ccr = (uint16_t)(ctx->cpu.sr & CCR_X);

    dst &= mask;
    src &= mask;
    result = (dst - src) & mask;
    if (result == 0u) ccr |= CCR_Z;
    if ((result & sign) != 0u) ccr |= CCR_N;
    if ((((dst ^ src) & (dst ^ result)) & sign) != 0u) ccr |= CCR_V;
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

static uint32_t ea_increment(unsigned int reg, unsigned int size)
{
    if (size == 1u && reg == 7u) return 2u;
    return size;
}

static int read_memory_value(const struct amtari_context *ctx, uint32_t address,
                             unsigned int size, uint32_t *value)
{
    if (size == 1u) {
        uint8_t v;
        if (guest_read8(ctx, address, &v) != 0) return AMTARI_EFAULT;
        *value = v;
        return 0;
    }
    if (size == 2u) {
        uint16_t v;
        if (amtari_guest_read16(ctx, address, &v) != 0) return AMTARI_EFAULT;
        *value = v;
        return 0;
    }
    return amtari_guest_read32(ctx, address, value);
}

static int write_memory_value(struct amtari_context *ctx, uint32_t address,
                              unsigned int size, uint32_t value)
{
    if (size == 1u) return guest_write8(ctx, address, (uint8_t)value);
    if (size == 2u) return guest_write16(ctx, address, (uint16_t)value);
    return guest_write32(ctx, address, value);
}

static int ea_read(struct amtari_context *ctx, unsigned int mode, unsigned int reg,
                   unsigned int size, uint32_t *ext_pc, uint32_t *value)
{
    uint32_t address;
    uint16_t ext;

    if (mode == 0u) {
        *value = dreg_value(ctx, reg, size);
        return 0;
    }
    if (mode == 1u) {
        if (size == 1u) return AMTARI_EILLEGAL;
        *value = ctx->cpu.a[reg] & size_mask(size);
        return 0;
    }
    if (mode == 2u) return read_memory_value(ctx, ctx->cpu.a[reg], size, value);
    if (mode == 3u) {
        address = ctx->cpu.a[reg];
        if (read_memory_value(ctx, address, size, value) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[reg] += ea_increment(reg, size);
        return 0;
    }
    if (mode == 4u) {
        uint32_t amount = ea_increment(reg, size);
        if (ctx->cpu.a[reg] < amount) return AMTARI_EFAULT;
        ctx->cpu.a[reg] -= amount;
        return read_memory_value(ctx, ctx->cpu.a[reg], size, value);
    }
    if (mode == 5u) {
        if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
        *ext_pc += 2u;
        address = (uint32_t)((int32_t)ctx->cpu.a[reg] + (int32_t)(int16_t)ext);
        return read_memory_value(ctx, address, size, value);
    }
    if (mode == 7u && reg == 0u) {
        if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
        *ext_pc += 2u;
        address = (uint32_t)(int32_t)(int16_t)ext;
        return read_memory_value(ctx, address, size, value);
    }
    if (mode == 7u && reg == 1u) {
        if (amtari_guest_read32(ctx, *ext_pc, &address) != 0) return AMTARI_EFAULT;
        *ext_pc += 4u;
        return read_memory_value(ctx, address, size, value);
    }
    if (mode == 7u && reg == 4u) {
        if (size == 4u) {
            if (amtari_guest_read32(ctx, *ext_pc, value) != 0) return AMTARI_EFAULT;
            *ext_pc += 4u;
        } else {
            if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
            *ext_pc += 2u;
            *value = (size == 1u) ? (uint32_t)(ext & 0xffu) : (uint32_t)ext;
        }
        return 0;
    }
    return AMTARI_EILLEGAL;
}

static int ea_write(struct amtari_context *ctx, unsigned int mode, unsigned int reg,
                    unsigned int size, uint32_t *ext_pc, uint32_t value)
{
    uint32_t address;
    uint16_t ext;

    if (mode == 0u) {
        dreg_write(ctx, reg, size, value);
        return 0;
    }
    if (mode == 2u) return write_memory_value(ctx, ctx->cpu.a[reg], size, value);
    if (mode == 3u) {
        address = ctx->cpu.a[reg];
        if (write_memory_value(ctx, address, size, value) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[reg] += ea_increment(reg, size);
        return 0;
    }
    if (mode == 4u) {
        uint32_t amount = ea_increment(reg, size);
        if (ctx->cpu.a[reg] < amount) return AMTARI_EFAULT;
        ctx->cpu.a[reg] -= amount;
        return write_memory_value(ctx, ctx->cpu.a[reg], size, value);
    }
    if (mode == 5u) {
        if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
        *ext_pc += 2u;
        address = (uint32_t)((int32_t)ctx->cpu.a[reg] + (int32_t)(int16_t)ext);
        return write_memory_value(ctx, address, size, value);
    }
    if (mode == 7u && reg == 0u) {
        if (amtari_guest_read16(ctx, *ext_pc, &ext) != 0) return AMTARI_EFAULT;
        *ext_pc += 2u;
        address = (uint32_t)(int32_t)(int16_t)ext;
        return write_memory_value(ctx, address, size, value);
    }
    if (mode == 7u && reg == 1u) {
        if (amtari_guest_read32(ctx, *ext_pc, &address) != 0) return AMTARI_EFAULT;
        *ext_pc += 4u;
        return write_memory_value(ctx, address, size, value);
    }
    return AMTARI_EILLEGAL;
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

static int decode_size_bits(uint16_t opcode, unsigned int *size)
{
    unsigned int bits = (unsigned int)((opcode >> 6) & 3u);
    if (bits == 0u) *size = 1u;
    else if (bits == 1u) *size = 2u;
    else if (bits == 2u) *size = 4u;
    else return AMTARI_EILLEGAL;
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

    if ((opcode & 0xfff8u) == 0x4e58u) {
        unsigned int reg = opcode & 7u;
        uint32_t old_a;
        ctx->cpu.a[7] = ctx->cpu.a[reg];
        if (pop32(ctx, &old_a) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[reg] = old_a;
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1f8u) == 0x41e8u) {
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t address;
        if (displacement_address(ctx, pc, src, &address) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[dst] = address;
        ctx->cpu.pc = pc + 4u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf1ffu) == 0x41f9u) {
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t address;
        if (amtari_guest_read32(ctx, pc + 2u, &address) != 0) return AMTARI_EFAULT;
        ctx->cpu.a[dst] = address;
        ctx->cpu.pc = pc + 6u;
        return AMTARI_EXEC_RUNNING;
    }

    if ((opcode & 0xf100u) == 0x7000u) {
        unsigned int reg = (unsigned int)((opcode >> 9) & 7u);
        ctx->cpu.d[reg] = (uint32_t)(int32_t)(int8_t)(opcode & 0xffu);
        set_nz(ctx, ctx->cpu.d[reg], 4u);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* EXT.W Dn / EXT.L Dn */
    if ((opcode & 0xfff8u) == 0x4880u) {
        unsigned int reg = opcode & 7u;
        int16_t value = (int16_t)(int8_t)(ctx->cpu.d[reg] & 0xffu);
        dreg_write(ctx, reg, 2u, (uint16_t)value);
        set_nz(ctx, (uint16_t)value, 2u);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }
    if ((opcode & 0xfff8u) == 0x48c0u) {
        unsigned int reg = opcode & 7u;
        int32_t value = (int32_t)(int16_t)(ctx->cpu.d[reg] & 0xffffu);
        ctx->cpu.d[reg] = (uint32_t)value;
        set_nz(ctx, ctx->cpu.d[reg], 4u);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* CLR.B/W/L <ea> */
    if ((opcode & 0xff00u) == 0x4200u) {
        unsigned int size;
        unsigned int mode = (unsigned int)((opcode >> 3) & 7u);
        unsigned int reg = opcode & 7u;
        uint32_t ext_pc = pc + 2u;
        int rc;
        if (decode_size_bits(opcode, &size) != 0 || mode == 1u) return AMTARI_EILLEGAL;
        rc = ea_write(ctx, mode, reg, size, &ext_pc, 0u);
        if (rc != 0) return rc;
        set_nz(ctx, 0u, size);
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    /* TST.B/W/L <ea> */
    if ((opcode & 0xff00u) == 0x4a00u) {
        unsigned int size;
        unsigned int mode = (unsigned int)((opcode >> 3) & 7u);
        unsigned int reg = opcode & 7u;
        uint32_t ext_pc = pc + 2u;
        uint32_t value;
        int rc;
        if (decode_size_bits(opcode, &size) != 0 || mode == 1u) return AMTARI_EILLEGAL;
        rc = ea_read(ctx, mode, reg, size, &ext_pc, &value);
        if (rc != 0) return rc;
        set_nz(ctx, value, size);
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    /* MOVE.B/W/L, including MOVEA.W/L when destination mode is An. */
    if ((opcode & 0xc000u) == 0u && (opcode & 0x3000u) != 0u) {
        unsigned int top = (unsigned int)((opcode >> 12) & 3u);
        unsigned int size = (top == 1u) ? 1u : (top == 2u) ? 4u : 2u;
        unsigned int src_mode = (unsigned int)((opcode >> 3) & 7u);
        unsigned int src_reg = opcode & 7u;
        unsigned int dst_mode = (unsigned int)((opcode >> 6) & 7u);
        unsigned int dst_reg = (unsigned int)((opcode >> 9) & 7u);
        uint32_t ext_pc = pc + 2u;
        uint32_t value;
        int rc = ea_read(ctx, src_mode, src_reg, size, &ext_pc, &value);
        if (rc != 0) return rc;
        if (dst_mode == 1u) {
            if (size == 1u) return AMTARI_EILLEGAL;
            if (size == 2u) ctx->cpu.a[dst_reg] = (uint32_t)(int32_t)(int16_t)value;
            else ctx->cpu.a[dst_reg] = value;
        } else {
            rc = ea_write(ctx, dst_mode, dst_reg, size, &ext_pc, value);
            if (rc != 0) return rc;
            set_nz(ctx, value, size);
        }
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    /* CMPI.B/W/L #imm,Dn */
    if ((opcode & 0xff00u) == 0x0c00u && ((opcode >> 3) & 7u) == 0u) {
        unsigned int size;
        unsigned int reg = opcode & 7u;
        uint32_t ext_pc = pc + 2u;
        uint32_t value;
        if (decode_size_bits(opcode, &size) != 0) return AMTARI_EILLEGAL;
        if (ea_read(ctx, 7u, 4u, size, &ext_pc, &value) != 0) return AMTARI_EFAULT;
        set_cmp(ctx, dreg_value(ctx, reg, size), value, size);
        ctx->cpu.pc = ext_pc;
        return AMTARI_EXEC_RUNNING;
    }

    /* CMP.B/W/L Dm,Dn */
    if ((opcode & 0xf138u) == 0xb000u ||
        (opcode & 0xf138u) == 0xb040u ||
        (opcode & 0xf138u) == 0xb080u) {
        unsigned int size;
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        if (decode_size_bits(opcode, &size) != 0) return AMTARI_EILLEGAL;
        set_cmp(ctx, dreg_value(ctx, dst, size), dreg_value(ctx, src, size), size);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* OR.B/W/L Dm,Dn */
    if ((opcode & 0xf138u) == 0x8000u ||
        (opcode & 0xf138u) == 0x8040u ||
        (opcode & 0xf138u) == 0x8080u) {
        unsigned int size;
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (decode_size_bits(opcode, &size) != 0) return AMTARI_EILLEGAL;
        value = dreg_value(ctx, dst, size) | dreg_value(ctx, src, size);
        dreg_write(ctx, dst, size, value);
        set_nz(ctx, value, size);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* AND.B/W/L Dm,Dn */
    if ((opcode & 0xf138u) == 0xc000u ||
        (opcode & 0xf138u) == 0xc040u ||
        (opcode & 0xf138u) == 0xc080u) {
        unsigned int size;
        unsigned int src = opcode & 7u;
        unsigned int dst = (unsigned int)((opcode >> 9) & 7u);
        uint32_t value;
        if (decode_size_bits(opcode, &size) != 0) return AMTARI_EILLEGAL;
        value = dreg_value(ctx, dst, size) & dreg_value(ctx, src, size);
        dreg_write(ctx, dst, size, value);
        set_nz(ctx, value, size);
        ctx->cpu.pc = pc + 2u;
        return AMTARI_EXEC_RUNNING;
    }

    /* EOR.B/W/L Dm,Dn */
    if ((opcode & 0xf138u) == 0xb100u ||
        (opcode & 0xf138u) == 0xb140u ||
        (opcode & 0xf138u) == 0xb180u) {
        unsigned int size;
        unsigned int src = (unsigned int)((opcode >> 9) & 7u);
        unsigned int dst = opcode & 7u;
        uint32_t value;
        if (decode_size_bits(opcode, &size) != 0) return AMTARI_EILLEGAL;
        value = dreg_value(ctx, dst, size) ^ dreg_value(ctx, src, size);
        dreg_write(ctx, dst, size, value);
        set_nz(ctx, value, size);
        ctx->cpu.pc = pc + 2u;
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
        set_nz(ctx, ctx->cpu.d[0], 4u);
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
