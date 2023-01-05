#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"
#include "ma35d1-dcultrafb.h"
#include "math.h"

#define MATH_Add(X, Y)                (float)((X) + (Y))
#define MATH_Divide(X, Y)             (float)((X) / (Y))
#define MATH_Multiply(X, Y)           MATH_Divide((X), MATH_Divide(1.0f, (Y)))
#define MATH_DivideFromUInteger(X, Y)   (float)(X) / (float)(Y)
#define MATH_Int2Float(X)             (float)(X)
#define MATH_Sine(X)                  (float)(sinf((X)))

/* Alignment with a power of two value. */
#define ALIGN(n, align) \
( \
    ((n) + ((align) - 1)) & ~((align) - 1) \
)

#define MAXKERNELSIZE                 9
#define SUBPIXELINDEXBITS             5
#define PI                            3.14159265358979323846f

#define SUBPIXELCOUNT \
    (1 << SUBPIXELINDEXBITS)

#define SUBPIXELLOADCOUNT \
    (SUBPIXELCOUNT / 2 + 1)

#define WEIGHTSTATECOUNT \
    (((SUBPIXELLOADCOUNT * MAXKERNELSIZE + 1) & ~1) / 2)

#define KERNELTABLESIZE \
    (SUBPIXELLOADCOUNT * MAXKERNELSIZE * sizeof(unsigned short))

#define KERNELSTATES \
    (ALIGN(KERNELTABLESIZE + 4, 8))

void dump_vscreeninfo(struct fb_var_screeninfo *fvsi)
{
	printf("======= FB VAR SCREENINFO =======\n");
	printf("xres: %d\n", fvsi->xres);
	printf("yres: %d\n", fvsi->yres);
	printf("yres_virtual: %d\n", fvsi->yres_virtual);
	printf("buffer number: %d\n", fvsi->yres_virtual / fvsi->yres);
	printf("bpp : %d\n", fvsi->bits_per_pixel);
	printf("red bits    :\n");
	printf("    offset   : %d\n", fvsi->red.offset);
	printf("    length   : %d\n", fvsi->red.length);
	printf("    msb_right: %d\n", fvsi->red.msb_right);
	printf("green bits  :\n");
	printf("    offset   : %d\n", fvsi->green.offset);
	printf("    length   : %d\n", fvsi->green.length);
	printf("    msb_right: %d\n", fvsi->green.msb_right);
	printf("blue bits   :\n");
	printf("    offset   : %d\n", fvsi->blue.offset);
	printf("    length   : %d\n", fvsi->blue.length);
	printf("    msb_right: %d\n", fvsi->blue.msb_right);
	printf("transp bits :\n");
	printf("    offset   : %d\n", fvsi->transp.offset);
	printf("    length   : %d\n", fvsi->transp.length);
	printf("    msb_right: %d\n", fvsi->transp.msb_right);

	printf("=================================\n");
}

void dump_fscreeninfo(struct fb_fix_screeninfo *ffsi)
{
	printf("======= FB FIX SCREENINFO =======\n");
	printf("id          : %s\n", ffsi->id);
	printf("smem_start  : 0x%08lX\n", ffsi->smem_start);
	printf("smem_len    : %u\n", ffsi->smem_len);
	printf("line_length : %u\n", ffsi->line_length);
	printf("=================================\n");
}

int ShowPage(char *name, struct fb_var_screeninfo *var, char *usermem, unsigned int oneframesize)
{
    FILE *fp;
    char *temp = NULL;

    fp = fopen(name, "rb");
    if(fp == NULL)
    {
        printf("can't open %s\n", name);
    }

    fseek(fp, 0L, SEEK_SET);
    temp = (char *)(usermem + var->yoffset * var->xres * var->bits_per_pixel / 8);
    fread(temp, sizeof(char), oneframesize, fp);
    fclose(fp);

    return 0;
}

int CleanScreen(unsigned int color, struct fb_var_screeninfo *var, char *usermem, unsigned int oneframesize)
{
    unsigned int *addr;
    unsigned int i = 0;
    unsigned int size = 0;

    addr = (unsigned int *)(usermem + var->yoffset * var->xres * var->bits_per_pixel / 8);
    size = oneframesize;

    if (var->bits_per_pixel == 32)
    {
        while (i < size)
        {
            *addr = color;
            addr++;
            i += 4;
        }
    }
    else
    {
        /* ToDo */
    }
    return 0;
}

int GetFixScreenInfo(int fd, struct fb_fix_screeninfo *fix)
{
    int ret = 0;

    ret = ioctl(fd, FBIOGET_FSCREENINFO, fix);
    if (ret < 0)
    {
        printf("can't get fix\n");
        return -1;
    }
    return 0;
}

int GetVarScreenInfo(int fd, struct fb_var_screeninfo *var)
{
    int ret = 0;

    ret = ioctl(fd, FBIOGET_VSCREENINFO, var);
    if (ret < 0)
    {
        printf("can't get var\n");
        return -1;
    }
    return 0;
}

