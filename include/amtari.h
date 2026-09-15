#ifndef AMTARI_H
#define AMTARI_H

#include <stddef.h>
#include <stdint.h>

#define AMTARI_VERSION "0.3.2-m3"
#define AMTARI_PATH_MAX 260
#define AMTARI_BASEPAGE_SIZE 256u
#define AMTARI_MEM_BLOCK_MAX 32u
#define AMTARI_EINVAL (-1)
#define AMTARI_EFAULT (-2)
#define AMTARI_ENOSYS (-3)
#define AMTARI_EIO (-4)
#define AMTARI_ENOENT (-5)
#define AMTARI_EBADF (-6)
#define AMTARI_EACCES (-7)
#define AMTARI_EEXIST (-8)
#define AMTARI_ENOTDIR (-9)
#define AMTARI_EISDIR (-10)
#define AMTARI_ENOSPC (-11)
#define AMTARI_ENOTEMPTY (-12)
#define AMTARI_ENOEXEC (-13)
#define AMTARI_ENOMEM (-14)
#define AMTARI_EILLEGAL (-15)
#define AMTARI_EXEC_HALTED 0
#define AMTARI_EXEC_RUNNING 1

enum amtari_mode { AMTARI_MODE_NATIVE = 0, AMTARI_MODE_HYBRID, AMTARI_MODE_FULL };
enum amtari_machine { AMTARI_MACHINE_ST = 0, AMTARI_MACHINE_STE, AMTARI_MACHINE_TT, AMTARI_MACHINE_FALCON };
enum amtari_trap_kind { AMTARI_TRAP_UNKNOWN = 0, AMTARI_TRAP_GEMDOS, AMTARI_TRAP_GEM, AMTARI_TRAP_BIOS, AMTARI_TRAP_XBIOS };

typedef int (*amtari_console_getc_fn)(void *opaque);
typedef int (*amtari_console_putc_fn)(void *opaque, unsigned char ch);
typedef int (*amtari_console_status_fn)(void *opaque);
typedef int (*amtari_clock_get_fn)(void *opaque, uint32_t *tos_datetime);
typedef int (*amtari_clock_set_fn)(void *opaque, uint32_t tos_datetime);
typedef int32_t (*amtari_fs_open_fn)(void *opaque, const char *path, uint16_t mode);
typedef int32_t (*amtari_fs_create_fn)(void *opaque, const char *path, uint16_t attr);
typedef int32_t (*amtari_fs_close_fn)(void *opaque, int16_t handle);
typedef int32_t (*amtari_fs_read_fn)(void *opaque, int16_t handle, void *buffer, uint32_t count);
typedef int32_t (*amtari_fs_write_fn)(void *opaque, int16_t handle, const void *buffer, uint32_t count);
typedef int32_t (*amtari_fs_seek_fn)(void *opaque, int16_t handle, int32_t offset, uint16_t mode);
typedef int32_t (*amtari_fs_delete_fn)(void *opaque, const char *path);
typedef int32_t (*amtari_fs_mkdir_fn)(void *opaque, const char *path);
typedef int32_t (*amtari_fs_rmdir_fn)(void *opaque, const char *path);
typedef int (*amtari_program_fetch_fn)(void *opaque, const char *path, const uint8_t **data, size_t *size);
typedef int32_t (*amtari_vdi_dispatch_fn)(void *opaque, uint16_t opcode,
                                         const int16_t *intin, uint16_t intin_count,
                                         const int16_t *ptsin, uint16_t ptsin_count,
                                         int16_t *intout, uint16_t intout_capacity,
                                         uint16_t *intout_count,
                                         int16_t *ptsout, uint16_t ptsout_capacity,
                                         uint16_t *ptsout_count);

struct amtari_cpu_state { uint32_t d[8]; uint32_t a[8]; uint32_t pc; uint16_t sr; };
struct amtari_guest_memory { uint8_t *data; size_t size; };
struct amtari_console_io {
    amtari_console_getc_fn getc;
    amtari_console_putc_fn putc;
    amtari_console_status_fn input_ready;
    amtari_console_status_fn output_ready;
    void *opaque;
};
struct amtari_clock_io { amtari_clock_get_fn get; amtari_clock_set_fn set; void *opaque; };
struct amtari_vdi_io { amtari_vdi_dispatch_fn dispatch; void *opaque; };
struct amtari_fs_io {
    amtari_fs_open_fn open; amtari_fs_create_fn create; amtari_fs_close_fn close;
    amtari_fs_read_fn read; amtari_fs_write_fn write; amtari_fs_seek_fn seek;
    amtari_fs_delete_fn unlink; amtari_fs_mkdir_fn mkdir; amtari_fs_rmdir_fn rmdir; void *opaque;
};
struct amtari_process_io { amtari_program_fetch_fn fetch; void *opaque; };
struct amtari_prg_info { uint32_t text_size, data_size, bss_size, symbol_size, flags; uint16_t absolute; };
struct amtari_mem_block {
    uint32_t address;
    uint32_t size;
    uint32_t owner_basepage;
    uint8_t valid;
    uint8_t in_use;
};

