#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "amtari.h"

struct fake_fs {
    char last_path[AMTARI_PATH_MAX];
    uint16_t last_mode;
    uint16_t last_attr;
    int16_t last_handle;
    int32_t last_offset;
    uint8_t write_buf[16];
    uint32_t write_count;
};

static void put16(uint8_t *memory, uint32_t address, uint16_t value)
{
    memory[address] = (uint8_t)(value >> 8);
    memory[address + 1u] = (uint8_t)value;
}

static void put32(uint8_t *memory, uint32_t address, uint32_t value)
{
    memory[address] = (uint8_t)(value >> 24);
    memory[address + 1u] = (uint8_t)(value >> 16);
    memory[address + 2u] = (uint8_t)(value >> 8);
    memory[address + 3u] = (uint8_t)value;
}

static int32_t fs_open(void *opaque, const char *path, uint16_t mode)
{
    struct fake_fs *fs = opaque;
    strcpy(fs->last_path, path);
    fs->last_mode = mode;
    return 7;
}

static int32_t fs_create(void *opaque, const char *path, uint16_t attr)
{
    struct fake_fs *fs = opaque;
    strcpy(fs->last_path, path);
    fs->last_attr = attr;
    return 8;
}

static int32_t fs_close(void *opaque, int16_t handle)
{
    struct fake_fs *fs = opaque;
    fs->last_handle = handle;
    return 0;
}

static int32_t fs_read(void *opaque, int16_t handle, void *buffer, uint32_t count)
{
    struct fake_fs *fs = opaque;
    static const char data[] = "DATA";
    uint32_t n = count < 4u ? count : 4u;
    fs->last_handle = handle;
    memcpy(buffer, data, n);
    return (int32_t)n;
}

static int32_t fs_write(void *opaque, int16_t handle, const void *buffer, uint32_t count)
{
    struct fake_fs *fs = opaque;
    assert(count <= sizeof(fs->write_buf));
    fs->last_handle = handle;
    memcpy(fs->write_buf, buffer, count);
    fs->write_count = count;
    return (int32_t)count;
}

static int32_t fs_seek(void *opaque, int16_t handle, int32_t offset, uint16_t mode)
{
    struct fake_fs *fs = opaque;
    fs->last_handle = handle;
    fs->last_offset = offset;
    fs->last_mode = mode;
    return 1234;
}

static int32_t fs_delete(void *opaque, const char *path)
{
    struct fake_fs *fs = opaque;
    strcpy(fs->last_path, path);
    return 0;
}

static int32_t fs_mkdir(void *opaque, const char *path)
{
    struct fake_fs *fs = opaque;
    strcpy(fs->last_path, path);
    return 0;
}

static int32_t fs_rmdir(void *opaque, const char *path)
{
    struct fake_fs *fs = opaque;
    strcpy(fs->last_path, path);
    return 0;
}

