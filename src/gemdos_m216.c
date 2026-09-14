/* M2.20 GEMDOS process/memory extension.
 *
 * M2.19 introduced a bounded guest heap. M2.20 adds splitting, adjacent free
 * block coalescing, top trimming, largest-free-block queries over fragmented
 * memory, and keeps Pexec children above any active parent heap allocation.
 */
#define AMTARI_M214_DISPATCH_NAME amtari_gemdos_dispatch_m214
#include "gemdos_m214.c"
#undef AMTARI_M214_DISPATCH_NAME

#define M220_STACK_RESERVE 4096u
#define M220_MAX_STEPS 65536u
#define M220_MAX_PROCESS_DEPTH 8u

static uint32_t m220_align16(uint32_t value)
{
    return (value + 15u) & ~15u;
}

static uint32_t m220_memory_limit(const struct amtari_context *ctx)
{
    return ctx->memory.size > UINT32_MAX ? UINT32_MAX : (uint32_t)ctx->memory.size;
}

static int m220_find_invalid_slot(const struct amtari_context *ctx)
{
    unsigned int i;
    for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
        if (!ctx->mem_blocks[i].valid) return (int)i;
    }
    return -1;
}

static int m220_find_block(const struct amtari_context *ctx, uint32_t address)
{
    unsigned int i;
    for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
        if (ctx->mem_blocks[i].valid && ctx->mem_blocks[i].in_use &&
            ctx->mem_blocks[i].address == address) return (int)i;
    }
    return -1;
}

static void m220_clear_slot(struct amtari_mem_block *block)
{
    memset(block, 0, sizeof(*block));
}

static void m220_coalesce_free(struct amtari_context *ctx)
{
    int changed;
    unsigned int i;
    unsigned int j;

    do {
        changed = 0;
        for (i = 0u; i < AMTARI_MEM_BLOCK_MAX && !changed; ++i) {
            uint32_t i_end;
            if (!ctx->mem_blocks[i].valid || ctx->mem_blocks[i].in_use) continue;
            i_end = ctx->mem_blocks[i].address + ctx->mem_blocks[i].size;
            for (j = i + 1u; j < AMTARI_MEM_BLOCK_MAX; ++j) {
                uint32_t j_end;
                if (!ctx->mem_blocks[j].valid || ctx->mem_blocks[j].in_use) continue;
                j_end = ctx->mem_blocks[j].address + ctx->mem_blocks[j].size;
                if (i_end == ctx->mem_blocks[j].address) {
                    ctx->mem_blocks[i].size += ctx->mem_blocks[j].size;
                    m220_clear_slot(&ctx->mem_blocks[j]);
                    changed = 1;
                    break;
                }
                if (j_end == ctx->mem_blocks[i].address) {
                    ctx->mem_blocks[i].address = ctx->mem_blocks[j].address;
                    ctx->mem_blocks[i].size += ctx->mem_blocks[j].size;
                    m220_clear_slot(&ctx->mem_blocks[j]);
                    changed = 1;
                    break;
                }
            }
        }
    } while (changed);
}

static void m220_trim_heap(struct amtari_context *ctx)
{
    int changed;
    unsigned int i;

    m220_coalesce_free(ctx);
    if (ctx->heap_top < ctx->next_load_address) ctx->heap_top = ctx->next_load_address;

    do {
        changed = 0;
        for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
            uint32_t end;
            if (!ctx->mem_blocks[i].valid || ctx->mem_blocks[i].in_use) continue;
            end = ctx->mem_blocks[i].address + ctx->mem_blocks[i].size;
            if (end == ctx->heap_top && ctx->mem_blocks[i].address >= ctx->next_load_address) {
                ctx->heap_top = ctx->mem_blocks[i].address;
                m220_clear_slot(&ctx->mem_blocks[i]);
                changed = 1;
                break;
            }
        }
    } while (changed);

    if (ctx->heap_top == ctx->next_load_address) {
        int any_used = 0;
        for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
            if (ctx->mem_blocks[i].valid && ctx->mem_blocks[i].in_use) {
                any_used = 1;
                break;
            }
        }
        if (!any_used && ctx->process_depth == 0u) ctx->heap_top = 0u;
    }
}

static void m220_release_owner(struct amtari_context *ctx, uint32_t owner)
{
    unsigned int i;
    for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
        if (ctx->mem_blocks[i].valid && ctx->mem_blocks[i].in_use &&
            ctx->mem_blocks[i].owner_basepage == owner) {
            ctx->mem_blocks[i].in_use = 0u;
            ctx->mem_blocks[i].owner_basepage = 0u;
        }
    }
    m220_trim_heap(ctx);
}

