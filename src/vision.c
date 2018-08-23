#include "vision.h"
#include <png.h>

int write_png_file_rgb(
    const char* path,
    int width,
    int height,
    const char* buffer){

    FILE *fp = fopen(path, "wb");

    if(!fp)
    {
        b_bad("Couldn't open %s for writing", path);
        return -1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) return -2;

    png_infop info = png_create_info_struct(png);
    if (!info) return -3;

    if (setjmp(png_jmpbuf(png))) return -4;

    png_init_io(png, fp);

    // Output is 8bit depth, RGB format.
    png_set_IHDR(
        png,
        info,
        width, height,
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    png_bytep rows[height];
    for(int i = height; i--;)
    {
        rows[i] = (png_bytep)(buffer + i * (width * 3));
    }

    png_write_image(png, rows);
    png_write_end(png, NULL);

    fclose(fp);

    return 0;
}


void image_patch_f(float* dst, color_t* rgb, rectangle_t rect)
{
    // slice out patches to use for activation
    for (int kr = rect.h; kr--;)
    for (int kc = rect.w; kc--;)
    {
        color_t color = rgb[((rect.y + kr) * FRAME_W) + rect.x + kc];
        dst[(kr * 48) + kc * 3 + 0] = (color.r / 255.0f) - 0.5f;
        dst[(kr * 48) + kc * 3 + 1] = (color.g / 255.0f) - 0.5f;
        dst[(kr * 48) + kc * 3 + 2] = (color.b / 255.0f) - 0.5f;
    }
}


void image_patch_b(color_t* dst, color_t* rgb, rectangle_t rect)
{
    // slice out patches to use for activation
    for (int kr = rect.h; kr--;)
    for (int kc = rect.w; kc--;)
    {
        color_t color = rgb[((rect.y + kr) * FRAME_W) + rect.x + kc];
        dst[(kr * rect.w) + kc] = color;
    }
}
