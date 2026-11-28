/* SPDX-License-Identifier: GPL-3.0-or-later */
#include WIILLR_MM9_NATIVE_TITLE
static void product_hook_pages(void){
    mm9_hook_pages();
#if WIILLR_MENU_PICTURES
    hook_pages[0x1520]|=HOOK_MENU;hook_pages[0x1521]|=HOOK_MENU;
#endif
}
static bool dispatch_hooks(CPUState* c,u32 a,u8 h){
    if(mm9_dispatch_hooks(c,a,h))return true;
#if WIILLR_MENU_PICTURES
    if((h&HOOK_MENU) && wii_menu_picture_hook_call(c,a))return true;
#endif
    return false;
}
static void product_invalidate(u32 a,u32 s){
    mm9_invalidate(a,s);
#if WIILLR_MENU_PICTURES
    wii_menu_picture_hook_invalidate(a,s);
#endif
}