static int32_t m220_malloc(struct amtari_context *ctx)
{
    uint32_t request;
    uint32_t size;
    uint32_t start;
    uint32_t limit;
    uint32_t largest = 0u;
    unsigned int i;
    int split_slot;
    int slot;

    if (read_arg32(ctx, 0u, &request) != 0) return AMTARI_EFAULT;
    limit = m220_memory_limit(ctx);

    if (request == UINT32_MAX) {
        start = ctx->heap_top > ctx->next_load_address ? ctx->heap_top : ctx->next_load_address;
        if (start < limit) largest = limit - start;
        for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
            if (ctx->mem_blocks[i].valid && !ctx->mem_blocks[i].in_use &&
                ctx->mem_blocks[i].size > largest) largest = ctx->mem_blocks[i].size;
        }
        return (int32_t)largest;
    }

    if (request == 0u || request > UINT32_MAX - 15u) return AMTARI_ENOMEM;
    size = m220_align16(request);
    m220_coalesce_free(ctx);

    for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
        uint32_t remainder;
        if (!ctx->mem_blocks[i].valid || ctx->mem_blocks[i].in_use ||
            ctx->mem_blocks[i].size < size) continue;

        remainder = ctx->mem_blocks[i].size - size;
        if (remainder >= 16u) {
            split_slot = m220_find_invalid_slot(ctx);
            if (split_slot >= 0) {
                ctx->mem_blocks[split_slot].address = ctx->mem_blocks[i].address + size;
                ctx->mem_blocks[split_slot].size = remainder;
                ctx->mem_blocks[split_slot].owner_basepage = 0u;
                ctx->mem_blocks[split_slot].valid = 1u;
                ctx->mem_blocks[split_slot].in_use = 0u;
                ctx->mem_blocks[i].size = size;
            }
        }
        ctx->mem_blocks[i].in_use = 1u;
        ctx->mem_blocks[i].owner_basepage = ctx->current_basepage;
        return (int32_t)ctx->mem_blocks[i].address;
    }

    slot = m220_find_invalid_slot(ctx);
    if (slot < 0) return AMTARI_ENOMEM;
    start = ctx->heap_top > ctx->next_load_address ? ctx->heap_top : ctx->next_load_address;
    start = m220_align16(start);
    if (start > limit || size > limit - start) return AMTARI_ENOMEM;

    ctx->mem_blocks[slot].address = start;
    ctx->mem_blocks[slot].size = size;
    ctx->mem_blocks[slot].owner_basepage = ctx->current_basepage;
    ctx->mem_blocks[slot].valid = 1u;
    ctx->mem_blocks[slot].in_use = 1u;
    ctx->heap_top = start + size;
    return (int32_t)start;
}

static int32_t m220_mfree(struct amtari_context *ctx)
{
    uint32_t address;
    int slot;
    if (read_arg32(ctx, 0u, &address) != 0) return AMTARI_EFAULT;
    slot = m220_find_block(ctx, address);
    if (slot < 0) return AMTARI_EINVAL;
    ctx->mem_blocks[slot].in_use = 0u;
    ctx->mem_blocks[slot].owner_basepage = 0u;
    m220_trim_heap(ctx);
    return 0;
}

static int32_t m220_mshrink(struct amtari_context *ctx)
{
    uint16_t dummy;
    uint32_t address;
    uint32_t requested;
    uint32_t new_size;
    uint32_t old_size;
    uint32_t old_end;
    int slot;
    int free_slot;

    if (read_arg16(ctx, 0u, &dummy) != 0 || read_arg32(ctx, 2u, &address) != 0 ||
        read_arg32(ctx, 6u, &requested) != 0) return AMTARI_EFAULT;
    (void)dummy;
    slot = m220_find_block(ctx, address);
    if (slot < 0) return AMTARI_EINVAL;
    if (requested == 0u || requested > UINT32_MAX - 15u) return AMTARI_EINVAL;
    new_size = m220_align16(requested);
    old_size = ctx->mem_blocks[slot].size;
    if (new_size > old_size) return AMTARI_ENOMEM;
    if (new_size == old_size) return 0;

    old_end = address + old_size;
    if (old_end == ctx->heap_top) {
        ctx->mem_blocks[slot].size = new_size;
        ctx->heap_top = address + new_size;
        m220_trim_heap(ctx);
        return 0;
    }

    free_slot = m220_find_invalid_slot(ctx);
    if (free_slot < 0) return AMTARI_ENOMEM;
    ctx->mem_blocks[free_slot].address = address + new_size;
    ctx->mem_blocks[free_slot].size = old_size - new_size;
    ctx->mem_blocks[free_slot].owner_basepage = 0u;
    ctx->mem_blocks[free_slot].valid = 1u;
    ctx->mem_blocks[free_slot].in_use = 0u;
    ctx->mem_blocks[slot].size = new_size;
    m220_coalesce_free(ctx);
    return 0;
}

