static unsigned char menu_sha256[1][32];
static unsigned char menu_boot_sha256[32];
static bool menu_boot_verified;
static bool menu_package_started;
static WiiPackage* menu_package;
static char menu_system_status[1024];
static WiiNandSetupInfo menu_import_info;
static unsigned char menu_import_sha256[32];
static char menu_keys_path[1024];
#include WIILLR_MM9_LAUNCHER_TITLE
#include "menu_launcher_mods.inc"
static const char* game_caption(const WiiGx* gx){return gx->title==0x100000002ull?"Wii Menu":"Mega Man 9";}
static bool product_pointing(const WiiGx* gx){return gx->title==0x100000002ull;}
static bool product_default_fit(const RecompLauncherCGameInfo* game){return game->name && !strcmp(game->name,"Wii Menu");}
static bool product_renderer_active(const WiiGx* gx){return gx->title==0x1000157523945ull;}
static bool product_active(const WiiGx* gx){return gx->title==0x100000002ull || gx->title==0x1000157523945ull;}
static void product_extent(const WiiGx* gx,unsigned w,unsigned h,bool fit){if(product_pointing(gx))wii_menu_grid_extent(w,h,fit);}
static void product_in_session(void){wii_launcher_mods_in_session();}
static void product_mod_finish(const RecompLauncherCGameInfo* game,const RecompLauncherCSettings* before){menu_mod_finish(game,before);}
static void product_host_actions(unsigned held){wii_mm9_mods_host_actions(held);}
static bool product_set_view(WiiGx* gx,int value){
#if WIILLR_MM9_RENDERER
    return gx->title!=0x1000157523945ull || wii_mm9_renderer_set_view(gx,(WiiMm9View)value);
#else
    (void)gx;(void)value;return false;
#endif
}
bool wii_launcher_menu_session(const char* image) {
    const char* directory=getenv("WIILLR_USER_DIR");char* pref=NULL;
    if(!directory || !*directory) {
        pref=SDL_GetPrefPath("RetroPortingToolKit","Wii Menu");directory=pref;
    }
    if(!directory || strlen(directory)>=sizeof launcher_directory ||
       !image || strlen(image)>=sizeof boot_image ||
       snprintf(config_path,sizeof config_path,"%s/config.ini",directory)>=(int)sizeof config_path ||
       snprintf(rom_cache_path,sizeof rom_cache_path,"%s/boot.cfg",directory)>=(int)sizeof rom_cache_path) {
        SDL_free(pref);return false;
    }
    strcpy(launcher_directory,directory);
    memset(menu_sha256,0,sizeof menu_sha256);
    if(*image) {
        size_t size=0;void* original=SDL_LoadFile(image,&size);
        if(!original){SDL_free(pref);return false;}
        recompui_sha256_compute(original,size,menu_sha256[0]);SDL_free(original);
    }
    RecompLauncherCGameInfo game={0};launcher_profile_apply("wii",&game);
    game.name="Wii Menu";game.num_players=4;game.config_path=config_path;
    game.rom_noun="Wii system";game.channel_grid_art=1;
    game.system_media_title="Wii system files";
    game.system_media_status=menu_system_status;
    game.system_media_note="Import your Wii Menu 4.3U nand.bin (or its folder), or an extracted NAND root containing title, ticket, sys and shared2. Matching keys.bin beside the backup is detected automatically. Your original files and saves are preserved; the Menu uses a managed copy.";
    game.system_media_folders=1;game.host_persists_paths=1;
    game.rom_cache_path=rom_cache_path;game.settings_bindings=1;
    game.known_sha256=(const uint8_t(*)[32])menu_sha256;game.num_known_sha256=1;
    if(view_supported()) {
        game.has_supersampling=1;game.internal_resolution_labels=resolution_labels;
        game.internal_resolution_values=resolution_values;game.num_internal_resolutions=6;
        game.internal_resolution_note="Raster resolution for geometry and effects. Bitmap artwork retains its original detail.";
    }
    load_settings(&game);menu_mod_prepare(&game);
    if(!wii_launcher_peripherals_prepare(&game,directory)){SDL_free(pref);return false;}
    game_info=game;strcpy(boot_image,image);SDL_free(pref);
    char volume[8];snprintf(volume,sizeof volume,"%d",settings.enable_audio?settings.volume:0);
    if(!set_env("WIILLR_WINDOW_TITLE","Wii Menu") || !set_env("GCN_WINDOW","1") ||
       !set_env("GCN_AUDIO_VOLUME",volume))return false;
    wii_launcher_input_reset();
    GcnllrPad initial_input;wii_launcher_input_poll(&settings,&initial_input);
    active=true;return true;
}

