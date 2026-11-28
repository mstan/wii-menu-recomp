/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 4.3U channel-library policy belongs to this consumer. Metadata/order live on
 * the host; the original manager and scene retain their 48 resident records.
 * Never put virtual indices in original arrays or serialize them to iplsave. */
#include <wiillr/fs.h>
#include <inttypes.h>
enum {LIBRARY_MAX=48*128,LOAD_META=0x8133955c,LIST_REMOVE=0x81512270,
      DESTROY_OBJECT=0x813acbb8,APPEND_OBJECT=0x813acae0,CREATE_THUMBS=0x813abcc4};
static unsigned requested_multiplier=1;
static struct {
    WiiFs* fs;unsigned capacity,anchor,populated,omitted;
    uint8_t info[LIBRARY_MAX][16];uint64_t order[LIBRARY_MAX];uint8_t reserved[LIBRARY_MAX];bool order_valid;
    CPUState* cpu;uint32_t manager;bool ready;
    int resident[48];
    struct {bool active,override;unsigned physical,logical;uint32_t object;} load;
    char error[160];
} library={.capacity=48};
const char* wii_menu_grid_library_error(void){return library.error;}
static uint32_t be32(const uint8_t* p) {return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t info_title(const uint8_t info[16]) {return (uint64_t)be32(info+8)<<32|be32(info+12);}
static void title_info(uint8_t info[16],uint64_t title) {
    memset(info,0,16);info[0]=3;info[7]=14; /* SCENE_NORMAL_CHANNEL */
    for(unsigned n=0;n<8;n++)info[8+n]=(uint8_t)(title>>(56-n*8));
}
bool wii_menu_grid_library_configure(unsigned multiplier) {
    if(!multiplier || multiplier>128 || (multiplier&(multiplier-1)) || (multiplier>1 && !wii_menu_grid_supported()))return false;
    requested_multiplier=multiplier;return true;
}
void wii_menu_grid_library_attach(WiiFs* fs) {
    memset(&library,0,sizeof library);library.capacity=48;
    if(!fs)return;
    const char* override=getenv("WIILLR_MENU_CHANNEL_MULTIPLIER");
    if(override) {char* end=NULL;unsigned long value=strtoul(override,&end,10);
        if(end!=override && !*end && value<=128)wii_menu_grid_library_configure((unsigned)value);}
    library.fs=fs;library.capacity=48*requested_multiplier;
    for(unsigned n=0;n<48;n++)library.resident[n]=n;
    if(library.capacity>48) {
        library.order_valid=wii_fs_menu_library_read(fs,library.order,&library.anchor);
        if(!library.order_valid)strcpy(library.error,"Channel order could not be read; existing sidecar preserved.");
        if(library.anchor>=library.capacity)library.anchor=0;
    }
}
static unsigned channel_capacity(void) {return library.capacity;}
static bool grid_requested(void) {return (enabled && fit) || library.capacity>48;}
static int logical_slot(unsigned physical) {
    return physical<48?(library.ready?library.resident[physical]:(int)physical):-1;
}
static int physical_slot(unsigned logical) {
    if(!library.ready)return logical<48?(int)logical:-1;
    for(unsigned n=0;n<48;n++)if(library.resident[n]==(int)logical)return n;
    return -1;
}
static uint32_t channel_record(unsigned physical) {return library.manager+12+physical*0x70;}
static bool installed_channel(uint64_t title) {
    const uint32_t type=(uint32_t)(title>>32);
    if(type!=0x10001 && type!=0x10002 && type!=0x10004)return false;
    uint32_t size=0,tickets=0,contents=0;
    return wii_es_tmd_view(library.fs,title,NULL,0,&size)==0 && size>=0x6c &&
        wii_es_ticket_views(library.fs,title,NULL,0,&tickets)==0 && tickets>0 &&
        wii_es_stored_contents(library.fs,title,NULL,0,&contents)==0 && contents==(size-0x5c)/16;
}
static bool catalog_contains(uint64_t title) {
    for(unsigned n=0;n<LIBRARY_MAX;n++)if(library.info[n][0]==3 && info_title(library.info[n])==title)return true;
    return false;
}
static void save_anchor(unsigned anchor) {
    if(library.capacity<=48 || anchor>=library.capacity || library.anchor==anchor)return;
    library.anchor=anchor;
    if(library.ready && library.order_valid && !wii_fs_menu_library_write(library.fs,library.order,anchor))
        strcpy(library.error,"Channel page could not be saved; native saves are unchanged.");
}
static void library_shortcuts(const char* text,size_t size) {
    char line[160];
    if(size>=3 && !memcmp(text,"\xef\xbb\xbf",3)){text+=3;size-=3;}
    for(unsigned lines=0;lines<LIBRARY_MAX && size;lines++) {
        const char* end=memchr(text,'\n',size);size_t length=end?(size_t)(end-text):size;
        if(length>=sizeof line) {
            strcpy(library.error,"Shortcut configuration contains an overlong line.");break;
        }
        memcpy(line,text,length);line[length]=0;
        size_t consumed=length+(end!=NULL);text+=consumed;size-=consumed;
        if(memchr(line,0,length))continue;
        unsigned slot;char target[32],extra;
        if(sscanf(line,"%u %31s %c",&slot,target,&extra)!=2 || slot<48 || slot>=LIBRARY_MAX)continue;
        if(!strcmp(target,"-")) {
            memset(library.info[slot],0,16);library.order[slot]=0;library.reserved[slot]=1;continue;
        }
        if(strlen(target)!=16 || strspn(target,"0123456789abcdefABCDEF")!=16)continue;
        uint64_t title=strtoull(target,NULL,16);
        if(installed_channel(title)) {
            title_info(library.info[slot],title);library.order[slot]=title;library.reserved[slot]=0;
        }
    }
}
static void library_catalog(CPUState* c) {
    if(library.capacity<=48 || library.ready || !library.fs)return;
    uint32_t manager=word(c,0x81089008+0x84),save=word(c,0x81089008+0x94);
    if(!ptr(c,manager,0x150c) || !ptr(c,save+0x30,48*16))return;
    /* Wait for genuine asynchronous metadata ownership before recycling. */
    for(unsigned n=0;n<48;n++)if(word(c,manager+12+n*0x70+0x4c)!=2)return;
    library.cpu=c;library.manager=manager;
    if(library.order_valid)library.error[0]=0;
    memset(library.info,0,sizeof library.info);library.populated=library.omitted=0;
    memcpy(library.info,ptr(c,save+0x30,48*16),48*16);
    for(unsigned n=48;n<LIBRARY_MAX;n++)
        if(library.order[n] && installed_channel(library.order[n]))title_info(library.info[n],library.order[n]);
    /* Optional overflow placement/shortcuts. Never shadows a stock slot. Each
     * line is a zero-based position >=48 followed by an installed title ID.
     * Duplicate targets are independent shortcuts, not additional save data. */
    uint8_t* config=malloc(WIILLR_MENU_LIBRARY_CONFIG_LIMIT);
    uint32_t config_size=0;
    if(config && wii_fs_menu_library_config(library.fs,config,WIILLR_MENU_LIBRARY_CONFIG_LIMIT,&config_size))
        library_shortcuts((const char*)config,config_size);
    else strcpy(library.error,"Shortcut configuration could not be read; existing order retained.");
    free(config);
    uint32_t count=0;
    if(wii_es_titles(library.fs,NULL,0,&count)==0 && count<=131072) {
        const uint32_t allocation=count;
        uint8_t* titles=calloc(allocation?allocation:1,8);
        if(titles && wii_es_titles(library.fs,titles,allocation,&count)==0 && count<=allocation) {
            unsigned next=48;
            for(unsigned n=0;n<count;n++) {
                uint64_t title=(uint64_t)be32(titles+n*8)<<32|be32(titles+n*8+4);
                if(catalog_contains(title) || !installed_channel(title))continue;
                while(next<LIBRARY_MAX && (library.order[next] || library.reserved[next]))next++;
                if(next==LIBRARY_MAX){library.omitted++;continue;}
                library.order[next]=title;title_info(library.info[next++],title);
            }
        }
        free(titles);
    }
    for(unsigned n=0;n<library.capacity;n++)if(library.info[n][0])library.populated++;
    for(unsigned n=library.capacity;n<LIBRARY_MAX;n++)if(library.info[n][0])library.omitted++;
    library.ready=true;
    if(library.order_valid && !wii_fs_menu_library_write(library.fs,library.order,library.anchor))
        strcpy(library.error,"Channel order could not be saved; native saves are unchanged.");
}
static uint32_t object_for(CPUState* c,unsigned physical) {
    uint32_t object=word(c,grid.scene+0x58),link=word(c,grid.scene+0x60)&0xffff;
    for(unsigned n=0;object && n<49;n++,object=word(c,object+link+4))
        if(word(c,object+0x1c)==physical/12 && word(c,object+0x20)==physical%12)return object;
    return 0;
}
static bool channel_ready(CPUState* c,unsigned logical) {
    if(library.capacity<=48)return true;
    int physical=physical_slot(logical);if(physical<0)return false;
    if(!library.ready || word(c,channel_record(physical)+0x4c)!=2)return false;
    uint32_t object=object_for(c,physical);
    return object && word(c,object+0x18)==3;
}
static void library_refresh(CPUState* c,uint32_t pc) {
    if(pc!=0x81337c64 || !library.ready || c!=library.cpu || c->gpr[3]!=library.manager)return;
    const uint8_t* allowed=ptr(c,library.manager+0x1b80,1);
    if(!allowed || !*allowed)return; /* Original refresh did not acquire ownership. */
    library.ready=false;library.load.active=library.load.override=false;
    for(unsigned n=0;n<48;n++)library.resident[n]=n;
}
/* Preload the next viewport and keep all currently visible cells. Recycle only
 * a fully created offscreen object whose RSO/CS worker has no active module.
 * The disc record is permanently pinned. No concurrent callback sees a record
 * reassigned while its metadata request is outstanding. */
static bool desired_slot(unsigned logical,const unsigned* desired,unsigned count) {
    for(unsigned n=0;n<count;n++)if(desired[n]==logical)return true;
    return false;
}
static int library_victim(CPUState* c,const unsigned* desired,unsigned count) {
    int best=-1;unsigned best_distance=0;
    for(unsigned n=0;n<48;n++) {
        int logical=library.resident[n];
        if(logical>=0 && desired_slot(logical,desired,count))continue;
        if(logical>=0 && in_extent(logical,grid.step_x*.75f))continue;
        uint32_t record=channel_record(n),object=object_for(c,n);
        const uint8_t* info=ptr(c,record+0x38,16);
        if(!info || info[0]==1 || word(c,record+0x4c)!=2)continue;
        if(object && (word(c,object+0x18)!=3 || word(c,object+0xec)>1))continue;
        unsigned distance=logical<0?UINT32_MAX:(unsigned)abs(logical-(int)grid.first);
        if(best<0 || distance>best_distance){best=n;best_distance=distance;}
    }
    return best;
}
static int guest_call(CPUState*,uint32_t,uint32_t,uint32_t,uint32_t,unsigned);
static int library_next(CPUState* c) {
    unsigned phase=word(c,grid.scene+0xc0);
    if(!library.ready || library.load.active || (phase>3 && phase!=12 && phase!=13))return 0;
    unsigned capacity=grid.columns*3,first=grid.first>capacity?grid.first-capacity:0;
    unsigned last=grid.first+capacity*3;if(last>library.capacity)last=library.capacity;
    unsigned wanted[47],count=0;
    for(unsigned n=first;n<last && count<47;n++)
        if(in_extent(n,grid.step_x*.75f))wanted[count++]=n;
    /* Ahead/behind preparation never displaces a visible cell. One resident
     * record remains reserved for the Disc Channel even far from its page. */
    for(unsigned n=grid.first;n<last && count<47;n++)
        if(!desired_slot(n,wanted,count))wanted[count++]=n;
    for(unsigned n=grid.first;n>first && count<47;)
        if(!desired_slot(--n,wanted,count))wanted[count++]=n;
    for(unsigned n=0;n<count;n++) {
        unsigned logical=wanted[n];if(physical_slot(logical)>=0)continue;
        int physical=library_victim(c,wanted,count);if(physical<0)return 0;
        /* Empty cells and aliases of an already cached title can change their
         * logical position without touching any guest object or allocation. */
        uint32_t record=channel_record(physical);
        if(!memcmp(ptr(c,record+0x38,16),library.info[logical],16)) {
            library.resident[physical]=logical;continue;
        }
        library.load.active=true;library.load.physical=physical;library.load.logical=logical;
        library.load.object=object_for(c,physical);
        grid.call.active=true;grid.call.lr=c->lr;grid.call.sp=c->gpr[1];grid.call.object=grid.scene;
        library.resident[physical]=-1;
        if(library.load.object)return guest_call(c,LIST_REMOVE,grid.scene+0x58,library.load.object,0,20);
        return guest_call(c,COMMON,grid.scene,0,0,21);
    }
    return 0;
}
static int library_continue(CPUState* c,unsigned stage) {
    if(stage==20)return guest_call(c,DESTROY_OBJECT,grid.scene,library.load.object,0,21);
    if(stage==21) {
        unsigned physical=library.load.physical;
        uint32_t record=channel_record(physical);
        uint32_t finished=word(c,0x81698690);if(finished)mem_write32(c,0x81698690,finished-1);
        const uint8_t* loaded=ptr(c,record+0x18,1);
        if(loaded && *loaded) {uint32_t valid=word(c,0x81698694);if(valid)mem_write32(c,0x81698694,valid-1);}
        uint32_t old_file=word(c,record+0x48);
        if(old_file)return guest_call(c,0x8133ccc4,old_file,1,0,25);
        stage=25;
    }
    if(stage==25) {
        unsigned physical=library.load.physical;
        for(unsigned n=0;n<0x70;n+=4)mem_write32(c,channel_record(physical)+n,0);
        library.load.override=true;
        return guest_call(c,LOAD_META,library.manager,physical/12,physical%12,22);
    }
    if(stage==22) {
        library.load.override=false;
        /* Async reads continue on the original scheduler. Return to calcCommon
         * instead of spinning or blocking the host waiting for NAND. */
        library.resident[library.load.physical]=library.load.logical;
        c->lr=grid.call.lr;c->gpr[3]=grid.scene;grid.call.active=false;
        return 0;
    }
    if(stage==23)return guest_call(c,CREATE_THUMBS,grid.scene,0,0,24);
    if(stage==24) {
        library.load.active=false;grid.refresh=true;
        c->lr=grid.call.lr;c->gpr[3]=grid.scene;grid.call.active=false;
    }
    return 0;
}
static int library_poll(CPUState* c) {
    if(!library.load.active || library.load.override || grid.call.active)return 0;
    if(word(c,channel_record(library.load.physical)+0x4c)!=2)return 0;
    grid.call.active=true;grid.call.lr=c->lr;grid.call.sp=c->gpr[1];grid.call.object=grid.scene;
    return guest_call(c,APPEND_OBJECT,grid.scene,library.load.physical/12,library.load.physical%12,23);
}
static int library_meta(CPUState* c,uint32_t pc) {
    if(pc!=0x813395b4 || !library.load.override || c->gpr[27]!=library.manager ||
       c->gpr[28]!=library.load.physical/12 || c->gpr[29]!=library.load.physical%12)return 0;
    const uint8_t* info=library.info[library.load.logical];
    /* Replace the value returned by getChanInfo, without modifying SaveData.
     * Five verified original instructions load four words and spill word zero. */
    c->gpr[0]=be32(info);c->gpr[10]=be32(info+4);c->gpr[9]=be32(info+8);c->gpr[8]=be32(info+12);
    mem_write32(c,c->gpr[1]+0x14,c->gpr[0]);c->pc=0x813395c8;c->cycles+=5;return 1;
}
