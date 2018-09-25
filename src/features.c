#include <stdio.h>
#include <stdarg.h>
#include "sys.h"
#include "structs.h"
#include "pid.h"
#include "cfg.h"

#define SLAM_FEATURES 256
#define SLAM_BUCKET_ROWS 10
#define SLAM_BUCKET_COLS 10
#define KERN 2


typedef struct {
    unsigned int x, y;
    chroma_t id_color;
    float strength;
    int life;
} vis_feature_t;


typedef struct {
    uint16_t detected;
    vis_feature_t prints[SLAM_FEATURES];
    uint8_t buckets[SLAM_BUCKET_ROWS][SLAM_BUCKET_COLS];
} msg_features_t;


void sig_handler(int sig)
{
    b_log("Caught signal %d", sig);
    exit(0);
}


vis_feature_t feature(color_t* patch, int w, int h)
{
    vis_feature_t f = {
        .id_color = { { rand() % 255, rand() % 255 } },
    };

    size_t pixels = w * h;
    int mean[3] = {};
    for (int i = pixels; i--;)
    {
        mean[0] += patch[i].r;
        mean[1] += patch[i].g;
        mean[2] += patch[i].b;
    }

    mean[0] /= pixels;
    mean[1] /= pixels;
    mean[2] /= pixels;

    int var = 0;
    for (int i = pixels; i--;)
    {
        var += pow(patch[i].r - mean[0], 2);
        var += pow(patch[i].g - mean[1], 2);
        var += pow(patch[i].b - mean[2], 2);
    }

    f.strength = sqrt(var / pixels);

    return f;
}


msg_features_t feats = {};

void find_features(raw_state_t* raw)
{
    color_t rgb[FRAME_W * FRAME_H];

    yuv422_to_rgb(raw->view.luma, raw->view.chroma, rgb, FRAME_W, FRAME_H);

    // color black as a visual
    for (int r = 0; r < FRAME_H; r += 1)
    for (int c = 0; c < FRAME_W; c += 1)
    {
        raw->view.luma[r * FRAME_W + c] = 0;
    }

    memset(feats.buckets, 0, sizeof(feats.buckets));

    // match existing features
    feats.detected = 0;

    // find new features
    while (feats.detected < SLAM_FEATURES)
    {
        int r = (KERN >> 1) + rand() % (FRAME_H - (KERN - 1));
        int c = (KERN >> 1) + rand() % (FRAME_W - (KERN - 1));

        color_t patch[9];
        rectangle_t dims = { c - (KERN >> 1), r - (KERN >> 1), KERN, KERN };
        image_patch_b(patch, rgb, dims);

        vis_feature_t f = feature(patch, dims.w, dims.h);
        f.x = dims.x;
        f.y = dims.y;
        f.id_color.cb = f.x * (255.f / (float)FRAME_W);
        f.id_color.cr = f.y * (255.f / (float)FRAME_H);

        if (f.strength > 64)
        {
            feats.prints[feats.detected] = f;
            feats.detected++;
        }
    }

    int d_col = FRAME_W / SLAM_BUCKET_COLS;
    int d_row = FRAME_H / SLAM_BUCKET_ROWS;
    for (int i = feats.detected; i--;)
    {
        vis_feature_t f = feats.prints[i];

        {
            raw->view.luma[f.y * FRAME_W + f.x] = 255;
            raw->view.chroma[f.y * (FRAME_W >> 1) + (f.x >> 1)] = f.id_color;
        }

        feats.buckets[f.y / d_row][f.x / d_col]++;
    }

    for (int r = 0; r < FRAME_H; r += 1)
    for (int c = 0; c < FRAME_W; c += 1)
    {
        int count = feats.buckets[r / d_row][c / d_col];

        if(count <= 0) continue;

        raw->view.luma[r * FRAME_W + c] = count;
        raw->view.chroma[r * (FRAME_W >> 1) + (c >> 1)].cb = c * (255.f / (float)FRAME_W);
        raw->view.chroma[r * (FRAME_W >> 1) + (c >> 1)].cr = r * (255.f / (float)FRAME_H);
    }
}


int main(int argc, char* const argv[])
{
    PROC_NAME = argv[0];

    signal(SIGINT, sig_handler);
    cfg_base("/etc/bot/features/");

    // define and process cli args
    cli_cmd_t cmds[] = {
        CLI_CMD_LOG_VERBOSITY,
        {} // terminator
    };
    cli("Detects visual features in the camera frame", cmds, argc, argv);

    message_t msg = {};

    while (1)
    {
        if (read_pipeline_payload(&msg, PAYLOAD_STATE))
        {
            b_bad("read error");
            sig_handler(2);
            return -1;
        }

        find_features(&msg.payload.state);

        if (write_pipeline_payload(&msg))
        {
            b_bad("Failed to write payload");
            return -1;
        }
    }

    return 0;
}