static void menu_identity(const WiiNandSetupInfo* info) {
    snprintf(menu_system_status,sizeof menu_system_status,"%s%s%s",
        info->menu_label,info->region_label[0]?" · ":"",info->region_label);
}
static int import_menu_system(const char* source,char* out,size_t out_capacity,
                              char* error,size_t error_capacity) {
    if(!menu_package) {
        snprintf(error,error_capacity,"Use the packaged Wii Menu launcher to import system files.");return 0;
    }
    WiiNandSetupInfo info={0};
    if(!wii_package_import_system(menu_package,source,*menu_keys_path?menu_keys_path:NULL,&info,error,error_capacity))return 0;
    const int n=snprintf(out,out_capacity,"%s/%s",wii_package_nand(menu_package),info.boot_relative);
    if(n<0 || (size_t)n>=out_capacity) {
        snprintf(error,error_capacity,"The managed system path is too long.");return 0;
    }
    size_t size=0;void* data=SDL_LoadFile(out,&size);
    if(!data) {snprintf(error,error_capacity,"Could not read the imported Wii Menu.");return 0;}
    recompui_sha256_compute(data,size,menu_import_sha256);SDL_free(data);
    menu_import_info=info;return 1;
}
static void menu_imported(const char* boot_path) {
    (void)boot_path;
    memcpy(menu_sha256[0],menu_import_sha256,sizeof menu_import_sha256);
    menu_identity(&menu_import_info);
}

int wii_launcher_menu_package(WiiPackage* package,bool force,bool check_only) {
    if(!package || !wii_package_menu(package))return 2;
    char error[1024]={0},image[32768]={0};WiiNandSetupInfo info={0};
    const bool ready=wii_package_ready(package);
    if(ready) {
        if(!wii_package_refresh(package,&info,error,sizeof error)) {
            fprintf(stderr,"launcher: %s\n",error);return 2;
        }
        const int n=snprintf(image,sizeof image,"%s/%s",wii_package_nand(package),info.boot_relative);
        if(n<0 || n>=(int)sizeof image)return 2;
        menu_identity(&info);
    } else {
        (void)wii_package_refresh(package,&info,error,sizeof error);
        SDL_PathInfo path_info;
        const bool existing=SDL_GetPathInfo(wii_package_nand(package),&path_info);
        snprintf(menu_system_status,sizeof menu_system_status,"%s",
            existing && *error?error:"System files have not been imported.");
    }
    if(check_only)return ready?0:2;
    if(!wii_launcher_menu_session(image))return 2;
    menu_package=package;
    static const char* const patterns[]={"*.bin"};
    game_info.rom_patterns=patterns;game_info.num_rom_patterns=1;
    game_info.rom_filter_desc="Wii NAND backup (nand.bin)";
    game_info.prepare_disc=import_menu_system;
    game_info.system_media_imported=menu_imported;
    game_info.system_media_keys_path=menu_keys_path;
    game_info.system_media_keys_capacity=sizeof menu_keys_path;
    game_info.prepare_busy_status="Checking and importing Wii system files...";
    game_info.prepare_success_status="Wii system files are ready.";
    const RecompLauncherCSettings before=settings;
    int action=RECOMP_LAUNCHER_RESULT_LAUNCH;
    char selected[32768];
    if(!ready || force || !settings.skip_launcher || !launch_devices_ready(&game_info))
        action=recomp_launcher_run_window("Wii Menu",&settings,&game_info,NULL,
                                          *image?image:NULL,selected,sizeof selected);
    menu_mod_finish(&game_info,&before);
    if(action==RECOMP_LAUNCHER_RESULT_QUIT)return 1;
    if(action!=RECOMP_LAUNCHER_RESULT_LAUNCH ||
       !wii_package_refresh(package,&info,error,sizeof error)) {
        fprintf(stderr,"launcher: %s\n",error);return 2;
    }
    menu_package_started=true;return 0;
}

