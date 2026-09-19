#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "amtari.h"

struct timer_fixture { int calls; uint32_t ms; };
struct key_fixture { int ready, key, reads; };
struct mouse_fixture { int ready; int16_t x, y; uint16_t buttons, kstate; };
struct cursor_fixture { int calls; uint16_t mode; uint32_t form; };
static int timer(void *o, uint32_t ms) { struct timer_fixture *f=o; f->calls++; f->ms=ms; return 0; }
static int kready(void *o) { return ((struct key_fixture *)o)->ready; }
static int kget(void *o) { struct key_fixture *f=o; f->reads++; f->ready=0; return f->key; }
static int kput(void *o, unsigned char c) { (void)o; (void)c; return 0; }
static int mouse(void *o, int16_t *x, int16_t *y, uint16_t *b, uint16_t *k) { struct mouse_fixture *f=o; if (!f->ready) return 0; *x=f->x; *y=f->y; *b=f->buttons; *k=f->kstate; return 1; }
static int cursor(void *o,uint16_t mode,uint32_t form) { struct cursor_fixture *f=o; f->calls++; f->mode=mode; f->form=form; return 0; }
static void w16(uint8_t *m, uint32_t a, uint16_t v) { m[a]=(uint8_t)(v>>8); m[a+1]=(uint8_t)v; }
static void w32(uint8_t *m, uint32_t a, uint32_t v) { w16(m,a,(uint16_t)(v>>16)); w16(m,a+2,(uint16_t)v); }
static void apb(uint8_t *m, uint16_t op, uint16_t ni, uint16_t no, uint16_t nai) { w32(m,0x300,0x340); w32(m,0x304,0x360); w32(m,0x308,0x380); w32(m,0x30c,0x3c0); w32(m,0x310,0x400); w32(m,0x314,0x440); w16(m,0x340,op); w16(m,0x342,ni); w16(m,0x344,no); w16(m,0x346,nai); w16(m,0x348,0); }
static void trap2(struct amtari_context *c, uint8_t *m) { w16(m,0x20,0x4e42); c->cpu.pc=0x20; c->cpu.d[0]=0xc8; c->cpu.d[1]=0x300; assert(amtari_exec_step(c)==AMTARI_EXEC_RUNNING); }
static uint16_t r16(struct amtari_context *c, uint32_t a) { uint16_t v=0; assert(!amtari_guest_read16(c,a,&v)); return v; }

