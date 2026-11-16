/* SPDX-License-Identifier: MIT */
/* Original 4.3U scene objects, NW4R panes and GUI components remain the owners
 * of channels, input and title transitions. This adapter changes presentation;
 * it never writes the channel manager's persistent page/index arrays.
 * Offsets and seams: annotations/wii-menu-43u.json, original SHA1 26116613... . */
#include <wiillr/menu_grid.h>
#include <wiillr/menu_picture.h>
#include <cpu/interpreter.h>
#include <memory/memory.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum {COMMON=0x813aaa38,REFRESH=0x813ac948,RESTART_MODULES=0x813aeaf8,
      MAKE_LIST=0x813ac9d0,GET_SCENE=0x8140b138,INIT_ARROW=0x8139ce28,ENCODED=0x10000};
typedef struct {uint32_t pane,component;float x,y,sx,sy;uint8_t visible,trigger;int flat;} Slot;
typedef struct {uint32_t pane;float sx,sy;} Root;
typedef struct {uint32_t pane;float x,y,sx,sy,gy;unsigned kind;} Footer;
static bool enabled,fit=true;
static bool return_anchor_valid;
static unsigned return_anchor;
static unsigned width=1280,height=720;
static struct {
    CPUState* cpu;uint32_t scene,layout;Slot slots[3][12];Root roots[192];unsigned root_count;
    unsigned columns,page,pages,first,count,carrier,anchor;float world_width,sx,sy,gap_x,step_x,step_y;
    bool ready,refresh,arrows,refresh_modules,loaded;int selected;
    uint32_t hidden,clock_anchor;unsigned shown[36],shown_count;float bank_x[3];
    struct {bool active;unsigned target,carrier;int direction;float progress;} scroll;
    Footer footer[8];unsigned footer_count;
    uint32_t background_pane;bool background_ready;WiiMenuPicture background;
    struct {bool active;unsigned stage;uint32_t lr,sp,object,button;} call;
} grid;

