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
      GET_SCENE=0x8140b138,INIT_ARROW=0x8139ce28,ENCODED=0x10000};
typedef struct {uint32_t pane,component;float x,y,sx,sy;uint8_t visible,trigger;} Slot;
typedef struct {uint32_t pane;float sx,sy;} Root;
static bool enabled,fit=true;
static bool return_anchor_valid;
static unsigned return_anchor;
static unsigned width=1280,height=720;
static struct {
    CPUState* cpu;uint32_t scene,layout;Slot slots[3][12];Root roots[192];unsigned root_count;
    unsigned columns,page,pages,first,count,carrier,anchor;float world_width,sx,sy,gap_x,step_x,step_y;
    bool ready,refresh,arrows,refresh_modules;int selected;
    uint64_t next_scroll;
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
static bool visible(unsigned flat) {return flat>=grid.first && flat<grid.first+grid.count && flat<48;}
static int flat_for(int bank,int index) {
    const int page=(int)grid.carrier+bank-1;
    return page>=0 && page<4?page*12+index:-1;
}
static bool active(CPUState* c) {
    return enabled && fit && grid.ready && grid.cpu==c && word(c,grid.scene)==0x8164e0b8 &&
        word(c,grid.scene+0x68)==grid.layout;
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
        int bank,index;if(name && memchr(name,0,17) && (index=name_slot(name,&bank))>=0) {
            if(word(c,component+0x94)!=pane || grid.slots[bank][index].pane)return false;
            Slot* s=&grid.slots[bank][index];s->pane=pane;s->component=component;
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
    if(columns!=grid.columns) {
        /* Keep the first displayed channel nearby when the capacity changes. */
        grid.page=grid.anchor/(columns*3);grid.columns=columns;grid.arrows=true;
    }
    grid.pages=(48+columns*3-1)/(columns*3);if(grid.page>=grid.pages)grid.page=grid.pages-1;
    grid.first=grid.page*columns*3;grid.count=48-grid.first;if(grid.count>columns*3)grid.count=columns*3;
    const unsigned carrier=(grid.first+(grid.count-1)/2)/12;
    if(carrier!=word(c,grid.scene+0xc8))grid.refresh=true;
    grid.carrier=carrier;
    const float aspect=(float)width/height;
    grid.world_width=real(c,grid.scene+0xec)>1.1f?832.f:608.f;
    const float gap=11.f,card_aspect=1.725f;
    const float card_h=fminf(96.f,((aspect*456.f*.88f-gap*(columns-1))/columns)/card_aspect);
    grid.sx=(card_h*card_aspect*grid.world_width/(aspect*456.f))/170.f;
    grid.sy=card_h/96.f;grid.gap_x=gap*grid.world_width/(aspect*456.f);
    grid.step_x=170.f*grid.sx+grid.gap_x;grid.step_y=card_h+gap;
}
static void tile_position(unsigned flat,float* x,float* y) {
    unsigned local=flat-grid.first;
    *x=((int)(local%grid.columns)-(grid.columns-1)*.5f)*grid.step_x;
    *y=148.f-(local/grid.columns)*grid.step_y;
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
        if(stage==1 && grid.refresh_modules)return guest_call(c,RESTART_MODULES,grid.scene,0,0,2);
        if(stage<=2)return guest_call(c,GET_SCENE,word(c,0x81089008+0x64),5,0,3);
        if(stage==3) {
            grid.call.button=c->gpr[3];
            if(grid.call.button)return guest_call(c,INIT_ARROW,grid.call.button,1,grid.page>0,4);
        }
        if(stage==4)return guest_call(c,INIT_ARROW,grid.call.button,0,grid.page+1<grid.pages,5);
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
    const unsigned phase=word(c,scene+0xc0);
    if((phase==1 || phase==3) && (grid.refresh || grid.arrows)) {
        grid.call.active=true;grid.call.lr=c->lr;grid.call.sp=c->gpr[1];grid.call.object=scene;
        if(grid.refresh) {
            mem_write32(c,scene+0xc8,grid.carrier);
            return guest_call(c,REFRESH,scene,grid.carrier,0,1);
        }
        return guest_call(c,GET_SCENE,word(c,0x81089008+0x64),5,0,3);
    }
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
        Slot* s=&grid.slots[b][i];int flat=flat_for(b,i);bool show=flat>=0 && visible(flat);
        visibility(c,s->pane,show);mem_write8(c,s->component+0x7c,show);
    }
    /* Cache the roots owned by the current channel list. The C ut::List has
     * object pointers and {prev,next} links, unlike Pane's C++ sentinel list. */
    grid.root_count=0;
    uint32_t object=word(c,scene+0x58);unsigned link=word(c,scene+0x60)&0xffff;
    for(unsigned n=0;object && n<49;n++) {
        if(word(c,object+0x1c)<4 && word(c,object+0x20)<12) {
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
    return 0;
}
void wii_menu_grid_transform(CPUState* c,uint32_t pane) {
    if(!active(c))return;
    for(unsigned b=0;b<3;b++)for(unsigned i=0;i<12;i++) {
        Slot* s=&grid.slots[b][i];if(pane!=s->pane)continue;
        const int flat=flat_for(b,i);if(flat<0 || !visible(flat))return;
        const uint32_t parent=word(c,pane+0xc);float x,y;tile_position(flat,&x,&y);
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
        if(pc==0x813ad55c)return result(c,c->gpr[4]<4 && c->gpr[5]<12 && visible(c->gpr[4]*12+c->gpr[5]));
        if(pc==0x813ad39c)return result(c,0); /* No neighboring page decorations. */
        if(pc==0x813ace78 && (c->gpr[5]&0xffff0000u)==ENCODED) {
            unsigned flat=c->gpr[5]&0xffff;c->gpr[4]=flat/12;c->gpr[5]=flat%12;return 0;
        }
        if(pc==0x813ae99c) {
            const char* n=(const char*)ptr(c,c->gpr[4],17);int b,i;
            if(n && memchr(n,0,17) && (i=name_slot(n,&b))>=0) {
                int flat=flat_for(b,i);return result(c,flat>=0 && visible(flat)?ENCODED|(unsigned)flat:UINT32_MAX);
            }
            return result(c,UINT32_MAX);
        }
        if(pc==0x813ada94) {
            grid.selected=(int)(c->gpr[4]*12+c->gpr[5]);
            return_anchor=grid.selected;return_anchor_valid=true;
        }
        if((pc==0x813adf18 || pc==0x813adf48) && grid.selected>=0 && word(c,grid.scene+0xc0)>=7 &&
           c->gpr[4]==(unsigned)grid.selected%12) {
            int bank=grid.selected/12-(int)grid.carrier+1;
            if(bank>=0 && bank<3)return result(c,grid.slots[bank][c->gpr[4]].pane);
        }
        if(pc==0x813ad8e0) {
            grid.next_scroll=c->cycles+240000000;
            if(c->gpr[4]==12 && grid.page)grid.page--;
            else if(c->gpr[4]==13 && grid.page+1<grid.pages)grid.page++;
            grid.anchor=grid.page*grid.columns*3;grid.refresh=grid.arrows=grid.refresh_modules=true;
            mem_write32(c,grid.scene+0xc0,1);return result(c,0);
        }
        if(pc==0x813ad8b8 && c->cycles<grid.next_scroll)return result(c,0);
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
int wii_menu_grid_picture_tiles(CPUState* c,uint32_t pane) {
    if(!active(c))return 0;
    uint32_t root=word(c,grid.layout+0x14),ancestor=pane;
    for(unsigned n=0;n<8 && ancestor && ancestor!=root;n++)ancestor=word(c,ancestor+0xc);
    if(ancestor!=root)return 0;
    const char* name=(const char*)ptr(c,pane+0xb4,17);
    if(!name || !memchr(name,0,17))return 0;
    if(!strcmp(name,"ChMask"))return -1;
    if(!strncmp(name,"BaseMask",8))return !strcmp(name,"BaseMask2")?(int)grid.count:-1;
    if(!strncmp(name,"Edge",4))return !strcmp(name,"Edge2")?(int)grid.count:-1;
    if(!strncmp(name,"Picture_0",9))return !strcmp(name,"Picture_00")?(int)grid.count:-1;
    return 0;
}
static float interpolate(float a,float b,float d,float e,float u,float v) {
    return (a+(b-a)*u)*(1-v)+(d+(e-d)*u)*v;
}
void wii_menu_grid_picture_tile(CPUState* c,uint32_t pane,unsigned tile,WiiMenuPicture* out) {
    if(!active(c) || tile>=grid.count)return;
    const WiiMenuPicture original=*out;
    const unsigned source=(grid.first+tile)%12;
    float x,y;tile_position(grid.first+tile,&x,&y);
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
        for(unsigned a=0;a<2;a++) {
            out->vertices[v].uv[a]=interpolate(original.vertices[0].uv[a],original.vertices[1].uv[a],original.vertices[3].uv[a],original.vertices[2].uv[a],u,t);
            out->extra_uv[v][a]=interpolate(original.extra_uv[0][a],original.extra_uv[1][a],original.extra_uv[3][a],original.extra_uv[2][a],u,t);
            for(unsigned j=0;j<7;j++)out->program_extra_uv[v][j][a]=interpolate(original.program_extra_uv[0][j][a],original.program_extra_uv[1][j][a],original.program_extra_uv[3][j][a],original.program_extra_uv[2][j][a],u,t);
        }
        for(unsigned a=0;a<4;a++)out->vertices[v].color[a]=interpolate(original.vertices[0].color[a],original.vertices[1].color[a],original.vertices[3].color[a],original.vertices[2].color[a],u,t);
    }
}
