#ifndef FFMPEG_RTMP_H
#define FFMPEG_RTMP_H

#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#endif

#include <time.h>
#include <stdbool.h>
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavfilter/avfilter.h"

typedef struct metadata_t {
    //  Video codec options
    int videoWidth;
    int videoHeight;
    int videoBitrate;
    //  Audio codec options
    int audioSampleRate;
    int numAudioChannels;
    //  Format options
    const char *outputFormatName;
    const char *outputFile;
} Metadata;


Metadata metadata;

enum AVPixelFormat VIDEO_PIX_FMT = AV_PIX_FMT_YUV420P;
enum AVCodecID VIDEO_CODEC_ID = AV_CODEC_ID_H264;
enum AVCodecID AUDIO_CODEC_ID = AV_CODEC_ID_AAC;
enum AVSampleFormat AUDIO_SAMPLE_FMT = AV_SAMPLE_FMT_S16;

// Create a timebase that will convert Android timestamps to FFmpeg encoder timestamps
AVRational androidSourceTimebase = (AVRational) {1, 1000};
AVRational flvDestTimebase = (AVRational) {1, 1000000};

//  Persistent variables we need to release at the end
AVFormatContext *outputFormatContext;
AVStream *audioStream = NULL, *videoStream = NULL;
AVPacket *packet = NULL;

int audioStreamIndex = -1;
int videoStreamIndex = -1;
int isConnectionOpen = 0;
int frameCount = 0;
bool foundKeyFrame = false;
int64_t lastVideoPts = 0;
int64_t lastAudioPts = 0;

char *get_error_string(int errorNum, char *errBuf);
void release_resources();

#ifndef ANDROID
#define LOGE(...)  printf(__VA_ARGS__)
#define LOGI(...)  printf(stderr, __VA_ARGS__)
#else
#define LOG_TAG "GoProStream"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#endif /* FFMPEGRTMP_H */