static int32_t m220_pexec_load_and_go(struct amtari_context *ctx)
{
    uint16_t mode;
    uint32_t name, cmdline_address, env;
    const uint8_t *image = 0;
    const uint8_t *cmdline = 0;
    size_t image_size = 0u;
    char path[AMTARI_PATH_MAX];
    struct amtari_cpu_state parent_cpu;
    uint32_t parent_basepage;
    uint32_t parent_next_load;
    uint32_t parent_heap_top;
    uint32_t basepage;
    uint32_t child_stack_top;
    uint32_t steps = 0u;
    uint8_t parent_depth;
    int32_t load_result;
    int child_rc;
    int rc;

    if (read_arg16(ctx, 0u, &mode) != 0 || read_arg32(ctx, 2u, &name) != 0 ||
        read_arg32(ctx, 6u, &cmdline_address) != 0 || read_arg32(ctx, 10u, &env) != 0)
        return AMTARI_EFAULT;
    (void)env;
    if (mode != 0u) return AMTARI_ENOSYS;
    if (ctx->process_depth >= M220_MAX_PROCESS_DEPTH) return AMTARI_ENOSYS;
    if (ctx->process.fetch == 0) return AMTARI_EIO;

    rc = amtari_path_translate(ctx, name, path, sizeof(path));
    if (rc != 0) return rc;
    if (cmdline_address != 0u) {
        size_t length;
        if (!amtari_guest_range_valid(ctx, cmdline_address, 1u)) return AMTARI_EFAULT;
        length = (size_t)ctx->memory.data[cmdline_address] + 1u;
        if (!amtari_guest_range_valid(ctx, cmdline_address, length)) return AMTARI_EFAULT;
        cmdline = &ctx->memory.data[cmdline_address];
    }

    rc = ctx->process.fetch(ctx->process.opaque, path, &image, &image_size);
    if (rc != 0) return amtari_gemdos_error_from_host(rc);
    if (image == 0) return AMTARI_EIO;

    parent_cpu = ctx->cpu;
    parent_basepage = ctx->current_basepage;
    parent_next_load = ctx->next_load_address;
    parent_heap_top = ctx->heap_top;
    parent_depth = ctx->process_depth;
    basepage = parent_heap_top > parent_next_load ? parent_heap_top : parent_next_load;
    basepage = m220_align16(basepage);

    load_result = amtari_prg_load_reserved(ctx, image, image_size, basepage, cmdline,
                                          M220_STACK_RESERVE);
    if (load_result < 0) return load_result;

    child_stack_top = ctx->next_load_address;
    if (child_stack_top < 4u ||
        !amtari_guest_range_valid(ctx, child_stack_top - 4u, 4u)) {
        ctx->next_load_address = parent_next_load;
        return AMTARI_ENOMEM;
    }
    if (ctx->heap_top < child_stack_top) ctx->heap_top = child_stack_top;

    ctx->process_depth = (uint8_t)(parent_depth + 1u);
    rc = amtari_exec_prepare(ctx, basepage, child_stack_top);
    if (rc == 0) child_rc = amtari_exec_run(ctx, M220_MAX_STEPS, &steps);
    else child_rc = rc;

    if (child_rc == AMTARI_EXEC_HALTED) child_rc = (int32_t)ctx->cpu.d[0];
    m220_release_owner(ctx, basepage);
    ctx->cpu = parent_cpu;
    ctx->current_basepage = parent_basepage;
    ctx->next_load_address = parent_next_load;
    ctx->heap_top = parent_heap_top;
    ctx->process_depth = parent_depth;

    return child_rc;
}

int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function)
{
    uint16_t mode;

    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    if (function == 0x48u) return m220_malloc(ctx);
    if (function == 0x49u) return m220_mfree(ctx);
    if (function == 0x4au) return m220_mshrink(ctx);
    if (function != 0x4bu) return amtari_gemdos_dispatch_m214(ctx, function);
    if (read_arg16(ctx, 0u, &mode) != 0) return AMTARI_EFAULT;
    if (mode != 0u) return amtari_gemdos_dispatch_m214(ctx, function);
    return m220_pexec_load_and_go(ctx);
}