static const uint8_t* ptr(CPUState* c,uint32_t a,unsigned n) {
    uint32_t available=0;const uint8_t* p=gcn_mem_resolve(c,a,&available);
    return p && n<=available?p:NULL;
}
static uint32_t word(CPUState* c,uint32_t a) {
    const uint8_t* p=ptr(c,a,4);return p?(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]:0;
}
static float real(CPUState* c,uint32_t a) {uint32_t w=word(c,a);float f;memcpy(&f,&w,4);return f;}
static void put(CPUState* c,uint32_t a,float f) {uint32_t w;memcpy(&w,&f,4);mem_write32(c,a,w);}
static void visibility(CPUState* c,uint32_t pane,bool show) {
    const uint8_t* p=ptr(c,pane+0xcf,1);if(p)mem_write8(c,pane+0xcf,(*p&~1u)|(show?1:0));
}
static int name_slot(const char* n,int* bank) {
    if(strncmp(n,"N_Ch_",5) || n[5]<'b' || n[5]>'d' || n[6]<'0' || n[6]>'1' ||
       n[7]<'0' || n[7]>'9' || n[8])return -1;
    int index=(n[6]-'0')*10+n[7]-'0'-1;if(index<0 || index>=12)return -1;
    *bank=n[5]-'b';return index;
}
static void tile_position(unsigned flat,float* x,float* y);
static bool in_extent(unsigned flat,float margin) {
    if(flat>=48 || !grid.columns)return false;
    float x,y;tile_position(flat,&x,&y);(void)y;
    return x+88.f*grid.sx>-grid.world_width*.5f-margin && x-88.f*grid.sx<grid.world_width*.5f+margin;
}
static bool visible(unsigned flat) {return in_extent(flat,0);}
static int flat_for(int bank,int index) {
    return grid.slots[bank][index].flat;
}
static bool active(CPUState* c) {
    return enabled && fit && grid.ready && grid.cpu==c && word(c,grid.scene)==0x8164e0b8 &&
        word(c,grid.scene+0x68)==grid.layout;
}
static bool owned(CPUState* c) {
    return grid.ready && grid.cpu==c && word(c,grid.scene)==0x8164e0b8 && word(c,grid.scene+0x68)==grid.layout;
}
static int result(CPUState* c,uint32_t value) {c->gpr[3]=value;c->pc=c->lr;++c->cycles;return 1;}
static void restore(CPUState* c) {
    if(!grid.ready || grid.cpu!=c || word(c,grid.scene)!=0x8164e0b8 || word(c,grid.scene+0x68)!=grid.layout)return;
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
        Slot* s=&grid.slots[b][i];put(c,s->pane+0x2c,s->x);put(c,s->pane+0x30,s->y);
        put(c,s->pane+0x44,s->sx);put(c,s->pane+0x48,s->sy);visibility(c,s->pane,s->visible);
        mem_write8(c,s->component+0x7c,s->trigger);
    }
    for(unsigned i=0;i<grid.root_count;i++) {
        Root* r=&grid.roots[i];put(c,r->pane+0x44,r->sx);put(c,r->pane+0x48,r->sy);
    }
    uint32_t object=word(c,grid.scene+0x58),current=word(c,grid.scene+0xc8),link=word(c,grid.scene+0x60)&0xffff;
    for(unsigned n=0;object && n<49;n++) {
        int bank=(int)word(c,object+0x1c)-(int)current+1;unsigned index=word(c,object+0x20);
        mem_write32(c,object+0x28,bank>=0 && bank<3 && index<12?grid.slots[bank][index].pane:grid.hidden);
        object=word(c,object+link+4);
    }
}
void wii_menu_grid_reset(void) {memset(&grid,0,sizeof grid);grid.selected=-1;}
bool wii_menu_grid_supported(void) {
#if WIILLR_MENU_TRANSFORM && WIILLR_MENU_FRAME_MODE
    return true;
#else
    return false;
#endif
}
void wii_menu_grid_configure(bool value) {
    value=value && wii_menu_grid_supported();
    if(enabled && !value && grid.cpu)restore(grid.cpu);
    enabled=value;
}
bool wii_menu_grid_enabled(void) {return enabled;}
void wii_menu_grid_extent(unsigned w,unsigned h,bool use_fit) {
    if(w>=160 && h>=160 && w<=16384 && h<=16384){width=w;height=h;}
    if(fit && !use_fit && grid.cpu)restore(grid.cpu);
    fit=use_fit;
}
bool wii_menu_grid_aspect(unsigned* w,unsigned* h) {
    if(!enabled || !fit || !grid.ready || !grid.cpu)return false;
    unsigned phase=word(grid.cpu,grid.scene+0xc0);
    if(phase>3 && (phase<10 || phase>13))return false;
    *w=width;*h=height;return true;
}
static bool bind_scene(CPUState* c,uint32_t scene) {
    if(word(c,scene)!=0x8164e0b8 || word(c,scene+0xcc)!=4)return false;
    const uint32_t layout=word(c,scene+0x68),gui=word(c,scene+0x7c);
    if(!layout || !gui)return false;
    const unsigned anchor=return_anchor_valid?return_anchor:word(c,scene+0xc8)*12;
    wii_menu_grid_reset();grid.cpu=c;grid.scene=scene;grid.layout=layout;grid.anchor=anchor;
    uint32_t node=word(c,gui+0x18);unsigned found=0;
    for(unsigned visited=0;node && visited<128;visited++) {
        const uint32_t pane=word(c,node),component=word(c,node+4);const char* name=(const char*)ptr(c,pane+0xb4,17);
        if(name && !strcmp(name,"Picture_16"))grid.hidden=pane;
        if(name && !strcmp(name,"N_Clock1"))grid.clock_anchor=pane;
        int bank,index;if(name && memchr(name,0,17) && (index=name_slot(name,&bank))>=0) {
            if(word(c,component+0x94)!=pane || grid.slots[bank][index].pane)return false;
            Slot* s=&grid.slots[bank][index];s->pane=pane;s->component=component;s->flat=-1;
            grid.bank_x[bank]=real(c,word(c,pane+0xc)+0x90);
            s->x=real(c,pane+0x2c);s->y=real(c,pane+0x30);s->sx=real(c,pane+0x44);s->sy=real(c,pane+0x48);
            s->visible=word(c,pane+0xcc)&1;s->trigger=word(c,component+0x7c)>>24;++found;
        }
        node=word(c,node+12);
    }
    grid.ready=found==36;grid.refresh=grid.arrows=grid.ready;
    return grid.ready;
}
static void geometry(CPUState* c) {
    unsigned columns=(unsigned)floorf((float)width/height*2.8f+0.1f);
    if(columns<2)columns=2;
    if(columns>8)columns=8;
    if(grid.scroll.active)columns=grid.columns;
    if(columns!=grid.columns) {
        /* Keep the first displayed channel nearby when the capacity changes. */
        grid.page=grid.anchor/(columns*3);grid.columns=columns;grid.arrows=true;
    }
    grid.pages=(48+columns*3-1)/(columns*3);if(grid.page>=grid.pages)grid.page=grid.pages-1;
    grid.first=grid.page*columns*3;grid.count=48-grid.first;if(grid.count>columns*3)grid.count=columns*3;
    const unsigned carrier=(grid.first+(grid.count-1)/2)/12;
    if(!grid.scroll.active) {
        if(carrier!=word(c,grid.scene+0xc8))grid.refresh=true;
        grid.carrier=carrier;
    }
    const float aspect=(float)width/height;
    grid.world_width=real(c,grid.scene+0xec)>1.1f?832.f:608.f;
    const float gap=11.f,card_aspect=1.725f;
    const float card_h=fminf(96.f,((aspect*456.f*.88f-gap*(columns-1))/columns)/card_aspect);
    grid.sx=(card_h*card_aspect*grid.world_width/(aspect*456.f))/170.f;
    grid.sy=card_h/96.f;grid.gap_x=gap*grid.world_width/(aspect*456.f);
    grid.step_x=170.f*grid.sx+grid.gap_x;grid.step_y=card_h+gap;
}
static void tile_position(unsigned flat,float* x,float* y) {
    unsigned capacity=grid.columns*3,local=flat%capacity,page=flat/capacity;
    float pages=(int)page-(int)grid.page;
    if(grid.scroll.active)pages-=grid.scroll.direction*grid.scroll.progress;
    *x=(((int)(local%grid.columns)-(grid.columns-1)*.5f)+pages*grid.columns)*grid.step_x;
    *y=148.f-(local/grid.columns)*grid.step_y;
}
static void map_slots(CPUState* c) {
    grid.shown_count=0;
    for(unsigned flat=0;flat<48;flat++)if(in_extent(flat,grid.step_x*.75f) && grid.shown_count<36)
        grid.shown[grid.shown_count++]=flat;
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
        Slot* s=&grid.slots[b][i];bool keep=false;
        for(unsigned n=0;n<grid.shown_count;n++)if(s->flat==(int)grid.shown[n])keep=true;
        if(!keep)s->flat=-1;
    }
    for(unsigned n=0;n<grid.shown_count;n++) {
        bool found=false;Slot* free_slot=NULL;
        for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
            Slot* s=&grid.slots[b][i];if(s->flat==(int)grid.shown[n])found=true;
            if(s->flat<0 && !free_slot)free_slot=s;
        }
        if(!found && free_slot)free_slot->flat=grid.shown[n];
    }
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
        Slot* s=&grid.slots[b][i];bool show=s->flat>=0;
        visibility(c,s->pane,show);
        mem_write8(c,s->component+0x7c,show && (unsigned)s->flat/(grid.columns*3)==grid.page && !grid.scroll.active);
    }
}
static uint32_t pane_for(unsigned flat) {
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++)
        if(grid.slots[b][i].flat==(int)flat)return grid.slots[b][i].pane;
    return grid.hidden;
}
static uint32_t find_pane(CPUState* c,uint32_t pane,const char* name,unsigned depth) {
    if(!pane || depth>16)return 0;
    const char* n=(const char*)ptr(c,pane+0xb4,17);
    if(n && memchr(n,0,17) && !strcmp(n,name))return pane;
    uint32_t link=word(c,pane+0x14);
    for(unsigned visited=0;link && link!=pane+0x14 && visited<512;visited++) {
        uint32_t found=find_pane(c,link-4,name,depth+1);if(found)return found;
        link=word(c,link);
    }
    return 0;
}
static void footer_add(CPUState* c,uint32_t pane,unsigned kind) {
    if(!pane || !ptr(c,pane,0xd4) || grid.footer_count>=8)return;
    for(unsigned n=0;n<grid.footer_count;n++)if(grid.footer[n].pane==pane)return;
    grid.footer[grid.footer_count++]=(Footer){pane,real(c,pane+0x2c),real(c,pane+0x30),
        real(c,pane+0x44),real(c,pane+0x48),real(c,pane+0xa0),kind};
}
static void bind_footer(CPUState* c,uint32_t button) {
    uint32_t layout=word(c,button+0x58),root=word(c,layout+0x14);
    footer_add(c,find_pane(c,root,"N_BtnL",0),1);
    footer_add(c,find_pane(c,root,"N_BtnR",0),2);
    footer_add(c,find_pane(c,root,"N_ArwL",0),5);
    footer_add(c,find_pane(c,root,"N_ArwR",0),5);
    footer_add(c,word(c,word(c,button+0x70)+0x14),4); /* embedded SD menu layout */
    /* The opt-out layout is used by other scenes. Its hidden controls sit
     * outside the viewport; scaling its root can pull them into the grid. */
    uint32_t clock_root=word(c,word(c,grid.scene+0x134)+0x14);
    footer_add(c,find_pane(c,clock_root,"N_WiiMenu",0),3);
}
static void footer_factors(float* sx,float* sy) {
    float aspect=(float)width/height;*sy=fminf(1.f,aspect/1.4f);*sx=*sy*(16.f/9.f)/aspect;
}
static bool footer_transform(CPUState* c,uint32_t pane) {
    for(unsigned n=0;n<grid.footer_count;n++) {
        Footer* f=&grid.footer[n];if(f->pane!=pane)continue;
        unsigned phase=word(c,grid.scene+0xc0);
        if(!enabled || !fit || (phase>3 && (phase<10 || phase>13))) {
            put(c,pane+0x44,f->sx);put(c,pane+0x48,f->sy);
            if(f->kind!=3){put(c,pane+0x2c,f->x);put(c,pane+0x30,f->y);}return true;
        }
        float sx,sy;footer_factors(&sx,&sy);
        uint32_t parent=word(c,pane+0xc);
        if(f->kind<=2) {
            float px=real(c,parent+0x84),py=real(c,parent+0x98),tx=real(c,parent+0x90),ty=real(c,parent+0xa0);
            if(fabsf(px)<.01f || fabsf(py)<.01f)return true;
            float edge=f->kind==1?-grid.world_width*.5f:grid.world_width*.5f;
            float x=edge+(tx+f->x*px-edge)*sx;
            float y=-228.f+(ty+f->y*py+228.f)*sy;
            put(c,pane+0x2c,(x-tx)/px);put(c,pane+0x30,(y-ty)/py);
            put(c,pane+0x44,f->sx*sx);put(c,pane+0x48,f->sy*sy);
        } else if(f->kind==5) {
            float py=real(c,parent+0x98);if(fabsf(py)>.01f)put(c,pane+0x30,(f->gy-real(c,parent+0xa0))/py);
        } else if(f->kind==6) {
            float px=real(c,parent+0x84),py=real(c,parent+0x98),tx=real(c,parent+0x90),ty=real(c,parent+0xa0);
            if(fabsf(px)>.01f)put(c,pane+0x2c,((tx+f->x*px)*sx-tx)/px);
            if(fabsf(py)>.01f)put(c,pane+0x30,(-228.f+(f->gy+228.f)*sy-ty)/py);
            put(c,pane+0x44,f->sx*sx);put(c,pane+0x48,f->sy*sy);
        } else {
            put(c,pane+0x44,f->sx*sx);put(c,pane+0x48,f->sy*sy);
            if(f->kind==4) {
                put(c,pane+0x2c,-grid.world_width*.5f+(f->x+grid.world_width*.5f)*sx);
                put(c,pane+0x30,-228.f+(f->y+228.f)*sy);
            } else if(grid.clock_anchor) {
                float py=real(c,parent+0x98),ty=real(c,parent+0xa0);
                if(fabsf(py)>.01f)put(c,pane+0x30,(-228.f+(real(c,grid.clock_anchor+0xa0)+228.f)*sy-ty)/py);
            }
        }
        return true;
    }
    return false;
}
/* The original helper calls run on the original guest stack and scheduler.
 * Reenter this seam after each return, then execute the unmodified common body.
 * No synthetic interpreter, nested frame loop or host blocking wait is needed. */