int SetVarScreenInfo(int fd, struct fb_var_screeninfo *var)
{
    int ret = 0;

    ret = ioctl(fd, FBIOPUT_VSCREENINFO, var);
    if (ret != 0)
    {
        printf("error set var\n");
        return -1;
    }
    return 0;
}

int SetBufferSize(int fd, dc_frame_info size)
{
    int ret = 0;

    ret = ioctl(fd, ULTRAFBIO_BUFFER_SIZE, &size);
    if (ret < 0)
    {
        printf("set buffer size error\n");
        return -1;
    }
    return 0;
}

int GetFBAddr(int fd, unsigned int *data)
{
    int ret = 0;

    ret = ioctl(fd, ULTRAFBIO_GET_FBADDR, data);
    if (ret < 0)
    {
        printf("get buffer address error\n");
        return -1;
    }
    return 0;
}

int SetBufferFormat(int fd, unsigned int format)
{
    int ret = 0;

    ret = ioctl(fd, ULTRAFBIO_SET_FORMAT, &format);
    if (ret < 0)
    {
        printf("set buffer format error\n");
        return -1;
    }
    return 0;
}


int GetBufferFormat(int fd, unsigned int *data)
{
    int ret = 0;

    ret = ioctl(fd, ULTRAFBIO_GET_FORMAT, data);
    if (ret < 0)
    {
        printf("get buffer format error\n");
        return -1;
    }
    return 0;
}

int SetOverlayRect(int fd, dc_overlay_rect rect)
{
    int ret = 0;
    ret = ioctl(fd, ULTRAFBIO_OVERLAY_RECT, &rect);
    if (ret < 0)
    {
        printf("set overlay rect error\n");
        return -1;
    }
    return 0;
}

int SetCursor(int fd, struct fb_cursor cursor)
{
    int ret = 0;
    ret = ioctl(fd, ULTRAFBIO_CURSOR, &cursor);
    if (ret < 0)
    {
        printf("set rgb cursor error\n");
        return -1;
    }
    return 0;
}

int SetColorkey(int fd, dc_color_key colorkey)
{
    int ret = 0;
    ret = ioctl(fd, ULTRAFBIO_COLORKEY, &colorkey);
    if (ret < 0)
    {
        printf("set colorkey error\n");
        return -1;
    }
    return 0;
}

#if 0
int SetEnableGamma(int fd, bool enable)
{
    int ret = 0;
    dc_gamma_table table;

    table.gamma_enable = enable;
    if (table.gamma_enable)
    {
        calculate_gamma_table(&table);
    }

    ret = ioctl(fd, ULTRAFBIO_GAMMA, &table);
    if (ret < 0)
    {
        printf("set gamma error\n");
        return -1;
    }
    return 0;
}
#endif


int SetEnableDither(int fd, bool enable)
{
    int ret = 0;

    ret = ioctl(fd, ULTRAFBIO_DITHER, &enable);
    if (ret < 0)
    {
        printf("set enable dither error\n");
        return -1;
    }
    return 0;
}

int SetPanDisplay(int fd, struct fb_var_screeninfo *var)
{
    int ret = 0;
    ret = ioctl(fd, FBIOPAN_DISPLAY, var);
    if (ret < 0)
    {
        printf("pandisplay error\n");
        return -1;
    }
    return 0;
}

int SetBlendingMode(int fd, dc_alpha_blending_mode mode)
{
    int ret = 0;
    ret = ioctl(fd, ULTRAFBIO_BLENDING_MODE, &mode);
    if (ret < 0)
    {
        printf("set blending mode error\n");
        return -1;
    }
    return 0;
}

int SetGlobalAlpha(int fd, dc_global_alpha global_alpha)
{
    int ret = 0;
    ret = ioctl(fd, ULTRAFBIO_GLOBAL_MODE_VALUE, &global_alpha);
    if (ret < 0)
    {
        printf("set global mode error\n");
        return -1;
    }
    return 0;
}

#if 0
float sinc_filter(float x, unsigned int radius)
{
    float pit, pitd, f1, f2, result;
    float f_radius = MATH_Int2Float(radius);

    if (x == 0.0f)
    {
        result = 1.0f;
    }
    else if ((x < -f_radius) || (x > f_radius))
    {
        result = 0.0f;
    }
    else
    {
        pit = MATH_Multiply(PI, x);
        pitd = MATH_Divide(pit, f_radius);

        f1 = MATH_Divide(MATH_Sine(pit), pit);
        f2 = MATH_Divide(MATH_Sine(pitd), pitd);

        result = MATH_Multiply(f1, f2);
    }

    return result;
}
#endif

void * os_mem_alloc(unsigned int size)
{
    if (size != 0)
        return malloc(size);

    return NULL;
}

void os_mem_free(void * addr)
{
    if (addr != NULL)
        free(addr);
}

