/* SPDX-License-Identifier: MIT */
/* Synthetic original-scene and GUI fixture; no firmware bytes. Checks input
 * routing, native-call continuations, paging and transform/hitbox geometry. */
#define WIILLR_MENU_TRANSFORM 1
#define WIILLR_MENU_FRAME_MODE 1
#include "../src/menu_grid.c"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static CPUState cpu;
u8* gcn_mem_resolve(CPUState* c,u32 a,u32* size) {
    a&=0x1fffffff;if(a>=c->ram_size){*size=0;return NULL;}*size=c->ram_size-a;return c->ram+a;
}
void mem_write8_cia(CPUState* c,u32 a,u8 v,u32 cia){(void)cia;u32 n;u8* p=gcn_mem_resolve(c,a,&n);CHECK(p);*p=v;}
void mem_write16_cia(CPUState* c,u32 a,u16 v,u32 cia){mem_write8_cia(c,a,v>>8,cia);mem_write8_cia(c,a+1,v,cia);}
void mem_write32_cia(CPUState* c,u32 a,u32 v,u32 cia){mem_write16_cia(c,a,v>>16,cia);mem_write16_cia(c,a+2,v,cia);}
void mem_write8_legacy(CPUState* c,u32 a,u8 v){mem_write8_cia(c,a,v,c->pc);}
void mem_write16_legacy(CPUState* c,u32 a,u16 v){mem_write16_cia(c,a,v,c->pc);}
void mem_write32_legacy(CPUState* c,u32 a,u32 v){mem_write32_cia(c,a,v,c->pc);}
enum {SCENE=0x80001000,LAYOUT=0x80002000,GUI=0x80003000,PARENT=0x80004000};
static void finish_calls(void) {
    unsigned calls=0;
    while(grid.call.active) {
        CHECK(++calls<=6);
        if(cpu.pc==GET_SCENE)cpu.gpr[3]=0x80080000;
        cpu.pc=COMMON;wii_menu_grid_entry(&cpu,COMMON);
    }
    CHECK(cpu.lr==0x80009900 && cpu.gpr[3]==SCENE);
}
static void frame(void) {cpu.gpr[3]=SCENE;cpu.lr=0x80009900;cpu.pc=COMMON;wii_menu_grid_entry(&cpu,COMMON);finish_calls();}
static void init(void) {
    memset(cpu.ram,0,cpu.ram_size);wii_menu_grid_reset();wii_menu_grid_configure(true);
    cpu.cycle_deadline=UINT64_MAX;cpu.gpr[1]=0x800ff000;
    mem_write32(&cpu,SCENE,0x8164e0b8);mem_write32(&cpu,SCENE+0x68,LAYOUT);mem_write32(&cpu,SCENE+0x7c,GUI);
    mem_write32(&cpu,SCENE+0xcc,4);mem_write32(&cpu,SCENE+0xc0,1);put(&cpu,SCENE+0xec,832.f/608.f);
    put(&cpu,PARENT+0x84,832.f/608.f);put(&cpu,PARENT+0x98,1.f);
    uint32_t previous=0;
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
        uint32_t node=0x80005000+(b*12+i)*0x20,pane=0x80010000+(b*12+i)*0x200,comp=0x80020000+(b*12+i)*0x200;
        mem_write32(&cpu,node,pane);mem_write32(&cpu,node+4,comp);mem_write32(&cpu,comp+0x94,pane);
        if(previous)mem_write32(&cpu,previous+12,node);else mem_write32(&cpu,GUI+0x18,node);previous=node;
        char name[17];snprintf(name,sizeof name,"N_Ch_%c%02u",'b'+b,i+1);
        memcpy(cpu.ram+(pane&0x1fffffff)+0xb4,name,strlen(name)+1);
        mem_write32(&cpu,pane+0xc,PARENT);put(&cpu,pane+0x44,1);put(&cpu,pane+0x48,1);
        mem_write8(&cpu,pane+0xcf,1);mem_write8(&cpu,comp+0x7c,b==1);
    }
}
int main(void) {
    cpu.ram_size=1024*1024;cpu.ram=calloc(1,cpu.ram_size);CHECK(cpu.ram);
    init();wii_menu_grid_extent(540,960,true);frame();CHECK(grid.ready && grid.columns==2 && grid.pages==8 && grid.count==6);
    /* Original slot 5 is the last tile on the first portrait page. */
    uint32_t pane=grid.slots[1][5].pane;wii_menu_grid_transform(&cpu,pane);
    float x,y;tile_position(5,&x,&y);CHECK(fabsf(real(&cpu,pane+0x2c)*real(&cpu,PARENT+0x84)-x)<.001f);
    CHECK(fabsf(real(&cpu,pane+0x30)-y)<.001f);
    for(unsigned page=0;page<8;page++) {
        CHECK(grid.first==page*6 && grid.count==6);
        for(unsigned flat=0;flat<48;flat++) {
            cpu.gpr[3]=SCENE;cpu.gpr[4]=flat/12;cpu.gpr[5]=flat%12;
            CHECK(wii_menu_grid_entry(&cpu,0x813ad55c));CHECK(cpu.gpr[3]==(flat>=page*6 && flat<page*6+6));
        }
        if(page<7){cpu.gpr[3]=SCENE;cpu.gpr[4]=13;CHECK(wii_menu_grid_entry(&cpu,0x813ad8e0));frame();}
    }
    /* Carrier remains a genuine stock page even with eight virtual pages. */
    CHECK(word(&cpu,SCENE+0xc8)==3 && grid.page==7);
    cpu.gpr[3]=SCENE;cpu.gpr[4]=0;cpu.gpr[5]=ENCODED|47;CHECK(!wii_menu_grid_entry(&cpu,0x813ace78));
    CHECK(cpu.gpr[4]==3 && cpu.gpr[5]==11);
    wii_menu_grid_extent(1680,720,true);frame();CHECK(grid.columns==6 && grid.pages==3 && grid.first==36);
    wii_menu_grid_extent(1960,720,true);frame();CHECK(grid.columns==7 && grid.first==42);
    wii_menu_grid_configure(false);CHECK(real(&cpu,pane+0x44)==1.f && real(&cpu,pane+0x48)==1.f);
    cpu.gpr[3]=SCENE;cpu.gpr[4]=0;cpu.gpr[5]=0;CHECK(!wii_menu_grid_entry(&cpu,0x813ad55c));
    free(cpu.ram);puts("Menu grid mapping, paging, native continuations and stock restoration passed");return 0;
}
