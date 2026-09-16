#ifndef AMTARI_H
#define AMTARI_H
#include <stddef.h>
#include <stdint.h>
#define AMTARI_VERSION "0.3.12-m3"
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
enum amtari_mode { AMTARI_MODE_NATIVE=0, AMTARI_MODE_HYBRID, AMTARI_MODE_FULL };
enum amtari_machine { AMTARI_MACHINE_ST=0, AMTARI_MACHINE_STE, AMTARI_MACHINE_TT, AMTARI_MACHINE_FALCON };
enum amtari_trap_kind { AMTARI_TRAP_UNKNOWN=0, AMTARI_TRAP_GEMDOS, AMTARI_TRAP_GEM, AMTARI_TRAP_BIOS, AMTARI_TRAP_XBIOS };
typedef int (*amtari_console_getc_fn)(void *opaque); typedef int (*amtari_console_putc_fn)(void *opaque,unsigned char ch); typedef int (*amtari_console_status_fn)(void *opaque); typedef int (*amtari_clock_get_fn)(void *opaque,uint32_t *tos_datetime); typedef int (*amtari_clock_set_fn)(void *opaque,uint32_t tos_datetime);
typedef int32_t (*amtari_fs_open_fn)(void*,const char*,uint16_t); typedef int32_t (*amtari_fs_create_fn)(void*,const char*,uint16_t); typedef int32_t (*amtari_fs_close_fn)(void*,int16_t); typedef int32_t (*amtari_fs_read_fn)(void*,int16_t,void*,uint32_t); typedef int32_t (*amtari_fs_write_fn)(void*,int16_t,const void*,uint32_t); typedef int32_t (*amtari_fs_seek_fn)(void*,int16_t,int32_t,uint16_t); typedef int32_t (*amtari_fs_delete_fn)(void*,const char*); typedef int32_t (*amtari_fs_mkdir_fn)(void*,const char*); typedef int32_t (*amtari_fs_rmdir_fn)(void*,const char*); typedef int (*amtari_program_fetch_fn)(void*,const char*,const uint8_t**,size_t*);
typedef int32_t (*amtari_vdi_dispatch_fn)(void*,uint16_t,const int16_t*,uint16_t,const int16_t*,uint16_t,int16_t*,uint16_t,uint16_t*,int16_t*,uint16_t,uint16_t*);
typedef int (*amtari_vdi_line_fn)(void *opaque,int16_t x1,int16_t y1,int16_t x2,int16_t y2,uint16_t color,uint16_t style);
typedef int32_t (*amtari_aes_dispatch_fn)(void*,uint16_t,const int16_t*,uint16_t,int16_t*,uint16_t,uint16_t*,const uint32_t*,uint16_t,uint32_t*,uint16_t,uint16_t*);
struct amtari_cpu_state { uint32_t d[8],a[8],pc; uint16_t sr; }; struct amtari_guest_memory { uint8_t *data; size_t size; };
struct amtari_console_io { amtari_console_getc_fn getc; amtari_console_putc_fn putc; amtari_console_status_fn input_ready,output_ready; void *opaque; }; struct amtari_clock_io { amtari_clock_get_fn get; amtari_clock_set_fn set; void *opaque; };
struct amtari_vdi_io { amtari_vdi_dispatch_fn dispatch; amtari_vdi_line_fn line; void *opaque; void *line_opaque; uint16_t width,height,colors,next_handle,line_color,line_style; };
struct amtari_aes_io { amtari_aes_dispatch_fn dispatch; void *opaque; uint16_t next_application_id,current_application_id; uint8_t application_active; };
struct amtari_fs_io { amtari_fs_open_fn open; amtari_fs_create_fn create; amtari_fs_close_fn close; amtari_fs_read_fn read; amtari_fs_write_fn write; amtari_fs_seek_fn seek; amtari_fs_delete_fn unlink; amtari_fs_mkdir_fn mkdir; amtari_fs_rmdir_fn rmdir; void *opaque; }; struct amtari_process_io { amtari_program_fetch_fn fetch; void *opaque; }; struct amtari_prg_info { uint32_t text_size,data_size,bss_size,symbol_size,flags; uint16_t absolute; }; struct amtari_mem_block { uint32_t address,size,owner_basepage; uint8_t valid,in_use; };
struct amtari_context { enum amtari_machine machine; enum amtari_mode mode; struct amtari_cpu_state cpu; struct amtari_guest_memory memory; struct amtari_console_io console; struct amtari_clock_io clock; struct amtari_vdi_io vdi; struct amtari_aes_io aes; struct amtari_fs_io fs; struct amtari_process_io process; uint32_t drive_mask; uint8_t current_drive; char cwd[26][AMTARI_PATH_MAX]; uint32_t next_load_address,current_basepage; struct amtari_mem_block mem_blocks[AMTARI_MEM_BLOCK_MAX]; uint32_t heap_top,random_seed; uint8_t process_depth; int initialized; };
const char *amtari_version(void); int amtari_init(struct amtari_context*); int amtari_random_seed(struct amtari_context*,uint32_t); int amtari_clock_bind(struct amtari_context*,amtari_clock_get_fn,void*); int amtari_clock_bind_rw(struct amtari_context*,amtari_clock_get_fn,amtari_clock_set_fn,void*);
int amtari_vdi_bind(struct amtari_context*,amtari_vdi_dispatch_fn,void*); int amtari_vdi_line_bind(struct amtari_context*,amtari_vdi_line_fn,void*); int amtari_vdi_configure(struct amtari_context*,uint16_t,uint16_t,uint16_t); int32_t amtari_vdi_dispatch(struct amtari_context*,uint16_t,const int16_t*,uint16_t,const int16_t*,uint16_t,int16_t*,uint16_t,uint16_t*,int16_t*,uint16_t,uint16_t*);
int amtari_aes_bind(struct amtari_context*,amtari_aes_dispatch_fn,void*); int32_t amtari_aes_dispatch(struct amtari_context*,uint16_t,const int16_t*,uint16_t,int16_t*,uint16_t,uint16_t*,const uint32_t*,uint16_t,uint32_t*,uint16_t,uint16_t*);
int amtari_guest_memory_bind(struct amtari_context*,uint8_t*,size_t); int amtari_guest_range_valid(const struct amtari_context*,uint32_t,size_t); int amtari_guest_read16(const struct amtari_context*,uint32_t,uint16_t*); int amtari_guest_read32(const struct amtari_context*,uint32_t,uint32_t*); int amtari_guest_write16(struct amtari_context*,uint32_t,uint16_t); int amtari_guest_write32(struct amtari_context*,uint32_t,uint32_t);
int amtari_console_bind(struct amtari_context*,amtari_console_getc_fn,amtari_console_putc_fn,void*); int amtari_console_status_bind(struct amtari_context*,amtari_console_status_fn,amtari_console_status_fn); int amtari_fs_bind(struct amtari_context*,amtari_fs_open_fn,amtari_fs_create_fn,amtari_fs_close_fn,amtari_fs_read_fn,amtari_fs_write_fn,amtari_fs_seek_fn,amtari_fs_delete_fn,amtari_fs_mkdir_fn,amtari_fs_rmdir_fn,void*); int amtari_fs_set_drives(struct amtari_context*,uint32_t,uint8_t); int amtari_path_translate(const struct amtari_context*,uint32_t,char*,size_t); int32_t amtari_gemdos_error_from_host(int32_t); int amtari_program_bind(struct amtari_context*,amtari_program_fetch_fn,void*); int amtari_process_set_load_address(struct amtari_context*,uint32_t); int amtari_prg_parse(const uint8_t*,size_t,struct amtari_prg_info*); int32_t amtari_prg_load(struct amtari_context*,const uint8_t*,size_t,uint32_t,const uint8_t*); int32_t amtari_prg_load_reserved(struct amtari_context*,const uint8_t*,size_t,uint32_t,const uint8_t*,uint32_t); int amtari_exec_prepare(struct amtari_context*,uint32_t,uint32_t); int amtari_exec_step(struct amtari_context*); int amtari_exec_run(struct amtari_context*,uint32_t,uint32_t*); enum amtari_trap_kind amtari_trap_decode(unsigned int); int32_t amtari_gemdos_dispatch(struct amtari_context*,uint16_t); int32_t amtari_trap_dispatch(struct amtari_context*,unsigned int,uint16_t);
#endif
