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
        CHECK(++calls<=12);
        if(cpu.pc==GET_SCENE)cpu.gpr[3]=0x80080000;
        cpu.pc=COMMON;wii_menu_grid_entry(&cpu,COMMON);
    }
    CHECK(cpu.lr==0x80009900 && cpu.gpr[3]==SCENE);
}
static void frame(void) {cpu.gpr[3]=SCENE;cpu.lr=0x80009900;cpu.pc=COMMON;wii_menu_grid_entry(&cpu,COMMON);finish_calls();}
static void scroll(int direction) {
    unsigned previous=grid.page;
    cpu.gpr[3]=SCENE;cpu.gpr[4]=direction<0?12:13;
    CHECK(!wii_menu_grid_entry(&cpu,0x813ad8e0));
    CHECK(grid.scroll.active && grid.page==previous);
    mem_write32(&cpu,SCENE+0xc0,direction<0?12:13);cpu.gpr[31]=SCENE;
    CHECK(wii_menu_grid_entry(&cpu,direction<0?0x813ac630:0x813ac674));
    unsigned carrier=cpu.gpr[direction<0?0:5]+direction;CHECK(carrier<4);
    mem_write32(&cpu,SCENE+0xc8,carrier);cpu.gpr[3]=SCENE;
    CHECK(!wii_menu_grid_entry(&cpu,REFRESH));
    CHECK(!grid.scroll.active && grid.page==previous+direction);
    mem_write32(&cpu,SCENE+0xc0,1);frame();
}
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
    uint32_t pane=pane_for(5);CHECK(pane);wii_menu_grid_transform(&cpu,pane);
    float x,y;tile_position(5,&x,&y);CHECK(fabsf(real(&cpu,pane+0x2c)*real(&cpu,PARENT+0x84)-x)<.001f);
    CHECK(fabsf(real(&cpu,pane+0x30)-y)<.001f);
    for(unsigned page=0;page<8;page++) {
        CHECK(grid.first==page*6 && grid.count==6);
        for(unsigned flat=0;flat<48;flat++) {
            cpu.gpr[3]=SCENE;cpu.gpr[4]=flat/12;cpu.gpr[5]=flat%12;
            bool expected=flat>=page*6 && flat<page*6+6;
            if(page && flat>=(page-1)*6 && flat<page*6 && flat%2==1)expected=true;
            if(page<7 && flat>=(page+1)*6 && flat<(page+2)*6 && flat%2==0)expected=true;
            CHECK(wii_menu_grid_entry(&cpu,0x813ad55c));CHECK(cpu.gpr[3]==expected);
            if(expected)CHECK(pane_for(flat));
        }
        CHECK(grid.shown_count<=36);
        if(page<7)scroll(1);
    }
    /* Carrier remains a genuine stock page even with eight virtual pages. */
    CHECK(word(&cpu,SCENE+0xc8)==3 && grid.page==7);
    cpu.gpr[3]=SCENE;cpu.gpr[4]=0;cpu.gpr[5]=ENCODED|47;CHECK(!wii_menu_grid_entry(&cpu,0x813ace78));
    CHECK(cpu.gpr[4]==3 && cpu.gpr[5]==11);
    wii_menu_grid_extent(1680,720,true);frame();CHECK(grid.columns==6 && grid.pages==3 && grid.first==36);
    wii_menu_grid_extent(1960,720,true);frame();CHECK(grid.columns==7 && grid.first==42);
    /* Physical aspect of a footer button equals its stock 16:9 aspect. */
    unsigned footer=0x80060000;mem_write32(&cpu,footer+0xc,PARENT);put(&cpu,footer+0x2c,294);
    put(&cpu,footer+0x44,1);put(&cpu,footer+0x48,1);footer_add(&cpu,footer,2);
    wii_menu_grid_extent(640,960,true);frame();wii_menu_grid_transform(&cpu,footer);
    float physical=real(&cpu,PARENT+0x84)*real(&cpu,footer+0x44)*(640.f/960.f)/(832.f/456.f)/real(&cpu,footer+0x48);
    CHECK(fabsf(physical-4.f/3.f)<.0001f);
    /* Disc foreground and backing share the same presentation scale. */
    unsigned obj=0x80070000,thumb=0x80071000,inner=0x80072000,overlay=0x80073000,outer=0x80074000;
    unsigned background=0x80075000,canvas=0x80076000;
    mem_write32(&cpu,SCENE+0x58,obj);mem_write32(&cpu,obj+0x30,thumb);mem_write32(&cpu,thumb+0x14,inner);
    mem_write32(&cpu,SCENE+0x9c,overlay);mem_write32(&cpu,overlay+0x14,outer);frame();
    mem_write32(&cpu,SCENE+0x84,background);mem_write32(&cpu,background+0x14,canvas);frame();
    wii_menu_grid_transform(&cpu,inner);wii_menu_grid_transform(&cpu,outer);
    wii_menu_grid_transform(&cpu,canvas);
    CHECK(real(&cpu,inner+0x44)==real(&cpu,outer+0x44) && real(&cpu,inner+0x48)==real(&cpu,outer+0x48));
    CHECK(real(&cpu,inner+0x44)==real(&cpu,canvas+0x44) && real(&cpu,inner+0x48)==real(&cpu,canvas+0x48));
    /* Sliding clock pictures retain their source mesh but stay inside the
     * footer's center, intersecting rather than expanding the caller clip. */
    unsigned clock=0x80077000,number=0x80078000;footer_add(&cpu,clock,3);
    mem_write32(&cpu,number+0xc,clock);CHECK(wii_menu_grid_picture_tiles(&cpu,number)==1);
    WiiMenuPicture picture={0};picture.draw.width=640;picture.draw.height=456;
    picture.draw.clip_width=640;picture.draw.clip_height=456;
    wii_menu_grid_picture_tile(&cpu,number,0,&picture);
    CHECK(picture.draw.clip_x>100 && picture.draw.clip_width<400 && picture.draw.clip_height==456);
    wii_menu_grid_configure(false);CHECK(real(&cpu,pane+0x44)==1.f && real(&cpu,pane+0x48)==1.f);
    cpu.gpr[3]=SCENE;cpu.gpr[4]=0;cpu.gpr[5]=0;CHECK(!wii_menu_grid_entry(&cpu,0x813ad55c));
    free(cpu.ram);puts("Menu grid mapping, paging, native continuations and stock restoration passed");return 0;
}