static bool installed_menu_valid(const char* image,const char* nand,unsigned uid,unsigned gid,unsigned ios_profile) {
    menu_boot_verified=false;
    if(!image || !nand || gid>0xffffu)return false;
    WiiFs* fs=wii_fs_mount(nand,uid,(uint16_t)gid);
    uint64_t title=0;uint32_t count=0;unsigned char view[0xd8];
    WiiTitleLaunch launch={0};int32_t status=-1;
    bool valid=fs && wii_fs_boot_title(fs,&title) && title==0x0000000100000002ull &&
        wii_es_ticket_views(fs,title,view,1,&count)==0 && count==1 &&
        wii_es_prepare_launch(fs,title,view,&launch,&status) && status==0 &&
        launch.title==title && launch.uid==uid && launch.gid==gid &&
        (launch.ios==0x100000004ull || launch.ios==0x100000050ull) &&
        (!ios_profile || launch.ios==(0x100000000ull|ios_profile)) &&
        launch.image && launch.size>=0x100 && launch.size<=0x5800000;
    if(valid) {
        FILE* file=fopen(image,"rb");valid=false;
        if(file) {
            if(!fseek(file,0,SEEK_END) && ftell(file)==(long)launch.size && !fseek(file,0,SEEK_SET)) {
                unsigned char part[16384];size_t offset=0;valid=true;
                while(offset<launch.size) {
                    const size_t remaining=launch.size-offset,n=remaining<sizeof part?remaining:sizeof part;
                    if(fread(part,1,n,file)!=n || memcmp(part,launch.image+offset,n)){valid=false;break;}
                    offset+=n;
                }
                valid=valid && fgetc(file)==EOF && !ferror(file);
            }
            fclose(file);
        }
        if(valid){recompui_sha256_compute(launch.image,launch.size,menu_boot_sha256);menu_boot_verified=true;}
    }
    wii_es_release_launch(&launch);if(fs)wii_fs_unmount(fs);return valid;
}
bool wii_launcher_menu_verify(const void* image,size_t size) {
    if(!menu_boot_verified || !image || !size)return false;
    unsigned char hash[32];recompui_sha256_compute(image,size,hash);
    return !memcmp(hash,menu_boot_sha256,sizeof hash);
}
int wii_launcher_menu_start(const char* image,const char* nand,unsigned uid,unsigned gid,
    bool force,bool check_only,char* selected,size_t capacity,unsigned ios_profile) {
    if(!selected || !image || strlen(image)>=capacity ||
       !installed_menu_valid(image,nand,uid,gid,ios_profile)) {
        fprintf(stderr,"launcher: the installed Wii Menu title, content or IOS profile could not be verified\n");return 2;
    }
    strcpy(selected,image);
    if(check_only){puts("{\"launcher_title_verified\":true,\"window_opened\":false}");return 0;}
    if(menu_package_started) {
        menu_package_started=false;
        goto menu_selected;
    }
    if(!wii_launcher_menu_session(image))return 2;
    const RecompLauncherCSettings before=settings;
    int action=RECOMP_LAUNCHER_RESULT_LAUNCH;
    if(force || !settings.skip_launcher || !launch_devices_ready(&game_info))
        action=recomp_launcher_run_window("Wii Menu",&settings,&game_info,NULL,image,selected,capacity);
    menu_mod_finish(&game_info,&before);
    if(action==RECOMP_LAUNCHER_RESULT_QUIT)return 1;
    if(action!=RECOMP_LAUNCHER_RESULT_LAUNCH || !installed_menu_valid(selected,nand,uid,gid,ios_profile) ||
       strlen(selected)>=sizeof boot_image)return 2;
menu_selected:
    strcpy(boot_image,selected);
    char volume[8];snprintf(volume,sizeof volume,"%d",settings.enable_audio?settings.volume:0);
    if(!set_env("WIILLR_WINDOW","1") || !set_env("GCN_WINDOW","1") ||
       !set_env("WIILLR_WINDOW_TITLE","Wii Menu") || !set_env("GCN_AUDIO_VOLUME",volume))return 2;
    if(!getenv("GCN_AUDIO") && !set_env("GCN_AUDIO","1"))return 2;
    wii_launcher_input_reset();active=true;return 0;
}
