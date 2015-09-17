#ifndef FFMPEGMUXER_H
#define FFMPEGMUXER_H

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#endif

#include <stdbool.h>
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavfilter/avfilter.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"

AVFormatContext *outputFmtCtx = NULL;
AVOutputFormat *outputFmt = NULL;

int videoStreamIdx = -1, audioStreamIdx = -1;

static int openCodecContext(int *streamIdx, AVFormatContext *fmtCtx, enum AVMediaType type);

void releaseOutputContext();

#ifndef ANDROID
#define LOGE(...)  printf(__VA_ARGS__)
#define LOGI(...)  printf(__VA_ARGS__)
#else
#define LOG_TAG "GoProAcquire"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#endif /* FFMPEGMUXER_H */