#include "GoProAcquire.h"

/**
 * Finds a specific stream from the given AVFormatContext of the given type.
 * Changes *streamIdx to be the index of the found stream.
 * Returns - 0 if successful.
 */
static int openCodecContext(int *streamIdx, AVFormatContext *fmtCtx, enum AVMediaType type) {
    int ret;
    AVStream *st;
    AVCodec *dec = NULL;
    AVCodecContext *decCtx = NULL;
    ret = av_find_best_stream(fmtCtx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        LOGE("Could not find %s stream. Return num is %d\n", av_get_media_type_string(type),
             ret);
        return ret;
    } else {
        *streamIdx = ret;
        st = fmtCtx->streams[*streamIdx];
        //  Find a decoder for the stream.
        decCtx = st->codec;
        dec = avcodec_find_decoder(decCtx->codec_id);
        if (!dec) {
            LOGE("Failed to find %s codec %s.\n", av_get_media_type_string(type),
                avcodec_get_name(decCtx->codec_id));
            return ret;
        }
        if ((ret = avcodec_open2(decCtx, dec, NULL)) < 0) {
            LOGE("Failed to open %s codec %s.\n", av_get_media_type_string(type),
                 avcodec_get_name(decCtx->codec_id));
            return ret;
        }
    }
    return 0;
}

void releaseOutputContext() {
    LOGI("Releasing the output context.\n");
    if (outputFmtCtx && !(outputFmt->flags & AVFMT_NOFILE))
        avio_closep(&outputFmtCtx->pb);
    avformat_free_context(outputFmtCtx);
}

#ifdef ANDROID
JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_GoProWrapper_startReading(JNIEnv *env,
                                                              jobject  __unused instance) {
    av_register_all();
    avformat_network_init();
    avcodec_register_all();

    AVFormatContext *avFmtCtx = NULL;
    AVStream *videoStream = NULL, *audioStream = NULL;
    AVCodecContext *videoDecCtx = NULL, *audioDecCtx = NULL;
    AVInputFormat *videoFormat = NULL;
    AVBitStreamFilterContext * audioFilter = NULL;
    int videoStreamIdx = -1, audioStreamIdx = -1;

    bool avDone = 0;
    int ret = 0;
    AVPacket avPacket;
    AVPacket filteredAudioPacket;
    int error = 0;

    // Open input files, and allocate format context
    char* inputFormat ="mpegts";

    if(!(videoFormat = av_find_input_format(inputFormat))){
        LOGE("Unknown input format %s", inputFormat);
        releaseOutputContext();
    }

    AVDictionary *options_dict = NULL;
    av_dict_set(&options_dict, "probesize", "16384", 0);
    if (avformat_open_input(&avFmtCtx, "udp://10.5.5.9:8554",
            videoFormat, &options_dict) < 0) {
        LOGE("Could not open video file %s\n", "udp://10.5.5.9:8554");
        releaseOutputContext();
    }

    avFmtCtx->iformat = videoFormat;

    // Retrieve stream information
    if (avformat_find_stream_info(avFmtCtx, NULL) < 0) {
        LOGE("Could not find video stream information.\n");
        releaseOutputContext();
    }

    if (openCodecContext(&videoStreamIdx, avFmtCtx, AVMEDIA_TYPE_VIDEO) >= 0) {
        videoStream = avFmtCtx->streams[videoStreamIdx];
        videoDecCtx = videoStream->codec;
    }
    else {
        LOGE("Couldn't find a video stream in file %s.", "udp://10.5.5.9:8554");
        releaseOutputContext();
    }

    if (openCodecContext(&audioStreamIdx, avFmtCtx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audioStream = avFmtCtx->streams[audioStreamIdx];
        audioDecCtx = audioStream->codec;
    }
    else {
        LOGE("Couldn't find a audio stream in file %s.", "udp://10.5.5.9:8554");
        releaseOutputContext();
    }

    if (!(audioFilter = av_bitstream_filter_init("aac_adtstoasc"))) {
        LOGE("Couldn't initialize the bitstream filter aac_adtstoasc");
        releaseOutputContext();
    }

    av_dump_format(avFmtCtx, 0, "udp://10.5.5.9:8554", 0);

    jclass thisClass = (*env)->GetObjectClass(env, instance);
    jmethodID connectionCallback = (*env)->GetMethodID(env, thisClass, "onReceiveGoProFrame", "(III)V");

    while (!avDone) {
        ret = av_read_frame(avFmtCtx, &avPacket);
        if (ret < 0) {
            avDone = true;
        }
        else {
            if(avPacket.stream_index == audioStreamIdx){
                ret = av_bitstream_filter_filter(audioFilter, audioDecCtx, NULL,
                    &avPacket.data, &avPacket.size,avPacket.data,
                    avPacket.size,avPacket.flags & AV_PKT_FLAG_KEY);
            }
            if (connectionCallback) {
                (*env)->CallVoidMethod(env, instance, connectionCallback,
                (int)(&avPacket), (int)(audioStream), (int)(videoStream));
            }
        }
     }
     LOGE("Done with the input stream.");
}
#endif