#if 0
/* Calculate weight array for sync filter. */
unsigned int * calculate_sync_table(unsigned char kernel_size, unsigned int src_size, unsigned int dst_size)
{
    float f_scale;
    int kernel_half;
    float f_subpixel_step;
    float f_subpixel_offset;
    unsigned int subpixel_pos;
    int kernel_pos;
    int padding;
    unsigned short * kernel_array;
    void * pointer = NULL;
    unsigned int * tableaddr = NULL;

    do
    {
        /* Allocate the array if not allocated yet. */
        if (tableaddr == NULL)
        {
            /* Allocate the array. */
            pointer = os_mem_alloc(KERNELSTATES);
            tableaddr = (unsigned int *)pointer;
        }

        /* Compute the scale factor. */
        f_scale = MATH_DivideFromUInteger(dst_size, src_size);

        /* Adjust the factor for magnification. */
        if (f_scale > 1.0f)
        {
            f_scale = 1.0f;
        }

        /* Calculate the kernel half. */
        kernel_half = (int) (kernel_size >> 1);

        /* Calculate the subpixel step. */
        f_subpixel_step = MATH_Divide(1.0f, MATH_Int2Float(SUBPIXELCOUNT));

        /* Init the subpixel offset. */
        f_subpixel_offset = 0.5f;

        /* Determine kernel padding size. */
        padding = (MAXKERNELSIZE - kernel_size) / 2;

        /* Set initial kernel array pointer. */
        kernel_array = (unsigned short *) (tableaddr + 1);

        /* Loop through each subpixel. */
        for (subpixel_pos = 0; subpixel_pos < SUBPIXELLOADCOUNT; subpixel_pos++)
        {
            /* Define a temporary set of weights. */
            float fSubpixelSet[MAXKERNELSIZE];

            /* Init the sum of all weights for the current subpixel. */
            float fWeightSum = 0.0f;
            unsigned short weightSum = 0;
            signed short adjustCount, adjustFrom;
            signed short adjustment;

            /* Compute weights. */
            for (kernel_pos = 0; kernel_pos < MAXKERNELSIZE; kernel_pos++)
            {
                /* Determine the current index. */
                int index = kernel_pos - padding;

                /* Pad with zeros. */
                if ((index < 0) || (index >= kernel_size))
                {
                    fSubpixelSet[kernel_pos] = 0.0f;
                }
                else
                {
                    if (kernel_size == 1)
                    {
                        fSubpixelSet[kernel_pos] = 1.0f;
                    }
                    else
                    {
                        /* Compute the x position for filter function. */
                        float fX =
                            MATH_Multiply(
                                MATH_Add(
                                    MATH_Int2Float(index - kernel_half),
                                    f_subpixel_offset),
                                f_scale);

                        /* Compute the weight. */
                        fSubpixelSet[kernel_pos] = sinc_filter(fX, kernel_half);
                    }

                    /* Update the sum of weights. */
                    fWeightSum = MATH_Add(fWeightSum, fSubpixelSet[kernel_pos]);
                }
            }

            /* Adjust weights so that the sum will be 1.0. */
            for (kernel_pos = 0; kernel_pos < MAXKERNELSIZE; kernel_pos++)
            {
                /* Normalize the current weight. */
                float fWeight = MATH_Divide(fSubpixelSet[kernel_pos],
                                                  fWeightSum);

                /* Convert the weight to fixed point and store in the table. */
                if (fWeight == 0.0f)
                {
                    kernel_array[kernel_pos] = 0x0000;
                }
                else if (fWeight >= 1.0f)
                {
                    kernel_array[kernel_pos] = 0x4000;
                }
                else if (fWeight <= -1.0f)
                {
                    kernel_array[kernel_pos] = 0xC000;
                }
                else
                {
                    kernel_array[kernel_pos] = (unsigned short)MATH_Multiply(fWeight, 16384.0f);
                }

                weightSum += kernel_array[kernel_pos];
            }

            /* Adjust the fixed point coefficients. */
            adjustCount = 0x4000 - weightSum;
            if (adjustCount < 0)
            {
                adjustCount = -adjustCount;
                adjustment = -1;
            }
            else
            {
                adjustment = 1;
            }

            adjustFrom = (MAXKERNELSIZE - adjustCount) / 2;

            for (kernel_pos = 0; kernel_pos < adjustCount; kernel_pos++)
            {
                kernel_array[adjustFrom + kernel_pos] += adjustment;
            }

            kernel_array += MAXKERNELSIZE;

            /* Advance to the next subpixel. */
            f_subpixel_offset = MATH_Add(f_subpixel_offset, -f_subpixel_step);
        }

    }
    while (0);

    return tableaddr;
}

/* Prepare gamma table config. */
void calculate_gamma_table(dc_gamma_table *gammatable)
{
    int i;
    float tmpf;
    unsigned char temp = 0;
    unsigned int ttt;

    for(i = 0; i < GAMMA_INDEX_MAX; i++)
    {
        tmpf = (i + 0.5f)/256.0f;
        tmpf = (float)(pow((double)tmpf, (double)0.45));
        temp = (unsigned char)(tmpf/(1.0f/256.0f) - 0.5f);
        ttt = ((temp<<22) | (temp<<12) |(temp<<2));
        gammatable->gamma[i][0] = (ttt >> 20) & 0x3FF;
        gammatable->gamma[i][1] = (ttt >> 10) & 0x3FF;
        gammatable->gamma[i][2] = ttt & 0x3FF;
    }
}
#endif