int main(void) {
 struct amtari_context c={0}; uint8_t mem[2048]; uint16_t id; unsigned i;
 struct timer_fixture t={0}; struct key_fixture key={1,0x1e61,0}; struct mouse_fixture m={1,123,77,1,4}; struct cursor_fixture cur={0};
 assert(!amtari_init(&c)); memset(mem,0,sizeof(mem)); assert(!amtari_guest_memory_bind(&c,mem,sizeof(mem))); assert(!amtari_aes_set_application_name(&c,"AMTARI"));
 assert(!amtari_console_bind(&c,kget,kput,&key)); assert(!amtari_console_status_bind(&c,kready,0));
 apb(mem,10,0,1,0);trap2(&c,mem);id=r16(&c,0x3c0);assert(id==1);
 memcpy(mem+0x500,"AMTARI",7);apb(mem,13,0,1,1);w32(mem,0x400,0x500);trap2(&c,mem);assert(r16(&c,0x3c0)==id);
 for(i=0;i<16;i++){mem[0x520+i]=(uint8_t)(0xa0+i);} apb(mem,12,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x520);trap2(&c,mem);memset(mem+0x540,0,16);apb(mem,11,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x540);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&!memcmp(mem+0x520,mem+0x540,16));
 apb(mem,12,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x520);trap2(&c,mem);apb(mem,23,0,1,1);w32(mem,0x400,0x560);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&!memcmp(mem+0x520,mem+0x560,16));
 assert(!amtari_aes_timer_bind(&c,timer,&t));apb(mem,24,2,1,0);w16(mem,0x380,25);w16(mem,0x382,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&t.ms==25);
 key.ready=1;key.key=0x1e61;apb(mem,20,0,1,0);trap2(&c,mem);assert(r16(&c,0x3c0)==0x1e61&&key.reads==1);
 key.ready=0;apb(mem,20,0,1,0);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 assert(!amtari_aes_mouse_bind(&c,mouse,&m));
 apb(mem,77,0,5,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==8&&r16(&c,0x3c4)==16&&r16(&c,0x3c6)==8&&r16(&c,0x3c8)==16);
 assert(!amtari_aes_graf_configure(&c,7,9,18,10,20));apb(mem,77,0,5,0);trap2(&c,mem);assert(r16(&c,0x3c0)==7&&r16(&c,0x3c2)==9&&r16(&c,0x3c4)==18&&r16(&c,0x3c6)==10&&r16(&c,0x3c8)==20);
 /* graf_slidebox opcode 76: host-neutral slider reports the initial normalized position */
 apb(mem,76,3,1,1);w16(mem,0x380,1);w16(mem,0x382,2);w16(mem,0x384,0);w32(mem,0x400,0x600);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* graf_watchbox opcode 75: cooperative button-state result for a guest object tree */
 m.ready=1;m.buttons=1;apb(mem,75,3,1,1);w16(mem,0x380,2);w16(mem,0x382,1);w16(mem,0x384,0);w32(mem,0x400,0x600);trap2(&c,mem);assert(r16(&c,0x3c0)==1);
 m.buttons=0;apb(mem,75,3,1,1);w16(mem,0x380,2);w16(mem,0x382,1);w16(mem,0x384,0);w32(mem,0x400,0x600);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* graf_shrinkbox opcode 74: host-neutral animation hint from source to destination rectangle */
 apb(mem,74,8,1,0);w16(mem,0x380,320);w16(mem,0x382,180);w16(mem,0x384,30);w16(mem,0x386,20);w16(mem,0x388,100);w16(mem,0x38a,60);w16(mem,0x38c,20);w16(mem,0x38e,12);trap2(&c,mem);assert(r16(&c,0x3c0)==1);
 /* graf_growbox opcode 73: host-neutral animation hint from source to destination rectangle */
 apb(mem,73,8,1,0);w16(mem,0x380,100);w16(mem,0x382,60);w16(mem,0x384,20);w16(mem,0x386,12);w16(mem,0x388,30);w16(mem,0x38a,20);w16(mem,0x38c,320);w16(mem,0x38e,180);trap2(&c,mem);assert(r16(&c,0x3c0)==1);
 /* graf_movebox opcode 72: animation hint completes successfully in host-neutral backend */
 apb(mem,72,6,1,0);w16(mem,0x380,40);w16(mem,0x382,20);w16(mem,0x384,10);w16(mem,0x386,15);w16(mem,0x388,120);w16(mem,0x38a,80);trap2(&c,mem);assert(r16(&c,0x3c0)==1);
 /* graf_dragbox opcode 71: cooperative final mouse position clamped to bounds */
 m.x=180;m.y=120;apb(mem,71,8,3,0);w16(mem,0x380,20);w16(mem,0x382,10);w16(mem,0x384,100);w16(mem,0x386,60);w16(mem,0x388,50);w16(mem,0x38a,40);w16(mem,0x38c,100);w16(mem,0x38e,80);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==130&&r16(&c,0x3c4)==110);
 m.x=70;m.y=55;apb(mem,71,8,3,0);w16(mem,0x380,20);w16(mem,0x382,10);w16(mem,0x384,100);w16(mem,0x386,60);w16(mem,0x388,50);w16(mem,0x38a,40);w16(mem,0x38c,100);w16(mem,0x38e,80);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==70&&r16(&c,0x3c4)==55);
 /* graf_rubberbox opcode 70: cooperative mouse snapshot with minimum dimensions */
 m.x=140;m.y=95;apb(mem,70,4,3,0);w16(mem,0x380,100);w16(mem,0x382,50);w16(mem,0x384,20);w16(mem,0x386,30);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==41&&r16(&c,0x3c4)==46);
 m.x=105;m.y=55;apb(mem,70,4,3,0);w16(mem,0x380,100);w16(mem,0x382,50);w16(mem,0x384,20);w16(mem,0x386,30);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==20&&r16(&c,0x3c4)==30);
 /* graf_mouse opcode 78: built-in shape, hide/show and USER_DEF guest pointer */
 assert(!amtari_aes_cursor_bind(&c,cursor,&cur));apb(mem,78,1,1,1);w16(mem,0x380,3);w32(mem,0x400,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&cur.calls==1&&cur.mode==3&&cur.form==0);
 apb(mem,78,1,1,1);w16(mem,0x380,256);w32(mem,0x400,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&cur.mode==256);
 apb(mem,78,1,1,1);w16(mem,0x380,257);w32(mem,0x400,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&cur.mode==257);
 apb(mem,78,1,1,1);w16(mem,0x380,255);w32(mem,0x400,0x600);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&cur.mode==255&&cur.form==0x600);
 m.x=321;m.y=199;m.buttons=3;m.kstate=12;apb(mem,79,0,5,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==321&&r16(&c,0x3c4)==199&&r16(&c,0x3c6)==3&&r16(&c,0x3c8)==12);
 m.x=123;m.y=77;m.buttons=1;m.kstate=4;apb(mem,21,3,5,0);w16(mem,0x380,1);w16(mem,0x382,1);w16(mem,0x384,1);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==123&&r16(&c,0x3c4)==77&&r16(&c,0x3c6)==1&&r16(&c,0x3c8)==4);
 m.buttons=0;apb(mem,21,3,5,0);w16(mem,0x380,1);w16(mem,0x382,1);w16(mem,0x384,1);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 m.buttons=1;apb(mem,22,5,5,0);w16(mem,0x380,0);w16(mem,0x382,100);w16(mem,0x384,50);w16(mem,0x386,50);w16(mem,0x388,50);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==123&&r16(&c,0x3c4)==77&&r16(&c,0x3c6)==1&&r16(&c,0x3c8)==4);
 m.x=10;m.y=10;apb(mem,22,5,5,0);w16(mem,0x380,1);w16(mem,0x382,100);w16(mem,0x384,50);w16(mem,0x386,50);w16(mem,0x388,50);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3c2)==10&&r16(&c,0x3c4)==10);
 apb(mem,12,2,1,1);w16(mem,0x380,id);w16(mem,0x382,16);w32(mem,0x400,0x520);trap2(&c,mem);apb(mem,25,16,7,1);w16(mem,0x380,0x0010);w32(mem,0x400,0x580);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0010);
 key.ready=1;key.key=0x1e61;apb(mem,25,16,7,0);w16(mem,0x380,1);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&r16(&c,0x3ca)==0x1e61);
 m.x=123;m.y=77;m.buttons=1;m.kstate=4;apb(mem,25,16,7,0);w16(mem,0x380,2);w16(mem,0x382,1);w16(mem,0x384,1);w16(mem,0x386,1);trap2(&c,mem);assert(r16(&c,0x3c0)==2);
 apb(mem,25,16,7,0);w16(mem,0x380,4);w16(mem,0x388,0);w16(mem,0x38a,100);w16(mem,0x38c,50);w16(mem,0x38e,50);w16(mem,0x390,50);trap2(&c,mem);assert(r16(&c,0x3c0)==4);
 apb(mem,25,16,7,0);w16(mem,0x380,8);w16(mem,0x392,0);w16(mem,0x394,100);w16(mem,0x396,50);w16(mem,0x398,50);w16(mem,0x39a,50);trap2(&c,mem);assert(r16(&c,0x3c0)==8);
 m.x=10;m.y=10;apb(mem,25,16,7,0);w16(mem,0x380,0x24);w16(mem,0x388,0);w16(mem,0x38a,100);w16(mem,0x38c,50);w16(mem,0x38e,50);w16(mem,0x390,50);w16(mem,0x39c,7);trap2(&c,mem);assert(r16(&c,0x3c0)==0x20&&t.ms==7);
 key.ready=1;key.key=0x3062;m.x=123;m.y=77;m.buttons=1;m.kstate=4;apb(mem,25,16,7,0);w16(mem,0x380,0x0f);w16(mem,0x382,1);w16(mem,0x384,1);w16(mem,0x386,1);w16(mem,0x388,0);w16(mem,0x38a,100);w16(mem,0x38c,50);w16(mem,0x38e,50);w16(mem,0x390,50);w16(mem,0x392,0);w16(mem,0x394,100);w16(mem,0x396,50);w16(mem,0x398,50);w16(mem,0x39a,50);trap2(&c,mem);assert(r16(&c,0x3c0)==0x0f&&r16(&c,0x3ca)==0x3062&&r16(&c,0x3cc)==1);
 /* wind_create opcode 100: allocate stable non-zero AES window handles */
 apb(mem,100,5,1,0);w16(mem,0x380,0x000f);w16(mem,0x382,10);w16(mem,0x384,20);w16(mem,0x386,320);w16(mem,0x388,180);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,100,5,1,0);trap2(&c,mem);assert(r16(&c,0x3c0)==2);
 /* wind_calc opcode 108: host-neutral rectangle calculation foundation */
 apb(mem,108,6,5,0);w16(mem,0x380,0);w16(mem,0x382,0x000f);w16(mem,0x384,10);w16(mem,0x386,20);w16(mem,0x388,320);w16(mem,0x38a,180);trap2(&c,mem);assert(r16(&c,0x3c0)==1);assert(r16(&c,0x3c2)==10&&r16(&c,0x3c4)==20&&r16(&c,0x3c6)==320&&r16(&c,0x3c8)==180);
 /* wind_update opcode 107: begin/end update contract */
 apb(mem,107,1,1,0);w16(mem,0x380,1);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,107,1,1,0);w16(mem,0x380,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,107,1,1,0);w16(mem,0x380,7);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* wind_find opcode 106: locate an open window containing the point */
 apb(mem,106,2,1,0);w16(mem,0x380,30);w16(mem,0x382,40);trap2(&c,mem);assert(r16(&c,0x3c0)==0);apb(mem,101,5,1,0);w16(mem,0x380,2);w16(mem,0x382,25);w16(mem,0x384,35);w16(mem,0x386,280);w16(mem,0x388,150);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,106,2,1,0);w16(mem,0x380,30);w16(mem,0x382,40);trap2(&c,mem);assert(r16(&c,0x3c0)==2);apb(mem,106,2,1,0);w16(mem,0x380,500);w16(mem,0x382,390);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* wind_set opcode 105: accept allocated handle/current rectangle and reject unknown handle */
 apb(mem,105,6,1,0);w16(mem,0x380,1);w16(mem,0x382,5);w16(mem,0x384,20);w16(mem,0x386,30);w16(mem,0x388,300);w16(mem,0x38a,160);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,105,2,1,0);w16(mem,0x380,99);w16(mem,0x382,5);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* stateful wind_set/wind_get: WF_CURRXYWH round-trip */
 apb(mem,105,6,1,0);w16(mem,0x380,2);w16(mem,0x382,5);w16(mem,0x384,25);w16(mem,0x386,35);w16(mem,0x388,280);w16(mem,0x38a,150);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,104,2,5,0);w16(mem,0x380,2);w16(mem,0x382,5);trap2(&c,mem);assert(r16(&c,0x3c0)==1);assert(r16(&c,0x3c2)==25&&r16(&c,0x3c4)==35&&r16(&c,0x3c6)==280&&r16(&c,0x3c8)==150);
 /* wind_get opcode 104: query stored window rectangle */
 apb(mem,104,2,5,0);w16(mem,0x380,1);w16(mem,0x382,4);trap2(&c,mem);assert(r16(&c,0x3c0)==1);assert(r16(&c,0x3c2)==20&&r16(&c,0x3c4)==30&&r16(&c,0x3c6)==300&&r16(&c,0x3c8)==160);
 /* wind_open opcode 101: accept allocated handles and reject unknown ones */
 apb(mem,101,5,1,0);w16(mem,0x380,1);w16(mem,0x382,10);w16(mem,0x384,20);w16(mem,0x386,320);w16(mem,0x388,180);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,101,5,1,0);w16(mem,0x380,99);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* wind_close opcode 102: accept allocated handles and reject unknown ones */
 apb(mem,102,1,1,0);w16(mem,0x380,1);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,102,1,1,0);w16(mem,0x380,99);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* wind_delete opcode 103: accept allocated handles and reject unknown ones */
 apb(mem,103,1,1,0);w16(mem,0x380,1);trap2(&c,mem);assert(r16(&c,0x3c0)==1);apb(mem,103,1,1,0);w16(mem,0x380,99);trap2(&c,mem);assert(r16(&c,0x3c0)==0);
 /* fsel_exinput opcode 91: extended selector accepts path, filename and title buffers */
 strcpy((char*)mem+0x680,"C:\\*.*");strcpy((char*)mem+0x6c0,"README.TXT");strcpy((char*)mem+0x700,"Open file");apb(mem,91,1,1,3);w16(mem,0x380,0);w32(mem,0x400,0x680);w32(mem,0x404,0x6c0);w32(mem,0x408,0x700);trap2(&c,mem);assert(r16(&c,0x3c0)==1);
 /* fsel_input opcode 90: host-neutral selector returns success without a UI backend */
 apb(mem,90,1,1,1);w16(mem,0x380,0);w32(mem,0x400,0x680);trap2(&c,mem);assert(r16(&c,0x3c0)==1);
 /* scrp_write opcode 81: update AES clipboard directory from guest string */
 strcpy((char*)mem+0x640,"D:\\SCRAP\\");apb(mem,81,0,1,1);w32(mem,0x400,0x640);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&!strcmp(c.aes.scrap_path,"D:\\SCRAP\\"));
 /* scrp_read opcode 80: copy current AES clipboard directory to guest buffer */
 strcpy(c.aes.scrap_path,"C:\\CLIPBRD\\");memset(mem+0x620,0xaa,32);apb(mem,80,0,1,1);w32(mem,0x400,0x620);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&!strcmp((char*)mem+0x620,"C:\\CLIPBRD\\"));
 apb(mem,19,0,1,0);trap2(&c,mem);assert(r16(&c,0x3c0)==1&&!c.aes.application_active);return 0;
}
