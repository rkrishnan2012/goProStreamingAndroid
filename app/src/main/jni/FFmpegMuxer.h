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
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"

#define VIDEO_INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

int outputWidth = 0, outputHeight = 0, outputVideoCodecID = -1;
AVFormatContext *outputFmtCtx = NULL;
AVOutputFormat *outputFmt = NULL;

AVCodec *outputVideoCodec = NULL;
AVCodecContext *videoEncCtx = NULL;

bool VERBOSE = false;

bool streamsAdded = false;
char *outputFileStr = NULL;
int64_t videoStartingPts = 0, audioStartingPts = 0;
int64_t videoStartingDts = 0, audioStartingDts = 0;
int videoStreamIdx = -1, audioStreamIdx = -1;

static int openCodecContext(int *streamIdx, AVFormatContext *fmtCtx, enum AVMediaType type);

void releaseOutputContext();

void configureEncoder(AVStream *audioStream, AVStream *videoStream);

void configureOutputContext(char *outputFile);

void writeInterleavedStream(AVStream *audioStream, AVStream *videoStream,
                            AVFormatContext *audioFmtCtx, AVFormatContext *videoFmtCtx,
                            bool encodingNeeded);

int stitchFile(const char *audioFileStr, const char *videoFileStr, bool onlyAnalyze);

#ifndef ANDROID
#define LOGE(...)  printf(__VA_ARGS__)
#define LOGI(...)  printf(__VA_ARGS__)
#else
#define LOG_TAG "FFmpegMuxer"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#endif /* FFMPEGMUXER_H */