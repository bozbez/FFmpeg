/*
 * Copyright (c) 2021 Joe Kaushal
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Wildife detector
 */

#include <float.h>
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/timestamp.h"
#include "avfilter.h"
#include "internal.h"

typedef struct WildlifeDetectContext {
    const AVClass *class;

    AVRational time_base;
    int nb_threads;

    AVFrame *prev_frame;

    int energy_threshold;
    int noise_threshold;

    int min_frames;
    int rem_frames;

    int *th_energy;
} WildlifeDetectContext;

#define OFFSET(x) offsetof(WildlifeDetectContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption wildlifedetect_options[] = {
    { "energy_threshold", "set the wildlife energy threshold",
       OFFSET(energy_threshold),
       AV_OPT_TYPE_INT, { .i64 = 10000 }, 0, INT32_MAX, FLAGS },

    { "noise_threshold", "set the per-pixel sum-abs-diff threshold",
       OFFSET(noise_threshold),
       AV_OPT_TYPE_INT, { .i64 = 15 }, 0, UINT8_MAX, FLAGS },

    { "min_frames", "set the minimum number of recorded frames per detection",
       OFFSET(min_frames),
       AV_OPT_TYPE_INT, { .i64 = 5 }, 0, INT32_MAX, FLAGS },

    { NULL }
};

AVFILTER_DEFINE_CLASS(wildlifedetect);

static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat pix_fmts[] = {
        AV_PIX_FMT_GRAY8,

        AV_PIX_FMT_NV12, AV_PIX_FMT_NV21,

        AV_PIX_FMT_YUV410P, AV_PIX_FMT_YUV411P, AV_PIX_FMT_YUV420P,
        AV_PIX_FMT_YUV422P, AV_PIX_FMT_YUV440P, AV_PIX_FMT_YUV444P,

        AV_PIX_FMT_YUVJ411P, AV_PIX_FMT_YUVJ420P, AV_PIX_FMT_YUVJ422P,
        AV_PIX_FMT_YUVJ444P, AV_PIX_FMT_YUVJ440P,

        AV_PIX_FMT_YUVA420P, AV_PIX_FMT_YUVA422P, AV_PIX_FMT_YUVA444P,

        AV_PIX_FMT_NONE
    };

    AVFilterFormats *fmts_list = ff_make_format_list(pix_fmts);

    if (!fmts_list)
        return AVERROR(ENOMEM);

    return ff_set_common_formats(ctx, fmts_list);
}

static int config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    WildlifeDetectContext *s = ctx->priv;

    s->time_base = inlink->time_base;
    s->nb_threads = ff_filter_get_nb_threads(ctx);

    s->rem_frames = -1;

    s->th_energy = av_calloc(s->nb_threads, sizeof(*s->th_energy));
    if (!s->th_energy)
        return AVERROR(ENOMEM);

    av_log(s, AV_LOG_VERBOSE, "energy_threshold:%d noise_threshold:%d\n",
           s->energy_threshold, s->noise_threshold);

    return 0;
}

static int energy_counter(AVFilterContext *ctx, void *arg,
                          int jobnr, int nb_jobs)
{
    WildlifeDetectContext *s = ctx->priv;

    AVFrame *cur_frame = arg;
    AVFrame *prev_frame = s->prev_frame;

    const int linesize = cur_frame->linesize[0];

    const int width = cur_frame->width;
    const int height = cur_frame->height;

    const int start = (height *  jobnr     ) / nb_jobs;
    const int end   = (height * (jobnr + 1)) / nb_jobs;

    const int noise_threshold = s->noise_threshold;

    int energy = 0;

    const uint8_t *p = cur_frame->data[0] + start * linesize;
    const uint8_t *q = prev_frame->data[0] + start * linesize;

    for (int i = 0; i < end - start; i++) {
        for (int x = 0; x < width; x++) {
            int val = abs(p[x] - q[x]);
            if (val >= noise_threshold)
                energy += val;
        }

        p += linesize;
        q += linesize;
    }

    s->th_energy[jobnr] = energy;
    return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *cur_frame)
{
    AVFilterContext *ctx = inlink->dst;
    WildlifeDetectContext *s = ctx->priv;

    int total_energy = 0;
    AVDictionary **meta = &cur_frame->metadata;

    if (!s->prev_frame) {
        s->prev_frame = av_frame_clone(cur_frame);
        av_frame_free(&cur_frame);

        return 0;
    }

    ctx->internal->execute(ctx, energy_counter, cur_frame, NULL,
                           FFMIN(inlink->h, s->nb_threads));

    for (int i = 0; i < s->nb_threads; i++)
        total_energy += s->th_energy[i];

    av_dict_set_int(meta, "lavfi.wildlifedetect.energy", total_energy, 0);
    av_log(ctx, AV_LOG_DEBUG, "frame:%"PRId64" energy:%d pts:%s t:%s type:%c\n",
           inlink->frame_count_out, total_energy, av_ts2str(cur_frame->pts),
           av_ts2timestr(cur_frame->pts, &s->time_base),
           av_get_picture_type_char(cur_frame->pict_type));

    if (total_energy >= s->energy_threshold) {
        s->rem_frames = s->min_frames;
        av_dict_set(meta, "lavfi.wildlifedetect.start",
                    av_ts2timestr(cur_frame->pts, &s->time_base), 0);
    }

    av_frame_free(&s->prev_frame);
    s->prev_frame = av_frame_clone(cur_frame);

    if (s->rem_frames > 0) {
        s->rem_frames--;
        return ff_filter_frame(ctx->outputs[0], cur_frame);
    } else if (s->rem_frames == 0) {
        s->rem_frames--;
        av_dict_set(meta, "lavfi.wildlifedetect.end",
                    av_ts2timestr(cur_frame->pts, &s->time_base), 0);
    }

    av_frame_free(&cur_frame);
    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    WildlifeDetectContext *s = ctx->priv;

    av_freep(&s->th_energy);
    av_frame_free(&s->prev_frame);
}

static const AVFilterPad wildlifedetect_inputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_input,
        .filter_frame  = filter_frame,
    },
    { NULL }
};

static const AVFilterPad wildlifedetect_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

AVFilter ff_vf_wildlifedetect = {
    .name          = "wildlifedetect",
    .description   = NULL_IF_CONFIG_SMALL("Detect and extract frames likely to contain wildlife"),
    .priv_size     = sizeof(WildlifeDetectContext),
    .query_formats = query_formats,
    .inputs        = wildlifedetect_inputs,
    .outputs       = wildlifedetect_outputs,
    .uninit        = uninit,
    .priv_class    = &wildlifedetect_class,
    .flags         = AVFILTER_FLAG_SLICE_THREADS,
};
