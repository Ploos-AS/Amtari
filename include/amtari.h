#ifndef AMTARI_H
#define AMTARI_H

#include <stddef.h>
#include <stdint.h>

#define AMTARI_VERSION "0.2.12-m2"
#define AMTARI_PATH_MAX 260
#define AMTARI_BASEPAGE_SIZE 256u
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
enum amtari_trap_kind { AMTARI_TRAP_UNKNOWN = 0, AMTARI_TRAP_GEMDOS, AMTARI_TRAP_BIOS, AMTARI_TRAP_XBIOS };

typedef int (*amtari_console_getc_fn)(void *opaque);
typedef int (*amtari_console_putc_fn)(void *opaque, unsigned char ch);
typedef int32_t (*amtari_fs_open_fn)(void *opaque, const char *path, uint16_t mode);
typedef int32_t (*amtari_fs_create_fn)(void *opaque, const char *path, uint16_t attr);
typedef int32_t (*amtari_fs_close_fn)(void *opaque, int16_t handle);
typedef int32_t (*amtari_fs_read_fn)(void *opaque, int16_t handle, void *buffer, uint32_t count);
typedef int32_t (*amtari_fs_write_fn)(void *opaque, int16_t handle, const void *buffer, uint32_t count);
typedef int32_t (*amtari_fs_seek_fn)(void *opaque, int16_t handle, int32_t offset, uint16_t mode);
typedef int32_t (*amtari_fs_delete_fn)(void *opaque, const char *path);