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
 * Wildife format converter
 */

#include <float.h>
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/timestamp.h"
#include "avfilter.h"
#include "internal.h"

typedef struct WildlifeFmtContext {
    const AVClass *class;

    AVRational time_base;
    int nb_threads;
} WildlifeFmtContext;

typedef struct WildlifeFmtFrames {
    AVFrame *src;
    AVFrame *dst;
} WildlifeFmtFrames;

#define OFFSET(x) offsetof(WildlifeFmtContext, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_FILTERING_PARAM

static const AVOption wildlifefmt_options[] = {
    { NULL }
};

AVFILTER_DEFINE_CLASS(wildlifefmt);

static int query_formats(AVFilterContext *ctx)
{
    static const enum AVPixelFormat in_fmts[] = { AV_PIX_FMT_YUYV422, AV_PIX_FMT_NONE };
    static const enum AVPixelFormat out_fmts[] = { AV_PIX_FMT_NV12, AV_PIX_FMT_NONE };
    int ret;

    if ((ret = ff_formats_ref(ff_make_format_list(in_fmts), &ctx->inputs[0]->outcfg.formats)) < 0)
        return ret;
    if ((ret = ff_formats_ref(ff_make_format_list(out_fmts), &ctx->outputs[0]->incfg.formats)) < 0)
        return ret;

    return 0;
}

static int config_input(AVFilterLink *inlink)
{
    AVFilterContext *ctx = inlink->dst;
    WildlifeFmtContext *s = ctx->priv;

    s->time_base = inlink->time_base;
    s->nb_threads = ff_filter_get_nb_threads(ctx);

    return 0;
}

static int format_converter(AVFilterContext *ctx, void *arg,
                            int jobnr, int nb_jobs)
{
    WildlifeFmtFrames *frames = arg;

    const int src_linesize = frames->src->linesize[0];
    const int dst_linesize = frames->dst->linesize[0];

    const int width  = frames->src->width;
    const int height = frames->src->height;

    const int start = (height *  jobnr     ) / nb_jobs;
    const int end   = (height * (jobnr + 1)) / nb_jobs;

    uint8_t *p = frames->src->data[0] + start * src_linesize;
    uint8_t *q = frames->dst->data[0] + start * dst_linesize;

    for (int i = 0; i < end - start; i++) {
        for (int x = 0; x < width; x++) {
            q[x] = p[x * 2];
        }

        p += src_linesize;
        q += dst_linesize;
    }

    memset(frames->dst->data[1] + (start / 2) * frames->dst->linesize[1],
            128, ((end - start) / 2) * frames->dst->linesize[1]);

    return 0;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *src_frame)
{
    AVFilterContext *ctx = inlink->dst;
    WildlifeFmtContext *s = ctx->priv;
    WildlifeFmtFrames frames = {
        .src = src_frame,
        .dst = NULL,
    };

    int ret;

    frames.dst = av_frame_alloc();
    if (!frames.dst)
        return AVERROR(ENOMEM);

    frames.dst->format = AV_PIX_FMT_NV12;
    frames.dst->width  = src_frame->width;
    frames.dst->height = src_frame->height;

    ret = av_frame_get_buffer(frames.dst, 0);
    if (ret < 0)
        goto fail;

    ret = av_frame_copy_props(frames.dst, frames.src);
    if (ret < 0)
        goto fail;

    ctx->internal->execute(ctx, format_converter, &frames, NULL,
                           FFMIN(inlink->h, s->nb_threads));

    av_frame_free(&frames.src);
    return ff_filter_frame(ctx->outputs[0], frames.dst);

fail:
    av_frame_free(&frames.src);
    av_frame_free(&frames.dst);

    return ret;
}

static const AVFilterPad wildlifefmt_inputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
        .config_props  = config_input,
        .filter_frame  = filter_frame,
    },
    { NULL }
};

static const AVFilterPad wildlifefmt_outputs[] = {
    {
        .name          = "default",
        .type          = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

AVFilter ff_vf_wildlifefmt = {
    .name          = "wildlifefmt",
    .description   = NULL_IF_CONFIG_SMALL("Wildlife specific pixel format converter"),
    .priv_size     = sizeof(WildlifeFmtContext),
    .query_formats = query_formats,
    .inputs        = wildlifefmt_inputs,
    .outputs       = wildlifefmt_outputs,
    .priv_class    = &wildlifefmt_class,
    .flags         = AVFILTER_FLAG_SLICE_THREADS,
};
