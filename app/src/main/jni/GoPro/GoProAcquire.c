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

static void wait_vsync()
{
    LOGE("waiting vsync");
    pthread_mutex_lock(&s_vsync_mutex);
    pthread_cond_wait(&s_vsync_cond, &s_vsync_mutex);
    pthread_mutex_unlock(&s_vsync_mutex);
    LOGE("Waited vsync");
}

static void check_gl_error(const char* op)
{
    GLint error;
    for (error = glGetError(); error; error = glGetError())
        LOGE("after %s() glError (0x%x)\n", op, error);
}


void renderPreview()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glTexImage2D(GL_TEXTURE_2D,		/* target */
                 0,			/* level */
                 GL_RGB,			/* internal format */
                 432,		/* width */
                 240,		/* height */
                 0,			/* border */
                 GL_RGB,			/* format */
                 GL_UNSIGNED_BYTE,/* type */
                 s_pixels);		/* pixels */
    glDrawTexiOES(0, 0, 0, surfaceWidth, surfaceHeight);
    /* tell the other thread to carry on */
    pthread_cond_signal(&s_vsync_cond);
    LOGE("Done");
}

#ifdef ANDROID
JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_GoProC_startReading(JNIEnv *env,
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

    //  Initialize the OpenGL lock
    pthread_cond_init(&s_vsync_cond, NULL);
    pthread_mutex_init(&s_vsync_mutex, NULL);

    //  Information about decoding the video frame
    AVCodecContext *decCtx = videoStream->codec;
    decodedVideoFrame = av_frame_alloc();


    struct SwsContext* sws_ctx = sws_getContext(decCtx->width, decCtx->height,
        AV_PIX_FMT_YUV420P, 432, 240, AV_PIX_FMT_RGB24,
        SWS_BICUBIC, NULL, NULL, NULL );

    //
    int got_frame;

    AVFrame* pFrameRGB=av_frame_alloc();
    int numBytes=avpicture_get_size(AV_PIX_FMT_RGB24, 432, 240);
    uint8_t* buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    avpicture_fill((AVPicture*)pFrameRGB, buffer, AV_PIX_FMT_RGB24,432, 240);

    while (!avDone) {
        ret = av_read_frame(avFmtCtx, &avPacket);
        if (ret < 0) {
            avDone = true;
        }
        else {
            //  If its an audio frame, we need to do a bit-stream filter before we send it over
            //  to the rtmp stream.
            if(avPacket.stream_index == audioStreamIdx){
                ret = av_bitstream_filter_filter(audioFilter, audioDecCtx, NULL,
                    &avPacket.data, &avPacket.size,avPacket.data,
                    avPacket.size,avPacket.flags & AV_PKT_FLAG_KEY);
            }
            else{
                ret = avcodec_decode_video2(decCtx, decodedVideoFrame, &got_frame, &avPacket);
                if (ret < 0) {
                    LOGE("Unable to decode the video frame.");
                }
                else if (got_frame) {
                    sws_scale(sws_ctx, (const uint8_t * const*)decodedVideoFrame->data,
                        decodedVideoFrame->linesize, 0, 240, pFrameRGB->data,
                        pFrameRGB->linesize);
                    s_pixels = pFrameRGB->data[0];
                    wait_vsync();
                }
            }
            if (connectionCallback) {
                (*env)->CallVoidMethod(env, instance, connectionCallback,
                (int)(&avPacket), (int)(audioStream), (int)(videoStream));
            }
        }
        av_free_packet(&avPacket);
     }
     LOGE("Done with the input stream.");
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_GoProC_surfaceResize(JNIEnv *env, jclass clazz, jint w, jint h)
{
    LOGI("native_gl_resize %d %d", w, h);
	glDeleteTextures(1, &s_texture);
	GLuint *start = s_disable_caps;
	while (*start)
		glDisable(*start++);
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &s_texture);
	glBindTexture(GL_TEXTURE_2D, s_texture);
	glTexParameterf(GL_TEXTURE_2D,
			GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D,
			GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glShadeModel(GL_FLAT);
	check_gl_error("glShadeModel");
	glColor4x(0x10000, 0x10000, 0x10000, 0x10000);
	check_gl_error("glColor4x");
	int rect[4] = {0, 240, 432, -240};
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, rect);
	check_gl_error("glTexParameteriv");
	surfaceWidth = w;
	surfaceHeight = h;
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_GoProC_surfaceDraw(JNIEnv *env, jclass clazz, jint w, jint h)
{
    // Render the preview to the surface
    renderPreview();
}
#endif