#ifndef AVC_VISION
#define AVC_VISION

#include "structs.h"
#include "sys.h"

int write_png_file_rgb(
    const char* path,
    int width,
    int height,
    const char* buffer);

void yuv422_to_rgb(uint8_t* luma, chroma_t* uv, color_t* rgb, int w, int h);

void image_patch_f(float* dst, color_t* rgb, rectangle_t rect);
void image_patch_b(color_t* dst, color_t* rgb, rectangle_t rect);

#endif