struct amtari_context {
    enum amtari_machine machine; enum amtari_mode mode; struct amtari_cpu_state cpu;
    struct amtari_guest_memory memory; struct amtari_console_io console; struct amtari_clock_io clock;
    struct amtari_vdi_io vdi; struct amtari_fs_io fs; struct amtari_process_io process;
    uint32_t drive_mask; uint8_t current_drive;
    char cwd[26][AMTARI_PATH_MAX]; uint32_t next_load_address; uint32_t current_basepage;
    struct amtari_mem_block mem_blocks[AMTARI_MEM_BLOCK_MAX]; uint32_t heap_top;
    uint32_t random_seed;
    uint8_t process_depth; int initialized;
};

const char *amtari_version(void);
int amtari_init(struct amtari_context *ctx);
int amtari_random_seed(struct amtari_context *ctx, uint32_t seed);
int amtari_clock_bind(struct amtari_context *ctx, amtari_clock_get_fn get_fn, void *opaque);
int amtari_clock_bind_rw(struct amtari_context *ctx, amtari_clock_get_fn get_fn,
                         amtari_clock_set_fn set_fn, void *opaque);
int amtari_vdi_bind(struct amtari_context *ctx, amtari_vdi_dispatch_fn dispatch_fn, void *opaque);
int32_t amtari_vdi_dispatch(struct amtari_context *ctx, uint16_t opcode,
                            const int16_t *intin, uint16_t intin_count,
                            const int16_t *ptsin, uint16_t ptsin_count,
                            int16_t *intout, uint16_t intout_capacity,
                            uint16_t *intout_count,
                            int16_t *ptsout, uint16_t ptsout_capacity,
                            uint16_t *ptsout_count);
int amtari_guest_memory_bind(struct amtari_context *ctx, uint8_t *data, size_t size);
int amtari_guest_range_valid(const struct amtari_context *ctx, uint32_t address, size_t length);
int amtari_guest_read16(const struct amtari_context *ctx, uint32_t address, uint16_t *value);
int amtari_guest_read32(const struct amtari_context *ctx, uint32_t address, uint32_t *value);
int amtari_guest_write16(struct amtari_context *ctx, uint32_t address, uint16_t value);
int amtari_console_bind(struct amtari_context *ctx, amtari_console_getc_fn getc_fn, amtari_console_putc_fn putc_fn, void *opaque);
int amtari_console_status_bind(struct amtari_context *ctx, amtari_console_status_fn input_ready_fn,
                               amtari_console_status_fn output_ready_fn);
int amtari_fs_bind(struct amtari_context *ctx, amtari_fs_open_fn open_fn, amtari_fs_create_fn create_fn,
                   amtari_fs_close_fn close_fn, amtari_fs_read_fn read_fn, amtari_fs_write_fn write_fn,
                   amtari_fs_seek_fn seek_fn, amtari_fs_delete_fn delete_fn, amtari_fs_mkdir_fn mkdir_fn,
                   amtari_fs_rmdir_fn rmdir_fn, void *opaque);
int amtari_fs_set_drives(struct amtari_context *ctx, uint32_t drive_mask, uint8_t current_drive);
int amtari_path_translate(const struct amtari_context *ctx, uint32_t guest_address, char *output, size_t output_size);
int32_t amtari_gemdos_error_from_host(int32_t host_error);
int amtari_program_bind(struct amtari_context *ctx, amtari_program_fetch_fn fetch_fn, void *opaque);
int amtari_process_set_load_address(struct amtari_context *ctx, uint32_t address);
int amtari_prg_parse(const uint8_t *image, size_t image_size, struct amtari_prg_info *info);
int32_t amtari_prg_load(struct amtari_context *ctx, const uint8_t *image, size_t image_size, uint32_t basepage, const uint8_t *cmdline);
int32_t amtari_prg_load_reserved(struct amtari_context *ctx, const uint8_t *image, size_t image_size,
                                uint32_t basepage, const uint8_t *cmdline, uint32_t tail_reserve);
int amtari_exec_prepare(struct amtari_context *ctx, uint32_t basepage, uint32_t stack_top);
int amtari_exec_step(struct amtari_context *ctx);
int amtari_exec_run(struct amtari_context *ctx, uint32_t max_steps, uint32_t *steps_executed);

enum amtari_trap_kind amtari_trap_decode(unsigned int trap_number);
int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function);
int32_t amtari_trap_dispatch(struct amtari_context *ctx, unsigned int trap_number, uint16_t function);

#endif