static int guest_call(CPUState* c,uint32_t pc,uint32_t a,uint32_t b,uint32_t d,unsigned stage) {
    grid.call.stage=stage;c->gpr[3]=a;c->gpr[4]=b;c->gpr[5]=d;c->lr=COMMON;c->pc=pc;++c->cycles;return 1;
}
static int common(CPUState* c) {
    if(grid.call.active) {
        if(c->gpr[1]!=grid.call.sp)return 0; /* Other scheduled thread. */
        const unsigned stage=grid.call.stage;
        if(stage>=10 && stage<13)return guest_call(c,MAKE_LIST,grid.scene,stage-9,0,stage+1);
        if(stage==13){grid.loaded=true;return guest_call(c,REFRESH,grid.scene,grid.carrier,0,1);}
        if(stage==1 && grid.refresh_modules)return guest_call(c,RESTART_MODULES,grid.scene,0,0,2);
        if(stage<=2)return guest_call(c,GET_SCENE,word(c,0x81089008+0x64),5,0,3);
        if(stage==3) {
            grid.call.button=c->gpr[3];
            if(grid.call.button)bind_footer(c,grid.call.button);
            if(grid.call.button)return guest_call(c,INIT_ARROW,grid.call.button,1,grid.page>0,4);
        }
        if(stage==4)return guest_call(c,INIT_ARROW,grid.call.button,0,grid.page+1<grid.pages,5);
        if(stage==5)return guest_call(c,GET_SCENE,word(c,0x81089008+0x64),4,0,6);
        if(stage==6) {
            uint32_t root=word(c,word(c,c->gpr[3]+0xd24)+0x14);
            footer_add(c,find_pane(c,root,"T_Day_b",0),6);
            uint32_t background=find_pane(c,root,"TopBack_b",0);
            if(background!=grid.background_pane)grid.background_ready=false;
            grid.background_pane=background;
        }
        c->lr=grid.call.lr;c->gpr[3]=grid.call.object;grid.call.active=false;
        mem_write8(c,grid.scene+0x100,grid.page>0);mem_write8(c,grid.scene+0x101,grid.page+1<grid.pages);
        grid.refresh_modules=false;grid.refresh=grid.arrows=false;
        if(!enabled || !fit){restore(c);return 0;}
    }
    uint32_t scene=c->gpr[3];
    if(word(c,scene)!=0x8164e0b8)return 0;
    if(!grid.ready || grid.cpu!=c || grid.scene!=scene || grid.layout!=word(c,scene+0x68)) {
        if(!bind_scene(c,scene))return 0;
    }
    geometry(c);
    map_slots(c);
    const unsigned phase=word(c,scene+0xc0);
    if((phase==1 || phase==3) && (grid.refresh || grid.arrows)) {
        grid.call.active=true;grid.call.lr=c->lr;grid.call.sp=c->gpr[1];grid.call.object=scene;
        if(!grid.loaded)return guest_call(c,MAKE_LIST,scene,0,0,10);
        if(grid.refresh) {
            mem_write32(c,scene+0xc8,grid.carrier);
            return guest_call(c,REFRESH,scene,grid.carrier,0,1);
        }
        return guest_call(c,GET_SCENE,word(c,0x81089008+0x64),5,0,3);
    }
    /* Cache the roots owned by the current channel list. The C ut::List has
     * object pointers and {prev,next} links, unlike Pane's C++ sentinel list. */
    grid.root_count=0;
    uint32_t object=word(c,scene+0x58);unsigned link=word(c,scene+0x60)&0xffff;
    for(unsigned n=0;object && n<49;n++) {
        if(word(c,object+0x1c)<4 && word(c,object+0x20)<12) {
            mem_write32(c,object+0x28,pane_for(word(c,object+0x1c)*12+word(c,object+0x20)));
            const unsigned offsets[]={0x30,0x38,0x40,0x48,0x60};
            for(unsigned j=0;j<5;j++) {
                uint32_t layout=word(c,object+offsets[j]),pane=layout?word(c,layout+0x14):0;
                if(!pane || !ptr(c,pane,0xd4) || grid.root_count>=192)continue;
                bool seen=false;for(unsigned k=0;k<grid.root_count;k++)if(grid.roots[k].pane==pane)seen=true;
                if(!seen)grid.roots[grid.root_count++]=(Root){pane,real(c,scene+0xec),1.f};
            }
        }
        object=word(c,object+link+4);
    }
    const unsigned disc_layers[]={0x84,0x9c};
    for(unsigned n=0;n<2;n++) {
        uint32_t disc=word(c,grid.scene+disc_layers[n]),disc_root=disc?word(c,disc+0x14):0;
        if(disc_root && grid.root_count<192)grid.roots[grid.root_count++]=(Root){disc_root,real(c,scene+0xec),1.f};
    }
    return 0;
}
void wii_menu_grid_transform(CPUState* c,uint32_t pane) {
    if(owned(c) && footer_transform(c,pane))return;
    if(!active(c))return;
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
        Slot* s=&grid.slots[b][i];if(pane!=s->pane)continue;
        const int flat=flat_for(b,i);if(flat<0 || !visible(flat))return;
        const uint32_t parent=word(c,pane+0xc);float x,y;tile_position(flat,&x,&y);
        if(grid.scroll.active) {
            float shift=real(c,parent+0x90)-grid.bank_x[b],span=512.f*real(c,grid.scene+0xec);
            if(span>0)grid.scroll.progress=fminf(1.f,fmaxf(0.f,-grid.scroll.direction*shift/span));
            tile_position(flat,&x,&y);
        }
        const float px=real(c,parent+0x84),py=real(c,parent+0x98);
        if(!isfinite(px) || !isfinite(py) || fabsf(px)<.01f || fabsf(py)<.01f)return;
        put(c,pane+0x2c,(x-real(c,parent+0x90))/px);put(c,pane+0x30,(y-real(c,parent+0xa0))/py);
        put(c,pane+0x44,grid.sx*real(c,grid.scene+0xec)/px);put(c,pane+0x48,grid.sy/py);return;
    }
    for(unsigned i=0;i<grid.root_count;i++)if(grid.roots[i].pane==pane) {
        put(c,pane+0x44,grid.sx*grid.roots[i].sx);put(c,pane+0x48,grid.sy*grid.roots[i].sy);return;
    }
}
bool wii_menu_grid_clip(CPUState* c,uint32_t scene,float* x,float* y) {
    if(!active(c) || grid.scene!=scene)return false;
    *x*=grid.sx;*y*=grid.sy;return true;
}
int wii_menu_grid_entry(CPUState* c,uint32_t pc) {
    if(pc==COMMON && grid.call.active && c->gpr[1]==grid.call.sp)return common(c);
    if(!enabled || !fit)return 0;
    if(c->cycles>=c->cycle_deadline)return 1;
    if(pc==COMMON)return common(c);
    if(!active(c))return 0;
    if(pc==0x813b01b8 && word(c,c->gpr[3]+0xc)==grid.scene && !c->gpr[5])return result(c,0); /* No drag in experiment. */
    if(c->gpr[3]==grid.scene) {
        if(pc==REFRESH && grid.scroll.active && (word(c,grid.scene+0xc0)==12 || word(c,grid.scene+0xc0)==13)) {
            grid.page=grid.scroll.target;grid.anchor=grid.page*grid.columns*3;grid.carrier=grid.scroll.carrier;
            grid.scroll.active=false;grid.arrows=true;geometry(c);map_slots(c);grid.refresh=false;
        }
        if(pc==0x813ad55c)return result(c,c->gpr[4]<4 && c->gpr[5]<12 && visible(c->gpr[4]*12+c->gpr[5]));
        if(pc==0x813ad69c)return result(c,c->gpr[4]<4 && c->gpr[5]<12 && pane_for(c->gpr[4]*12+c->gpr[5])!=grid.hidden);
        if(pc==0x813ade6c && c->gpr[4]<4 && c->gpr[5]<12)return result(c,pane_for(c->gpr[4]*12+c->gpr[5]));
        if(pc==0x813ad39c)return result(c,0); /* No neighboring page decorations. */
        if(pc==0x813ace78 && (c->gpr[5]&0xffff0000u)==ENCODED) {
            unsigned flat=c->gpr[5]&0xffff;c->gpr[4]=flat/12;c->gpr[5]=flat%12;return 0;
        }
        if(pc==0x813ae99c) {
            const char* n=(const char*)ptr(c,c->gpr[4],17);int b,i;
            if(n && memchr(n,0,17) && (i=name_slot(n,&b))>=0) {
                int flat=flat_for(b,i);return result(c,flat>=0 && visible(flat) && (unsigned)flat/(grid.columns*3)==grid.page && !grid.scroll.active?ENCODED|(unsigned)flat:UINT32_MAX);
            }
            return result(c,UINT32_MAX);
        }
        if(pc==0x813ada94) {
            grid.selected=(int)(c->gpr[4]*12+c->gpr[5]);
            return_anchor=grid.selected;return_anchor_valid=true;
        }
        if((pc==0x813adf18 || pc==0x813adf48) && grid.selected>=0 && word(c,grid.scene+0xc0)>=7 &&
           c->gpr[4]==(unsigned)grid.selected%12) {
            return result(c,pane_for(grid.selected));
        }
        if(pc==0x813ad8e0) {
            int direction=c->gpr[4]==12?-1:1;
            int target=(int)grid.page+direction;
            if(target<0 || target>=(int)grid.pages)return result(c,0);
            grid.scroll.active=true;grid.scroll.target=target;grid.scroll.direction=direction;grid.scroll.progress=0;
            unsigned first=target*grid.columns*3,count=48-first;if(count>grid.columns*3)count=grid.columns*3;
            grid.scroll.carrier=(first+(count-1)/2)/12;
            return 0; /* Original 20-frame brlan scroll owns timing/easing. */
        }
    }
    if(grid.scroll.active && c->gpr[31]==grid.scene) {
        if(pc==0x813ac630 || pc==0x813ac674) {
            c->gpr[pc==0x813ac630?0:5]=grid.scroll.carrier-grid.scroll.direction;
            c->pc=pc+4;++c->cycles;return 1;
        }
        if(pc==0x813ac63c || pc==0x813ac688) {
            unsigned left=grid.scroll.target,right=pc==0x813ac63c?0:grid.pages-1;
            unsigned condition=left<right?8:left>right?4:2;
            c->cr=(c->cr&0x0fffffffu)|((condition|((c->xer>>31)&1))<<28);
            if(pc==0x813ac688){c->pc=pc+4;++c->cycles;return 1;}
            return 0;
        }
    }
    /* Only page-bound comparisons see virtual pages. Original arrays, title
     * selection and loading continue to receive their genuine stock indices. */
    unsigned base=0,dest=0;bool max=false;
    switch(pc) {
    case 0x813ac4a4:base=30;dest=0;break;
    case 0x813ac4e4:base=30;dest=3;max=true;break;
    case 0x813ac4e8:base=30;dest=4;break;
    case 0x813b069c:base=3;dest=0;break;
    case 0x813b06e4:base=4;dest=3;max=true;break;
    case 0x813b06e8:base=4;dest=4;break;
    default:return 0;
    }
    if(c->gpr[base]!=grid.scene)return 0;
    c->gpr[dest]=max?grid.pages:grid.page;c->pc=pc+4;++c->cycles;return 1;
}
int wii_menu_grid_status(char* out,size_t capacity) {
    return snprintf(out,capacity,"{\"ok\":true,\"enabled\":%s,\"ready\":%s,\"columns\":%u,\"rows\":3,\"page\":%u,\"pages\":%u,\"first\":%u,\"count\":%u,\"scene\":%u,\"width\":%u,\"height\":%u}",
        enabled?"true":"false",grid.ready?"true":"false",grid.columns,grid.page,grid.pages,grid.first,grid.count,grid.scene,width,height);
}
static int footer_sheet(CPUState* c,uint32_t pane) {
    const char* n=(const char*)ptr(c,pane+0xb4,17);if(!n || !memchr(n,0,17))return 0;
    if((!strncmp(n,"board",5) && strlen(n)==6) || (!strncmp(n,"Shade",5) && strlen(n)==6) ||
       (!strncmp(n,"Line",4) && strlen(n)==5))return n[strlen(n)-1]=='2'?1:-1;
    return 0;
}
static bool clock_picture(CPUState* c,uint32_t pane) {
    for(unsigned depth=0;pane && depth<16;depth++,pane=word(c,pane+0xc))
        for(unsigned n=0;n<grid.footer_count;n++)
            if(grid.footer[n].kind==3 && grid.footer[n].pane==pane)return true;
    return false;
}
static uint32_t cursor_picture(CPUState* c,uint32_t pane) {
    unsigned w,h;if(!wii_menu_grid_aspect(&w,&h))return 0;
    for(unsigned depth=0;pane && depth<12;depth++,pane=word(c,pane+0xc)) {
        const char* name=(const char*)ptr(c,pane+0xb4,17);
        if(name && memchr(name,0,17) && !strcmp(name,"N_Trans") &&
           find_pane(c,pane,"N_Rot",0) && find_pane(c,pane,"N_SRot",0))return pane;
    }
    return 0;
}
int wii_menu_grid_picture_tiles(CPUState* c,uint32_t pane) {
    if(!active(c))return 0;
    if(pane==grid.background_pane || cursor_picture(c,pane))return 1;
    if(clock_picture(c,pane))return 1;
    uint32_t root=word(c,grid.layout+0x14),ancestor=pane;
    for(unsigned n=0;n<8 && ancestor && ancestor!=root;n++)ancestor=word(c,ancestor+0xc);
    if(ancestor!=root)return 0;
    const char* name=(const char*)ptr(c,pane+0xb4,17);
    if(!name || !memchr(name,0,17))return 0;
    int footer=footer_sheet(c,pane);if(footer)return footer>0?3:-1;
    if(!strncmp(name,"BaseMask",8))return !strcmp(name,"BaseMask2") && !grid.background_ready?(int)grid.shown_count:-1;
    if(!strncmp(name,"Edge",4))return !strcmp(name,"Edge2")?(int)grid.shown_count:-1;
    if(!strncmp(name,"Picture_0",9))return !strcmp(name,"Picture_00")?(int)grid.shown_count:-1;
    return 0;
}
static float interpolate(float a,float b,float d,float e,float u,float v) {
    return (a+(b-a)*u)*(1-v)+(d+(e-d)*u)*v;
}
static void sample_quad(const WiiMenuPicture* original,WiiMenuPicture* out,unsigned v,float u,float t) {
    for(unsigned a=0;a<2;a++) {
        out->vertices[v].uv[a]=interpolate(original->vertices[0].uv[a],original->vertices[1].uv[a],original->vertices[3].uv[a],original->vertices[2].uv[a],u,t);
        out->extra_uv[v][a]=interpolate(original->extra_uv[0][a],original->extra_uv[1][a],original->extra_uv[3][a],original->extra_uv[2][a],u,t);
        for(unsigned j=0;j<7;j++)out->program_extra_uv[v][j][a]=interpolate(original->program_extra_uv[0][j][a],original->program_extra_uv[1][j][a],original->program_extra_uv[3][j][a],original->program_extra_uv[2][j][a],u,t);
    }
    for(unsigned a=0;a<4;a++)out->vertices[v].color[a]=interpolate(original->vertices[0].color[a],original->vertices[1].color[a],original->vertices[3].color[a],original->vertices[2].color[a],u,t);
}
static bool screen_point(const WiiMenuPicture* picture,unsigned v,float* x,float* y) {
    const float* m=picture->draw.model_view;const float* p=picture->draw.projection;
    const float* pos=picture->vertices[v].position;float world[4]={0,0,0,1};
    for(unsigned a=0;a<3;a++)world[a]=m[a*4]*pos[0]+m[a*4+1]*pos[1]+m[a*4+2]*pos[2]+m[a*4+3];
    float clip[4]={0};for(unsigned a=0;a<4;a++)for(unsigned b=0;b<4;b++)clip[a]+=p[a*4+b]*world[b];
    if(!isfinite(clip[3]) || fabsf(clip[3])<.000001f)return false;
    *x=picture->draw.x+picture->draw.width*(clip[0]/clip[3]+1.f)*.5f;
    *y=picture->draw.y+picture->draw.height*(1.f-clip[1]/clip[3])*.5f;
    return isfinite(*x) && isfinite(*y);
}
static bool background_basis(const WiiMenuPicture* bg,float x[3],float y[3],float* determinant) {
    const unsigned vertices[3]={0,1,3};
    for(unsigned n=0;n<3;n++)if(!screen_point(bg,vertices[n],&x[n],&y[n]))return false;
    *determinant=(x[1]-x[0])*(y[2]-y[0])-(x[2]-x[0])*(y[1]-y[0]);
    return isfinite(*determinant) && fabsf(*determinant)>.000001f;
}
static void background_matte(WiiMenuPicture* out) {
    if(!grid.background_ready || !out->needs_mask || out->needs_program || out->effect)return;
    const WiiMenuPicture* bg=&grid.background;float x[3],y[3],det;
    if(!background_basis(bg,x,y,&det))return;
    float uv[4][2],mask[4][2],colors[4][4];
    for(unsigned v=0;v<4;v++) {
        float px,py;if(!screen_point(out,v,&px,&py))return;
        float u=((px-x[0])*(y[2]-y[0])-(py-y[0])*(x[2]-x[0]))/det;
        float t=((x[1]-x[0])*(py-y[0])-(y[1]-y[0])*(px-x[0]))/det;
        for(unsigned a=0;a<2;a++) {
            uv[v][a]=interpolate(bg->vertices[0].uv[a],bg->vertices[1].uv[a],bg->vertices[3].uv[a],bg->vertices[2].uv[a],u,t);
            mask[v][a]=out->mask.uv[a][0]*out->vertices[v].uv[0]+out->mask.uv[a][1]*out->vertices[v].uv[1]+out->mask.uv[a][2];
        }
        for(unsigned a=0;a<4;a++)colors[v][a]=out->vertices[v].color[a]*interpolate(bg->vertices[0].color[a],bg->vertices[1].color[a],bg->vertices[3].color[a],bg->vertices[2].color[a],u,t);
    }
    /* The stock sheet paints the backdrop around its rounded holes. Reflow
     * needs that backdrop at the destination pixel, rather than a cropped
     * copy of the old page gradient. Reuse the original background texture
     * and rounded mask with independent coordinates through the product API. */
    out->draw.material.texture=bg->draw.material.texture;
    out->color=bg->color;out->mask.color=out->color;
    out->effect=3;out->paired_uv=1;out->needs_mask=0;
    memset(out->primary_uv,0,sizeof out->primary_uv);memset(out->mask.uv,0,sizeof out->mask.uv);
    out->primary_uv[0][0]=out->primary_uv[1][1]=out->mask.uv[0][0]=out->mask.uv[1][1]=1;
    for(unsigned v=0;v<4;v++) {
        memcpy(out->vertices[v].uv,uv[v],sizeof uv[v]);memcpy(out->extra_uv[v],mask[v],sizeof mask[v]);
        memcpy(out->vertices[v].color,colors[v],sizeof colors[v]);
    }
}
void wii_menu_grid_picture_tile(CPUState* c,uint32_t pane,unsigned tile,WiiMenuPicture* out) {
    if(!active(c))return;
    const WiiMenuPicture original=*out;
    if(pane==grid.background_pane) {
        float x[3],y[3],det;
        grid.background_ready=!out->skipped && !out->needs_program && !out->needs_mask && !out->needs_layers && !out->effect &&
            out->draw.material.texture.size && background_basis(out,x,y,&det);
        if(grid.background_ready)grid.background=*out;
        return;
    }
    uint32_t cursor=cursor_picture(c,pane);
    if(cursor) {
        float sx,sy;footer_factors(&sx,&sy);
        const float x=real(c,cursor+0x90),y=real(c,cursor+0xa0);
        for(unsigned a=0;a<3;a++){out->draw.model_view[a]*=sx;out->draw.model_view[4+a]*=sy;}
        out->draw.model_view[3]=x+(out->draw.model_view[3]-x)*sx;
        out->draw.model_view[7]=y+(out->draw.model_view[7]-y)*sy;
        return;
    }
    if(clock_picture(c,pane)) {
        /* The original three clocks move with their page anchors. Keep that
         * motion within the center of the proportional footer, behind its
         * side caps, instead of letting the numerals cross the buttons. */
        float sx,sy;footer_factors(&sx,&sy);(void)sy;
        float edge=fminf(.4f,.25f*sx);
        int left=(int)ceilf(out->draw.x+out->draw.width*edge);
        int right=(int)floorf(out->draw.x+out->draw.width*(1.f-edge));
        int end=out->draw.clip_x+out->draw.clip_width;
        if(left<out->draw.clip_x)left=out->draw.clip_x;
        if(right>end)right=end;
        out->draw.clip_x=left;out->draw.clip_width=right>left?right-left:0;
        if(!out->draw.clip_width)out->skipped=1;
        return;
    }
    if(footer_sheet(c,pane)>0) {
        if(tile>=3)return;
        float sx,sy;footer_factors(&sx,&sy);
        const float px=original.draw.model_view[0],py=original.draw.model_view[5];
        if(fabsf(px)<.01f || fabsf(py)<.01f){out->skipped=1;return;}
        float end=fminf(grid.world_width*.4f,fabsf(px*(original.vertices[1].position[0]-original.vertices[0].position[0]))*.25f*sx);
        float edges[4]={-grid.world_width*.5f,-grid.world_width*.5f+end,grid.world_width*.5f-end,grid.world_width*.5f};
        const float uv[4]={0,.25f,.75f,1};const unsigned right[4]={0,1,1,0},bottom[4]={0,0,1,1};
        for(unsigned v=0;v<4;v++) {
            float u=uv[tile+right[v]],t=bottom[v];
            float source_y=interpolate(original.vertices[0].position[1],original.vertices[1].position[1],original.vertices[3].position[1],original.vertices[2].position[1],u,t);
            float world_y=py*source_y+original.draw.model_view[7];
            out->vertices[v].position[0]=(edges[tile+right[v]]-original.draw.model_view[3])/px;
            out->vertices[v].position[1]=(-228.f+(world_y+228.f)*sy-original.draw.model_view[7])/py;
            sample_quad(&original,out,v,u,t);
        }
        return;
    }
    if(tile>=grid.shown_count)return;
    const unsigned source=grid.shown[tile]%12;
    float x,y;tile_position(grid.shown[tile],&x,&y);
    const float sx=out->draw.model_view[0],sy=out->draw.model_view[5];
    if(fabsf(sx)<.01f || fabsf(sy)<.01f){out->skipped=1;return;}
    x=(x-out->draw.model_view[3])/sx;y=(y-out->draw.model_view[7])/sy;
    const float center_u=(source%4+.5f)/4.f,center_v=(source/4+.5f)/3.f;
    float center[2];
    for(unsigned a=0;a<2;a++)center[a]=interpolate(original.vertices[0].position[a],original.vertices[1].position[a],original.vertices[3].position[a],original.vertices[2].position[a],center_u,center_v);
    const unsigned right[4]={0,1,1,0},bottom[4]={0,0,1,1};
    for(unsigned v=0;v<4;v++) {
        float u=(source%4+right[v])/4.f,t=(source/4+bottom[v])/3.f;
        for(unsigned a=0;a<2;a++) {
            float pos=interpolate(original.vertices[0].position[a],original.vertices[1].position[a],original.vertices[3].position[a],original.vertices[2].position[a],u,t);
            out->vertices[v].position[a]=(a?y:x)+(pos-center[a])*(a?grid.sy:grid.sx);
        }
        sample_quad(&original,out,v,u,t);
    }
    const char* name=(const char*)ptr(c,pane+0xb4,17);
    if(name && !strcmp(name,"Picture_00"))background_matte(out);
}
