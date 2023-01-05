#include "ma35d1-dcultrafb.h"
#define Delay_SLEEP 0

void dump_vscreeninfo(struct fb_var_screeninfo *fvsi);
void dump_fscreeninfo(struct fb_fix_screeninfo *ffsi);

int GetFixScreenInfo(
    int fd,
    struct fb_fix_screeninfo *fix
    );

int GetVarScreenInfo(
    int fd,
    struct fb_var_screeninfo *var
    );

int SetVarScreenInfo(
    int fd,
    struct fb_var_screeninfo *var
    );

/* Read picture data to the specified buffer. */
int ShowPage(
    char *name,
    struct fb_var_screeninfo *var,
    char *usermem,
    unsigned int oneframesize
    );

/* Write the specified data to the buffer */
int CleanScreen(
    unsigned int color,
    struct fb_var_screeninfo *var,
    char *usermem,
    unsigned int oneframesize
    );

/* Calculate weight array for sync filter. */
unsigned int * calculate_sync_table(
    unsigned char kernel_size,
    unsigned int src_size,
    unsigned int dst_size
    );

int SetBufferSize(
    int fd,
    dc_frame_info size
    );

int SetBufferFormat(int fd, unsigned int format);
int GetFBAddr(int fd, unsigned int *data);
int GetBufferFormat(int fd, unsigned int *data);

/* Set the display window of overlay on the screen. */
int SetOverlayRect(
    int fd,
    dc_overlay_rect rect
    );

/* Set filter tap of scale. */
int SetScaleFilterTap(
    int fd,
    dc_filter_tap filtertap
    );

int SetCursor(int fd,
    struct fb_cursor cursor
    );

int SetEnableDither(
    int fd,
    bool enable
    );

int SetColorkey(
    int fd,
    dc_color_key colorkey
    );

int SetPanDisplay(
    int fd,
    struct fb_var_screeninfo *var
    );

int SetBlendingMode(
    int fd,
    dc_alpha_blending_mode mode
    );

int SetGlobalAlpha(
    int fd,
    dc_global_alpha global_alpha
    );

/* Alloc memory in user space. */
void * os_mem_alloc(
    unsigned int size
    );

/* Free memory in user space. */
void os_mem_free(
    void * addr
    );

