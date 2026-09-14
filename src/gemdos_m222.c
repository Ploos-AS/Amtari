/* M2.22 GEMDOS process arena tracking.
 *
 * M2.21 enforces heap ownership. M2.22 makes synchronous Pexec(0) reserve its
 * basepage, loaded image and stack tail inside the same bounded memory table
 * used by the GEMDOS heap. The process arena is temporary: because Pexec(0)
 * executes synchronously, the complete parent allocator topology is restored
 * when the child returns.
 */
#define AMTARI_M221_DISPATCH_NAME amtari_gemdos_dispatch_m221
#include "gemdos_m221.c"
#undef AMTARI_M221_DISPATCH_NAME

#define M222_STACK_RESERVE 4096u
#define M222_MAX_STEPS 65536u
#define M222_MAX_PROCESS_DEPTH 8u

static unsigned int m222_invalid_slots(const struct amtari_context *ctx)
{
    unsigned int i;
    unsigned int count = 0u;
    for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
        if (!ctx->mem_blocks[i].valid) ++count;
    }
    return count;
}

static int m222_reserve_arena(struct amtari_context *ctx, uint32_t basepage,
                              uint32_t child_stack_top)
{
    uint32_t arena_size = child_stack_top - basepage;
    unsigned int i;
    int split_slot;
    int slot;

    /* Reuse a free block that already covers this address range. A released
     * synchronous Pexec arena must never be shadowed by an overlapping
     * descriptor. In normal M2.22 operation the parent's descriptor table is
     * restored after Pexec, but this also keeps reservation locally robust. */
    m220_coalesce_free(ctx);
    for (i = 0u; i < AMTARI_MEM_BLOCK_MAX; ++i) {
        struct amtari_mem_block *block = &ctx->mem_blocks[i];
        uint32_t block_end;
        uint32_t arena_end;
        uint32_t prefix;
        uint32_t suffix;
        unsigned int needed;

        if (!block->valid || block->in_use) continue;
        block_end = block->address + block->size;
        arena_end = basepage + arena_size;
        if (basepage < block->address || arena_end > block_end) continue;

        prefix = basepage - block->address;
        suffix = block_end - arena_end;
        needed = (prefix != 0u ? 1u : 0u) + (suffix != 0u ? 1u : 0u);
        if (m222_invalid_slots(ctx) < needed) return AMTARI_ENOMEM;

        if (prefix != 0u) {
            split_slot = m220_find_invalid_slot(ctx);
            if (split_slot < 0) return AMTARI_ENOMEM;
            ctx->mem_blocks[split_slot].address = block->address;
            ctx->mem_blocks[split_slot].size = prefix;
            ctx->mem_blocks[split_slot].owner_basepage = 0u;
            ctx->mem_blocks[split_slot].valid = 1u;
            ctx->mem_blocks[split_slot].in_use = 0u;
        }

        if (suffix != 0u) {
            split_slot = m220_find_invalid_slot(ctx);
            if (split_slot < 0) return AMTARI_ENOMEM;
            ctx->mem_blocks[split_slot].address = arena_end;
            ctx->mem_blocks[split_slot].size = suffix;
            ctx->mem_blocks[split_slot].owner_basepage = 0u;
            ctx->mem_blocks[split_slot].valid = 1u;
            ctx->mem_blocks[split_slot].in_use = 0u;
        }

        block->address = basepage;
        block->size = arena_size;
        block->owner_basepage = basepage;
        block->valid = 1u;
        block->in_use = 1u;
        return 0;
    }

    slot = m220_find_invalid_slot(ctx);
    if (slot < 0) return AMTARI_ENOMEM;
    ctx->mem_blocks[slot].address = basepage;
    ctx->mem_blocks[slot].size = arena_size;
    ctx->mem_blocks[slot].owner_basepage = basepage;
    ctx->mem_blocks[slot].valid = 1u;
    ctx->mem_blocks[slot].in_use = 1u;
    return 0;
}

static int32_t m222_pexec_load_and_go(struct amtari_context *ctx)
{
    uint16_t mode;
    uint32_t name, cmdline_address, env;
    const uint8_t *image = 0;
    const uint8_t *cmdline = 0;
    size_t image_size = 0u;
    char path[AMTARI_PATH_MAX];
    struct amtari_cpu_state parent_cpu;
    struct amtari_mem_block parent_mem_blocks[AMTARI_MEM_BLOCK_MAX];
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
    if (ctx->process_depth >= M222_MAX_PROCESS_DEPTH) return AMTARI_ENOSYS;
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
    memcpy(parent_mem_blocks, ctx->mem_blocks, sizeof(parent_mem_blocks));
    parent_basepage = ctx->current_basepage;
    parent_next_load = ctx->next_load_address;
    parent_heap_top = ctx->heap_top;
    parent_depth = ctx->process_depth;
    basepage = parent_heap_top > parent_next_load ? parent_heap_top : parent_next_load;
    basepage = m220_align16(basepage);

    load_result = amtari_prg_load_reserved(ctx, image, image_size, basepage, cmdline,
                                          M222_STACK_RESERVE);
    if (load_result < 0) return load_result;

    child_stack_top = ctx->next_load_address;
    if (child_stack_top <= basepage || child_stack_top < 4u ||
        !amtari_guest_range_valid(ctx, child_stack_top - 4u, 4u)) {
        ctx->next_load_address = parent_next_load;
        return AMTARI_ENOMEM;
    }

    rc = m222_reserve_arena(ctx, basepage, child_stack_top);
    if (rc != 0) {
        memcpy(ctx->mem_blocks, parent_mem_blocks, sizeof(parent_mem_blocks));
        ctx->next_load_address = parent_next_load;
        ctx->heap_top = parent_heap_top;
        return rc;
    }

    if (ctx->heap_top < child_stack_top) ctx->heap_top = child_stack_top;

    ctx->process_depth = (uint8_t)(parent_depth + 1u);
    rc = amtari_exec_prepare(ctx, basepage, child_stack_top);
    if (rc == 0) child_rc = amtari_exec_run(ctx, M222_MAX_STEPS, &steps);
    else child_rc = rc;

    if (child_rc == AMTARI_EXEC_HALTED) child_rc = (int32_t)ctx->cpu.d[0];

    /* Pexec(0) is synchronous. Discard every child-owned arena/heap mutation
     * and restore the exact allocator graph visible to the parent before the
     * child started. This is stronger and safer than converting child blocks
     * into free parent blocks, which changes later Malloc placement/topology. */
    memcpy(ctx->mem_blocks, parent_mem_blocks, sizeof(parent_mem_blocks));
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
    if (function != 0x4bu) return amtari_gemdos_dispatch_m221(ctx, function);
    if (read_arg16(ctx, 0u, &mode) != 0) return AMTARI_EFAULT;
    if (mode != 0u) return amtari_gemdos_dispatch_m221(ctx, function);
    return m222_pexec_load_and_go(ctx);
}
