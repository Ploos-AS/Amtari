/* M2.18 GEMDOS process extension: reserved process blocks + bounded nested Pexec(0).
 *
 * The PRG loader now owns process-block allocation semantics. Pexec(0) requests
 * a private tail reservation for the guest stack, p_hitpa describes the actual
 * allocation high address, and nested children start after that complete block.
 */
#define AMTARI_M214_DISPATCH_NAME amtari_gemdos_dispatch_m214
#include "gemdos_m214.c"
#undef AMTARI_M214_DISPATCH_NAME

#define M218_STACK_RESERVE 4096u
#define M218_MAX_STEPS 65536u
#define M218_MAX_PROCESS_DEPTH 8u

static int32_t m218_pexec_load_and_go(struct amtari_context *ctx)
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
    if (ctx->process_depth >= M218_MAX_PROCESS_DEPTH) return AMTARI_ENOSYS;
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
    parent_depth = ctx->process_depth;
    basepage = parent_next_load;

    load_result = amtari_prg_load_reserved(ctx, image, image_size, basepage, cmdline,
                                          M218_STACK_RESERVE);
    if (load_result < 0) return load_result;

    child_stack_top = ctx->next_load_address;
    if (child_stack_top < 4u ||
        !amtari_guest_range_valid(ctx, child_stack_top - 4u, 4u)) {
        ctx->next_load_address = parent_next_load;
        return AMTARI_ENOMEM;
    }

    ctx->process_depth = (uint8_t)(parent_depth + 1u);
    rc = amtari_exec_prepare(ctx, basepage, child_stack_top);
    if (rc == 0) child_rc = amtari_exec_run(ctx, M218_MAX_STEPS, &steps);
    else child_rc = rc;

    if (child_rc == AMTARI_EXEC_HALTED) child_rc = (int32_t)ctx->cpu.d[0];
    ctx->cpu = parent_cpu;
    ctx->current_basepage = parent_basepage;
    ctx->next_load_address = parent_next_load;
    ctx->process_depth = parent_depth;

    return child_rc;
}

int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function)
{
    uint16_t mode;

    if (ctx == 0 || !ctx->initialized) return AMTARI_EINVAL;
    if (function != 0x4bu) return amtari_gemdos_dispatch_m214(ctx, function);
    if (read_arg16(ctx, 0u, &mode) != 0) return AMTARI_EFAULT;
    if (mode != 0u) return amtari_gemdos_dispatch_m214(ctx, function);
    return m218_pexec_load_and_go(ctx);
}
