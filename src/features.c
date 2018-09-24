#include <stdio.h>
#include <stdarg.h>
#include "sys.h"
#include "structs.h"
#include "pid.h"
#include "cfg.h"

#define SLAM_FEATURES 128
#define KERN 2


typedef struct {
    color_t id[KERN * KERN];
    unsigned int x, y;
    chroma_t id_color;
    float strength;
    int life;
} vis_feature_t;


typedef struct {
    uint16_t detected;
    vis_feature_t prints[SLAM_FEATURES];
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

        f.id[i] = patch[i];
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

float feature_cmp(vis_feature_t* c0, vis_feature_t* c1)
{
    int dist = 0;

    for (int i = KERN * KERN; i--;)
    {
        int dr = c0->id[i].r - c1->id[i].r;
        int dg = c0->id[i].g - c1->id[i].g;
        int db = c0->id[i].b - c1->id[i].b;
        dist += sqrt(dr * dr + dg * dg + db * db);
    }

    dist /= (KERN * KERN);

    return dist;
}

msg_features_t feats = {};

void match_existing(color_t* rgb)
{
    for (int i = 0; i < feats.detected; ++i)
    {
        const int win = KERN * 2;
        const int win_off = (win - KERN) / 2;
        vis_feature_t* fi = feats.prints + i;


        int r_min = CLAMP(fi->y - (win_off * 10), win_off, FRAME_H - win_off);
        int r_max = CLAMP(fi->y + KERN + (win_off * 10) , win_off, FRAME_H - win_off);
        int c_min = CLAMP(fi->x - win_off, win_off, FRAME_W - win_off);
        int c_max = CLAMP(fi->x + KERN, win_off, FRAME_W - win_off);

        float best_score = 64;
        vis_feature_t best_f;
        for (int r = r_min; r < r_max; r += 1)
        for (int c = c_min; c < c_max; c += 1)
        {
            color_t patch[KERN * KERN];
            rectangle_t dims = { c, r, KERN, KERN };
            image_patch_b(patch, rgb, dims);
            vis_feature_t new_f = feature(patch, dims.w, dims.h);
            new_f.x = dims.x;
            new_f.y = dims.y;

            if (new_f.strength <= 32) { continue; }

            // match is close enough, update
            float score = feature_cmp(fi, &new_f);
            if (score < 32.f)
            {
                best_score = score;
                best_f = new_f;
                // best_f.life = fi->life + 1;
                // goto found;
            }
        }

        if (best_score < 32)
        {
            fi->x = best_f.x;
            fi->y = best_f.y;
            fi->id_color.cb = fi->x * (255.f / (float)FRAME_W);
            fi->id_color.cr = fi->y * (255.f / (float)FRAME_H);
            fi->life++;
        }
        else
        {
            // missing, remove feature i
            feats.detected--;
            feats.prints[i] = feats.prints[feats.detected];
            i--;
        }
    }
}

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

    // match existing features
    feats.detected = 0;

    b_log("%d persisted", feats.detected);

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

    for (int i = feats.detected; i--;)
    {
        vis_feature_t f = feats.prints[i];
        // if (f.life > 10)
        {
            raw->view.luma[f.y * FRAME_W + f.x] = 255;
            raw->view.chroma[f.y * (FRAME_W >> 1) + (f.x >> 1)] = f.id_color;
        }
    }


    b_log("%d features", feats.detected);
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
