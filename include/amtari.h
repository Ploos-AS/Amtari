#ifndef AMTARI_H
#define AMTARI_H

#include <stddef.h>
#include <stdint.h>

#define AMTARI_VERSION "0.2.0-m2"
#define AMTARI_EINVAL (-1)
#define AMTARI_EFAULT (-2)
#define AMTARI_ENOSYS (-3)
#define AMTARI_EIO (-4)

enum amtari_mode {
    AMTARI_MODE_NATIVE = 0,
    AMTARI_MODE_HYBRID,
    AMTARI_MODE_FULL
};

enum amtari_machine {
    AMTARI_MACHINE_ST = 0,
    AMTARI_MACHINE_STE,
    AMTARI_MACHINE_TT,
    AMTARI_MACHINE_FALCON
};

enum amtari_trap_kind {
    AMTARI_TRAP_UNKNOWN = 0,
    AMTARI_TRAP_GEMDOS,
    AMTARI_TRAP_BIOS,
    AMTARI_TRAP_XBIOS
};

typedef int (*amtari_console_getc_fn)(void *opaque);
typedef int (*amtari_console_putc_fn)(void *opaque, unsigned char ch);

struct amtari_cpu_state {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
};

struct amtari_guest_memory {
    uint8_t *data;
    size_t size;
};

struct amtari_console_io {
    amtari_console_getc_fn getc;
    amtari_console_putc_fn putc;
    void *opaque;
};

struct amtari_context {
    enum amtari_machine machine;
    enum amtari_mode mode;
    struct amtari_cpu_state cpu;
    struct amtari_guest_memory memory;
    struct amtari_console_io console;
    int initialized;
};

const char *amtari_version(void);
int amtari_init(struct amtari_context *ctx);
int amtari_guest_memory_bind(struct amtari_context *ctx, uint8_t *data, size_t size);
int amtari_guest_range_valid(const struct amtari_context *ctx, uint32_t address, size_t length);
int amtari_guest_read16(const struct amtari_context *ctx, uint32_t address, uint16_t *value);
int amtari_guest_read32(const struct amtari_context *ctx, uint32_t address, uint32_t *value);
int amtari_console_bind(struct amtari_context *ctx, amtari_console_getc_fn getc_fn,
                        amtari_console_putc_fn putc_fn, void *opaque);

enum amtari_trap_kind amtari_trap_decode(unsigned int trap_number);
int32_t amtari_gemdos_dispatch(struct amtari_context *ctx, uint16_t function);
int32_t amtari_trap_dispatch(struct amtari_context *ctx, unsigned int trap_number, uint16_t function);

#endif
