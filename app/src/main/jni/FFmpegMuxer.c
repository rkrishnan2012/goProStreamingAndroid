#include "FFmpegMuxer.h"

static int openCodecContext(int *streamIdx, AVFormatContext *fmtCtx, enum AVMediaType type) {
    int ret;
    AVStream *st;
    AVCodec *dec = NULL;
    AVCodecContext *decCtx = NULL;
    ret = av_find_best_stream(fmtCtx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        LOGE("Could not find %s stream.\n", av_get_media_type_string(type));
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

void configureEncoder(AVStream *audioStream, AVStream *videoStream) {
    outputVideoCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!outputVideoCodec) {
        LOGE("Video encoder codec not found.\n");
        releaseOutputContext();
    }
    videoEncCtx = avcodec_alloc_context3(outputVideoCodec);
    if (!videoEncCtx) {
        LOGE("Could not allocate video codec context.\n");
        releaseOutputContext();
    }
    /* put sample parameters */
    videoEncCtx->bit_rate = videoStream->codec->bit_rate;
    double ratio = videoStream->codec->width / videoStream->codec->height;
    int newWidth = videoStream->codec->width, newHeight = videoStream->codec->height;
    /*if(ratio == 640.0/360.0){
        newWidth = 640;
        newHeight = 360;
    }
    else if(ratio == (640.0/480.0)){
        newWidth = 640;
        newHeight = 480;
    }
    else if(ratio == (360.0/640.0)){
        newWidth = 360;
        newHeight = 640;
    }
    else{
        newWidth = 480;
        newHeight = 640;
    }*/
    videoEncCtx->width = newWidth;
    videoEncCtx->height = newHeight;
    LOGI("Encoder dimensions are %dx%d.\n", newWidth, newHeight);
    videoEncCtx->time_base = (AVRational) {1, 30};
    videoEncCtx->gop_size = videoStream->codec->gop_size; /* emit one intra frame every ten frames */
    videoEncCtx->max_b_frames = videoStream->codec->max_b_frames;
    videoEncCtx->pix_fmt = videoStream->codec->pix_fmt;

    if (avcodec_open2(videoEncCtx, outputVideoCodec, NULL) < 0) {
        LOGE("Could not open video encoder codec.\n");
        releaseOutputContext();
    }
}

void configureOutputContext(char *outputFile) {
    LOGI("Configuring the output context.\n");
    outputFileStr = outputFile;
    int ret;
    //  Allocate the output context.
    avformat_alloc_output_context2(&outputFmtCtx, NULL, NULL, outputFileStr);
    if (!outputFmtCtx) {
        LOGE("Could not create output context.\n");
        releaseOutputContext();
    }
    outputFmt = outputFmtCtx->oformat;

    //  Open the file for writing if necessary.
    if (!(outputFmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outputFmtCtx->pb, outputFileStr, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("Could not open output file '%s'.", outputFileStr);
            releaseOutputContext();
        }
    }
}

void writeInterleavedStream(AVStream *audioStream, AVStream *videoStream,
                            AVFormatContext *audioFmtCtx, AVFormatContext *videoFmtCtx,
                            bool encodingNeeded) {
    AVPacket videoPkt, audioPkt;
    AVFrame *frame = NULL;
    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&videoPkt);

    av_init_packet(&audioPkt);

    int ret;
    //  For the first audio and video streams, the output file will inherit this stream.
    if (!streamsAdded) {
        //  Create new streams for the audio file.
        AVStream *outputAudioStream = NULL, *outputVideoStream = NULL;
        configureEncoder(audioStream, videoStream);
        if (audioStream != NULL) {
            LOGI("Adding audio stream.\n");
            outputAudioStream = avformat_new_stream(outputFmtCtx, audioStream->codec->codec);
            audioStreamIdx = 0;
            //  Copy the input stream context to output file.
            ret = avcodec_copy_context(outputAudioStream->codec, audioStream->codec);
            if (ret < 0) {
                LOGE("Failed to copy context from input to output audio stream codec context\n");
                releaseOutputContext();
            }
            //  Set some codec properties
            outputAudioStream->codec->codec_tag = 0;
            outputAudioStream->time_base = outputAudioStream->codec->time_base;
            if (outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                outputAudioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }
        }

        if (videoStream != NULL) {
            LOGI("Adding video stream.\n");
            outputVideoStream = avformat_new_stream(outputFmtCtx, outputVideoCodec);
            videoStreamIdx = audioStreamIdx + 1;
            if (!encodingNeeded) {
                //  Copy the input stream context to output file.aww
                ret = avcodec_copy_context(outputVideoStream->codec, videoStream->codec);
                if (ret < 0) {
                    LOGE("Failed to copy context from input to output audio stream codec context\n");
                    releaseOutputContext();
                }
            }
            else {
                double ratio = videoStream->codec->width / videoStream->codec->height;
                int newWidth, newHeight;
                if (ratio == 640.0 / 360.0) {
                    newWidth = 640;
                    newHeight = 360;
                }
                else if (ratio == (640.0 / 480.0)) {
                    newWidth = 640;
                    newHeight = 480;
                }
                else if (ratio == (360.0 / 640.0)) {
                    newWidth = 360;
                    newHeight = 640;
                }
                else {
                    newWidth = 480;
                    newHeight = 640;
                }
                LOGI("New dimensions are %dx%d.\n", newWidth, newHeight);
                outputVideoStream->codec->width = newWidth;
                outputVideoStream->codec->height = newHeight;
            }
            outputVideoStream->codec->codec_tag = 0;
            if (outputFmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
                outputVideoStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }
        }

        ret = avformat_write_header(outputFmtCtx, NULL);
        if (ret < 0) {
            LOGE("Error occurred when writing the file header.\n");
            releaseOutputContext();
        }

        //  Dump the output format context for debugging.
        av_dump_format(outputFmtCtx, 0, outputFileStr, 1);
        streamsAdded = true;
    }

    //  Write each frame from input audio and video to output file interleaved
    bool audioNeedsRead = audioStream != NULL, videoNeedsRead = videoStream != NULL;
    bool audioDone = !audioNeedsRead, videoDone = !videoNeedsRead;
    if (VERBOSE)
        LOGI("Reading audio: %d, Reading video: %d\n", audioNeedsRead, videoNeedsRead);
    int64_t nextAudioPts = 0, nextVideoPts = 0;
    int64_t nextAudioDts = 0, nextVideoDts = 0;
    while (!audioDone || !videoDone) {
        AVStream *inputStream, *outputStream;
        if (!audioDone && audioNeedsRead) {
            ret = av_read_frame(audioFmtCtx, &audioPkt);
            if (ret < 0) {
                audioDone = true;
                audioStartingPts = nextAudioPts;
                audioStartingDts = nextAudioDts;
                LOGI("EOS for audio found. Offset is now %lld.\n", audioStartingPts);
                nextAudioPts = INT_MAX;
            }
            else {
                inputStream = audioFmtCtx->streams[audioPkt.stream_index];
                outputStream = outputFmtCtx->streams[audioStreamIdx];
                audioPkt.pts += audioStartingPts;
                audioPkt.dts += audioStartingDts;
                audioPkt.pos = -1;
                audioPkt.stream_index = audioStreamIdx;
                audioNeedsRead = false;
            }
        }
        if (!videoDone && videoNeedsRead) {
            int lastVideoDuration = videoPkt.duration;
            ret = av_read_frame(videoFmtCtx, &videoPkt);
            if (ret < 0) {
                videoDone = true;
                videoStartingPts = nextVideoPts;
                videoStartingDts = nextVideoDts;
                LOGI("EOS for video found. Offset is now %lld.\n", videoStartingPts);
                nextVideoPts = INT_MAX;
            }
            else {
                inputStream = videoFmtCtx->streams[videoPkt.stream_index];
                outputStream = outputFmtCtx->streams[videoStreamIdx];
                videoPkt.pts = av_rescale_q_rnd(videoPkt.pts, inputStream->time_base,
                                                outputStream->time_base,
                                                AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
                videoPkt.pts += videoStartingPts;
                videoPkt.dts = av_rescale_q_rnd(videoPkt.dts, inputStream->time_base,
                                                outputStream->time_base,
                                                AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
                videoPkt.dts += videoStartingDts;
                videoPkt.duration = av_rescale_q(videoPkt.duration, inputStream->time_base,
                                                 outputStream->time_base);
                videoPkt.pos = -1;
                videoPkt.stream_index = videoStreamIdx;
                if (VERBOSE)
                    LOGI("Read video frame pts %lld.\n", videoPkt.pts);
                videoNeedsRead = false;
            }
        }

        if (!audioDone) {
            nextAudioPts = audioPkt.pts + audioPkt.duration;
            nextAudioDts = audioPkt.dts + audioPkt.duration;
            if (VERBOSE)
                LOGI("Write audio interleaved pts %lld. Next is %lld.\n", audioPkt.pts,
                     nextAudioPts);
            ret = av_interleaved_write_frame(outputFmtCtx, &audioPkt);
            if (ret < 0) {
                LOGE("Error muxing audio packet.\n");
                releaseOutputContext();
            }
            audioNeedsRead = true;
        }
        if (!videoDone) {
            if (encodingNeeded) {
                if (VERBOSE)
                    LOGI("Encoding is needed.\n");
                //  Allocate the frame needed to decode the video packet
                frame = av_frame_alloc();
                int gotFrame;
                if (!frame) {
                    LOGE("Could not allocate video frame.\n");
                    releaseOutputContext();
                }
                //  Decode the video packet
                ret = avcodec_decode_video2(videoStream->codec, frame, &gotFrame, &videoPkt);
                if (ret < 0) {
                    LOGE("Error while decoding frame.\n");
                    releaseOutputContext();
                }
                if (gotFrame) {
                    AVPacket encodedPacket;
                    av_init_packet(&encodedPacket);
                    encodedPacket.data = NULL;
                    encodedPacket.size = 0;
                    ret = avcodec_encode_video2(videoEncCtx, &encodedPacket, frame, &gotFrame);
                    if (ret < 0) {
                        LOGE("Error encoding frame.\n");
                        releaseOutputContext();
                    }
                    if (gotFrame) {
                        encodedPacket.pts = videoPkt.pts;// + videoStartingPts;
                        encodedPacket.dts = videoPkt.dts;
                        encodedPacket.stream_index = videoStreamIdx;
                        encodedPacket.duration = videoPkt.duration;
                        nextVideoPts = videoPkt.pts + encodedPacket.duration;
                        nextVideoDts = videoPkt.dts + encodedPacket.duration;
                        if (VERBOSE)
                            LOGI("Write video compressed interleaved pts %lld. Next is %lld.\n",
                                 encodedPacket.pts,
                                 nextVideoPts);
                        ret = av_interleaved_write_frame(outputFmtCtx, &encodedPacket);
                        if (ret < 0) {
                            LOGE("Error muxing video packet.\n");
                            releaseOutputContext();
                        }
                    }
                }
            }
            else {
                nextVideoPts = videoPkt.pts + videoPkt.duration;
                nextVideoDts = videoPkt.dts + videoPkt.duration;
                if (VERBOSE)
                    LOGI("Write video interleaved pts %lld. Next is %lld.\n", videoPkt.pts,
                         nextVideoPts);
                ret = av_interleaved_write_frame(outputFmtCtx, &videoPkt);
                if (ret < 0) {
                    LOGE("Error muxing video packet.\n");
                    releaseOutputContext();
                }
            }
            videoNeedsRead = true;
        }
    }
}

int stitchFile(const char *audioFileStr, const char *videoFileStr, bool onlyAnalyze) {
    AVFormatContext *videoFmtCtx = NULL, *audioFmtCtx = NULL;
    AVStream *videoStream = NULL, *audioStream = NULL;
    AVCodecContext *videoDecCtx = NULL, *audioDecCtx = NULL;
    int videoStreamIdx = -1, audioStreamIdx = -1;

    LOGI("Opening files %s and %s.\n", audioFileStr, videoFileStr);

    // Open input files, and allocate format context
    if (avformat_open_input(&videoFmtCtx, videoFileStr, NULL, NULL) < 0) {
        LOGE("Could not open video file %s\n", videoFileStr);
        releaseOutputContext();
    }
    if (avformat_open_input(&audioFmtCtx, audioFileStr, NULL, NULL) < 0) {
        LOGE("Could not open audio file %s\n", audioFileStr);
        releaseOutputContext();
    }

    // Retrieve stream information
    if (avformat_find_stream_info(videoFmtCtx, NULL) < 0) {
        LOGE("Could not find video stream information.\n");
        releaseOutputContext();
    }
    if (avformat_find_stream_info(audioFmtCtx, NULL) < 0) {
        LOGE("Could not find audio stream information.\n");
        releaseOutputContext();
    }

    //  Open the context and setup the decoder.
    if (openCodecContext(&videoStreamIdx, videoFmtCtx, AVMEDIA_TYPE_VIDEO) >= 0) {
        videoStream = videoFmtCtx->streams[videoStreamIdx];
        videoDecCtx = videoStream->codec;
    }
    else {
        LOGE("Couldn't find a video stream in file %s.", videoFileStr);
        releaseOutputContext();
    }
    if (openCodecContext(&audioStreamIdx, audioFmtCtx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audioStream = audioFmtCtx->streams[audioStreamIdx];
        audioDecCtx = audioStream->codec;
    }
    else {
        LOGE("Couldn't find a audio stream in file %s.", audioFileStr);
        releaseOutputContext();
    }

    //av_dump_format(videoFmtCtx, 0, videoFileStr, 0);
    //av_dump_format(audioFmtCtx, 0, audioFileStr, 0);

    // For logging purpose, dump the format.
    LOGI("Video dimensions are %dx%d and pixel format is %d, codec_id = %d.\n",
         videoDecCtx->width, videoDecCtx->height, videoDecCtx->pix_fmt, videoDecCtx->codec_id);

    //  Height and width of first video will be used for subsequent videos.
    if (!outputWidth || !outputHeight) {
        outputWidth = videoDecCtx->width;
        outputHeight = videoDecCtx->height;
    }

    bool reEncodingNeeded = outputWidth != videoDecCtx->width || outputHeight != videoDecCtx->height
                            || (outputVideoCodecID != -1 &&
                                outputVideoCodecID != videoDecCtx->codec_id);
    if (reEncodingNeeded) {
        LOGI("Reencoding needed is needed. output codec is %s, and input codec is %s.\n",
             avcodec_get_name(outputVideoCodecID),
             avcodec_get_name(videoDecCtx->codec_id));
    }
    if (reEncodingNeeded) {
        outputVideoCodecID = AV_CODEC_ID_MPEG4;
    }

    if (!onlyAnalyze) {
        writeInterleavedStream(audioStream, videoStream, audioFmtCtx, videoFmtCtx,
                               reEncodingNeeded);
    }

    //  Release resources.
    avcodec_close(videoDecCtx);
    avcodec_close(audioDecCtx);
    avformat_close_input(&videoFmtCtx);
    avformat_close_input(&audioFmtCtx);

    return videoDecCtx->codec_id;
}

int main(int argc, char **argv) {
    int i;

    //  Initialize the variables maybe from a prior mux
    outputWidth = 0, outputHeight = 0, outputVideoCodecID = -1;
    outputFmtCtx = NULL;
    outputFmt = NULL;

    outputVideoCodec = NULL;
    videoEncCtx = NULL;

    streamsAdded = false;
    outputFileStr = NULL;
    videoStartingPts = 0, audioStartingPts = 0;
    videoStartingDts = 0, audioStartingDts = 0;
    videoStreamIdx = -1, audioStreamIdx = -1;

    if (argc < 2 || (argc - 2) % 2 != 0) {
        printf("usage: %s <audio file> <video file> ... <output file>\n"
                       "This is a test program to mux multiple mp4 audio and video files."
                       "\n", argv[0]);
        return 1;
    }

    av_register_all();

    configureOutputContext(argv[argc - 1]);

    //  Read through all the files to make sure we need to do encoding (if they're not all same format)
    for (i = 1; i <= (argc - 2); i += 2) {
        int formatCodecId = stitchFile(argv[i], argv[i + 1], true);
        if (outputVideoCodecID == -1) {
            outputVideoCodecID = formatCodecId;
        }
        else if (outputVideoCodecID != formatCodecId) {
            outputVideoCodecID = AV_CODEC_ID_MPEG4;
            break;
        }
    }
    //  Go through files again and this time stitch them together.
    for (i = 1; i <= (argc - 2); i += 2) {
        stitchFile(argv[i], argv[i + 1], false);
    }


    //  Write the file trailer
    av_write_trailer(outputFmtCtx);

    releaseOutputContext();
    return 0;
}

#ifdef ANDROID
JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_startGoPro(JNIEnv *env,
                                                              jobject  __unused instance) {
    av_register_all();
    avformat_network_init();
    avcodec_register_all();

    AVFormatContext *videoFmtCtx = NULL;
    AVStream *videoStream = NULL, *audioStream = NULL;
    AVCodecContext *videoDecCtx = NULL, *audioDecCtx = NULL;
    AVInputFormat *videoFormat = NULL;
    int videoStreamIdx = -1, audioStreamIdx = -1;

    AVDictionary *options_dict = NULL;
    av_dict_set(&options_dict, "probesize", "16384", 0);

    // Open input files, and allocate format context
    char* inputFormat ="mpegts";

    if(!(videoFormat = av_find_input_format(inputFormat))){
        LOGE("Unknown input format %s", inputFormat);
    }

    LOGE("Video Format:%s",videoFormat->name);
    if (avformat_open_input(&videoFmtCtx, "udp://10.5.5.9:8554",
            videoFormat, &options_dict) < 0) {
        LOGE("Could not open video file %s\n", "udp://10.5.5.9:8554");
        releaseOutputContext();
    }

    videoFmtCtx->iformat = videoFormat;

    // Retrieve stream information
    if (avformat_find_stream_info(videoFmtCtx, NULL) < 0) {
        LOGE("Could not find video stream information.\n");
        releaseOutputContext();
    }

    LOGI("found stream info");

    //  Open the context and setup the decoder.
    if (openCodecContext(&videoStreamIdx, videoFmtCtx, AVMEDIA_TYPE_VIDEO) >= 0) {
        videoStream = videoFmtCtx->streams[videoStreamIdx];
        videoDecCtx = videoStream->codec;
    }
    else {
        LOGE("Couldn't find a video stream in file %s.", "udp://10.5.5.9:8554");
        releaseOutputContext();
    }
    if (openCodecContext(&audioStreamIdx, videoFmtCtx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audioStream = videoFmtCtx->streams[audioStreamIdx];
        audioDecCtx = audioStream->codec;
    }
    else {
        LOGE("Couldn't find a audio stream in file %s.", "udp://10.5.5.9:8554");
        releaseOutputContext();
    }

    LOGE("Dumping input format!!!");

    av_dump_format(videoFmtCtx, 0, "udp://10.5.5.9:8554", 0);
    bool videoDone = 0;
    int ret = 0;
    AVPacket videoPkt;
    int error = 0;

    jclass thisClass = (*env)->GetObjectClass(env, instance);
    jmethodID connectionCallback = (*env)->GetMethodID(env, thisClass, "onReceiveGoProFrame", "(III)V");

    while (!videoDone) {
        ret = av_read_frame(videoFmtCtx, &videoPkt);
        /*LOGI("Reading %s %s frame, pts is %lld",
            (&videoPkt)->stream_index == audioStreamIdx ? "audio" : "video",
            ((&videoPkt)->flags & AV_PKT_FLAG_KEY == 1 ? "key" : "regular"),
            (&videoPkt)->pts);*/
        if((&videoPkt)->stream_index == audioStreamIdx){
            continue;
        }
        if (ret < 0) {
            videoDone = true;
        }
        else {
            if (connectionCallback) {
                (*env)->CallVoidMethod(env, instance, connectionCallback,
                (int)(&videoPkt), (int)(audioStream), (int)(videoStream));
            }
        }
     }
     LOGE("Done with the input stream.");
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_muxFiles(JNIEnv *env,
                                                              jobject  __unused instance,
                                                              jobjectArray filesArray) {
    int i;
    av_register_all();

    //  Initialize the variables maybe from a prior mux
    outputWidth = 0, outputHeight = 0, outputVideoCodecID = -1;
    outputFmtCtx = NULL;
    outputFmt = NULL;

    outputVideoCodec = NULL;
    videoEncCtx = NULL;

    streamsAdded = false;
    outputFileStr = NULL;
    videoStartingPts = 0, audioStartingPts = 0;
    videoStartingDts = 0, audioStartingDts = 0;
    videoStreamIdx = -1, audioStreamIdx = -1;

    int stringCount = (int) (*env)->GetArrayLength(env, filesArray);
    jstring outputJString = (jstring) (*env)->GetObjectArrayElement(env, filesArray,
                                                                    stringCount - 1);
    char *outputRawString = (char *) (*env)->GetStringUTFChars(env, outputJString, 0);
    configureOutputContext(outputRawString);

    //  Read through all the files to make sure we need to do encoding (if they're not all same format)
    for (i = 0; i < stringCount - 1; i += 2) {
        jstring string = (jstring) (*env)->GetObjectArrayElement(env, filesArray, i);
        char *rawString = (char *) (*env)->GetStringUTFChars(env, string, 0);
        jstring string2 = (jstring) (*env)->GetObjectArrayElement(env, filesArray, i + 1);
        char *rawString2 = (char *) (*env)->GetStringUTFChars(env, string2, 0);

        int formatCodecId = stitchFile(rawString, rawString2, true);
        if(outputVideoCodecID == -1){
            outputVideoCodecID = formatCodecId;
        }
        else if(outputVideoCodecID != formatCodecId){
            outputVideoCodecID = AV_CODEC_ID_MPEG4;
            break;
        }

        (*env)->ReleaseStringUTFChars(env, string, rawString);
        (*env)->ReleaseStringUTFChars(env, string2, rawString2);
    }
    //  Go through files again and this time stitch them together.
    for (i = 0; i < stringCount - 1; i += 2) {
        jstring string = (jstring) (*env)->GetObjectArrayElement(env, filesArray, i);
        char *rawString = (char *) (*env)->GetStringUTFChars(env, string, 0);
        jstring string2 = (jstring) (*env)->GetObjectArrayElement(env, filesArray, i + 1);
        char *rawString2 = (char *) (*env)->GetStringUTFChars(env, string2, 0);

        stitchFile(rawString, rawString2, false);

        (*env)->ReleaseStringUTFChars(env, string, rawString);
        (*env)->ReleaseStringUTFChars(env, string2, rawString2);
    }

    LOGI("File saved to %s", outputRawString);

    //  Write the file trailer
    av_write_trailer(outputFmtCtx);
    releaseOutputContext();
    (*env)->ReleaseStringUTFChars(env, outputJString, outputRawString);
}
#endif