int main(void)
{
    struct amtari_context ctx = {0};
    struct fake_fs fs = {{0}, 0, 0, 0, 0, {0}, 0};
    uint8_t memory[512] = {0};
    char translated[AMTARI_PATH_MAX];

    assert(amtari_init(&ctx) == 0);
    assert(strcmp(amtari_version(), "0.2.2-m2") == 0);
    assert(amtari_guest_memory_bind(&ctx, memory, sizeof(memory)) == 0);
    assert(amtari_fs_bind(&ctx, fs_open, fs_create, fs_close, fs_read, fs_write,
                          fs_seek, fs_delete, fs_mkdir, fs_rmdir, &fs) == 0);
    assert(amtari_fs_set_drives(&ctx, (1u << 0) | (1u << 2) | (1u << 3), 2) == 0);

    strcpy((char *)&memory[128], "D:\\AUTO\\TEST.PRG");
    assert(amtari_path_translate(&ctx, 128, translated, sizeof(translated)) == 0);
    assert(strcmp(translated, "D:/AUTO/TEST.PRG") == 0);

    strcpy((char *)&memory[160], "README.TXT");
    assert(amtari_path_translate(&ctx, 160, translated, sizeof(translated)) == 0);
    assert(strcmp(translated, "C:/README.TXT") == 0);

    ctx.cpu.a[7] = 32;
    put16(memory, 34, 3);
    assert(amtari_gemdos_dispatch(&ctx, 0x0e) == (int32_t)((1u << 0) | (1u << 2) | (1u << 3)));
    assert(amtari_gemdos_dispatch(&ctx, 0x19) == 3);

    put32(memory, 34, 128);
    put16(memory, 38, 2);
    assert(amtari_gemdos_dispatch(&ctx, 0x3d) == 7);
    assert(strcmp(fs.last_path, "D:/AUTO/TEST.PRG") == 0);
    assert(fs.last_mode == 2);

    put16(memory, 34, 7);
    assert(amtari_gemdos_dispatch(&ctx, 0x3e) == 0);
    assert(fs.last_handle == 7);

    put16(memory, 34, 7);
    put32(memory, 36, 4);
    put32(memory, 40, 300);
    assert(amtari_gemdos_dispatch(&ctx, 0x3f) == 4);
    assert(memcmp(&memory[300], "DATA", 4) == 0);

    memcpy(&memory[320], "PING", 4);
    put16(memory, 34, 7);
    put32(memory, 36, 4);
    put32(memory, 40, 320);
    assert(amtari_gemdos_dispatch(&ctx, 0x40) == 4);
    assert(fs.write_count == 4);
    assert(memcmp(fs.write_buf, "PING", 4) == 0);

    strcpy((char *)&memory[192], "TOOLS");
    put32(memory, 34, 192);
    assert(amtari_gemdos_dispatch(&ctx, 0x39) == 0);
    assert(strcmp(fs.last_path, "D:/TOOLS") == 0);

    assert(amtari_gemdos_dispatch(&ctx, 0x3b) == 0);
    assert(strcmp(ctx.cwd[3], "TOOLS") == 0);

    put32(memory, 34, 360);
    put16(memory, 38, 4);
    assert(amtari_gemdos_dispatch(&ctx, 0x47) == 0);
    assert(strcmp((char *)&memory[360], "TOOLS") == 0);

    strcpy((char *)&memory[210], "NEW.TXT");
    put32(memory, 34, 210);
    put16(memory, 38, 0x20);
    assert(amtari_gemdos_dispatch(&ctx, 0x3c) == 8);
    assert(strcmp(fs.last_path, "D:/TOOLS/NEW.TXT") == 0);
    assert(fs.last_attr == 0x20);

    put32(memory, 34, 0xfffffff0u);
    put16(memory, 38, 8);
    put16(memory, 40, 2);
    assert(amtari_gemdos_dispatch(&ctx, 0x42) == 1234);
    assert(fs.last_handle == 8);
    assert(fs.last_offset == -16);
    assert(fs.last_mode == 2);

    strcpy((char *)&memory[230], "OLD.TXT");
    put32(memory, 34, 230);
    assert(amtari_gemdos_dispatch(&ctx, 0x41) == 0);
    assert(strcmp(fs.last_path, "D:/TOOLS/OLD.TXT") == 0);

    strcpy((char *)&memory[250], "EMPTY");
    put32(memory, 34, 250);
    assert(amtari_gemdos_dispatch(&ctx, 0x3a) == 0);
    assert(strcmp(fs.last_path, "D:/TOOLS/EMPTY") == 0);

    put32(memory, 40, 510);
    put16(memory, 34, 7);
    put32(memory, 36, 4);
    assert(amtari_gemdos_dispatch(&ctx, 0x3f) == AMTARI_EFAULT);

    assert(amtari_gemdos_error_from_host(-999) == AMTARI_EIO);
    assert(amtari_gemdos_error_from_host(AMTARI_ENOENT) == AMTARI_ENOENT);

    return 0;
}
