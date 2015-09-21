#ifndef FFMPEGMUXER_H
#define FFMPEGMUXER_H

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#endif

#include <stdbool.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <android/log.h>
#include <pthread.h>
#include <string.h>
#include "libswscale/swscale.h"
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

static pthread_cond_t s_vsync_cond;
static pthread_mutex_t s_vsync_mutex;
int surfaceWidth = 0;
int surfaceHeight = 0;
static GLuint s_texture = 0;
int got_frame = 0;
static AVFrame *decodedVideoFrame = NULL;

static uint8_t *s_pixels = 0;
#define RGB565(r, g, b)  (((r) << (5+6)) | ((g) << 6) | (b))

/* disable these capabilities. */
static GLuint s_disable_caps[] = {
        GL_FOG,
        GL_LIGHTING,
        GL_CULL_FACE,
        GL_ALPHA_TEST,
        GL_BLEND,
        GL_COLOR_LOGIC_OP,
        GL_DITHER,
        GL_STENCIL_TEST,
        GL_DEPTH_TEST,
        GL_COLOR_MATERIAL,
        0
};

#ifndef ANDROID
#define LOGE(...)  printf(__VA_ARGS__)
#define LOGI(...)  printf(__VA_ARGS__)
#else
#define LOG_TAG "GoProAcquire"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#endif /* FFMPEGMUXER_H */