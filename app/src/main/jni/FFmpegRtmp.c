#include "FFmpegRtmp.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

void initConnection(JNIEnv *env) {
    int error = 0;
    AVCodec *audio_codec, *video_codec;

    // Register the codec, formats, and the network.
    LOGI("Registering codec, formats, and the network.");
    av_register_all();
    avformat_network_init();
    avcodec_register_all();

    // Allocate resource for outputFormatContext
    LOGI("Allocating avformatContext.");
    if ((error = avformat_alloc_output_context2(&outputFormatContext, NULL,
                                                metadata.outputFormatName,
                                                metadata.outputFile)) < 0) {
        char errorStr[1024];
        get_error_string(error, errorStr);
        jclass exc = (*env)->FindClass(env, "java/lang/Exception");
        (*env)->ThrowNew(env, exc, errorStr);
        release_resources();
        return;
    }

    if (!outputFormatContext) {

        jclass exc = (*env)->FindClass(env, "java/lang/Exception");
        (*env)->ThrowNew(env, exc, "outputFormatContext was null!");
        release_resources();
        return;
    }



    // Verify that all the parameters have been set, or throw an IllegalArgumentException to Java.
    if (!metadata.videoHeight || !metadata.videoWidth || !metadata.audioSampleRate ||
        !metadata.videoBitrate ||
        !metadata.numAudioChannels || !metadata.outputFormatName || !metadata.outputFile) {
        jclass exc = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
        (*env)->ThrowNew(env, exc, "Make sure all the Metadata parameters have been passed.");
        release_resources();
        return;
    }
}


/**
 * Take the Java Metadata object and populate the C struct declared above with these parameters.
 */
void populate_metadata_from_java(JNIEnv *env, jobject jOpts) {
    jclass jMetadataClass = (*env)->GetObjectClass(env, jOpts);
    jfieldID jVideoHeightId = (*env)->GetFieldID(env, jMetadataClass, "videoHeight", "I");
    jfieldID jVideoWidthId = (*env)->GetFieldID(env, jMetadataClass, "videoWidth", "I");
    jfieldID jVideoBitrate = (*env)->GetFieldID(env, jMetadataClass, "videoBitrate", "I");
    jfieldID jAudioSampleRateId = (*env)->GetFieldID(env, jMetadataClass, "audioSampleRate", "I");
    jfieldID jNumAudioChannelsId = (*env)->GetFieldID(env, jMetadataClass, "numAudioChannels", "I");
    jfieldID jOutputFormatName = (*env)->GetFieldID(env, jMetadataClass, "outputFormatName",
                                                    "Ljava/lang/String;");
    jfieldID jOutputFile = (*env)->GetFieldID(env, jMetadataClass, "outputFile",
                                              "Ljava/lang/String;");
    jstring jStrOutputFormatName = (*env)->GetObjectField(env, jOpts, jOutputFormatName);
    jstring jStrOutputFile = (*env)->GetObjectField(env, jOpts, jOutputFile);

    metadata.videoHeight = (*env)->GetIntField(env, jOpts, jVideoHeightId);
    metadata.videoWidth = (*env)->GetIntField(env, jOpts, jVideoWidthId);
    metadata.videoBitrate = (*env)->GetIntField(env, jOpts, jVideoBitrate);
    metadata.audioSampleRate = (*env)->GetIntField(env, jOpts, jAudioSampleRateId);
    metadata.numAudioChannels = (*env)->GetIntField(env, jOpts, jNumAudioChannelsId);
    metadata.outputFormatName = (*env)->GetStringUTFChars(env, jStrOutputFormatName, 0);
    metadata.outputFile = (*env)->GetStringUTFChars(env, jStrOutputFile, 0);
}

/**
 * Add an output stream to the given AVFormatContext.
 */
static AVStream *add_stream(AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id) {
    AVCodecContext *codecContext = NULL;
    AVStream *st = NULL;

    //  This will be null for video stream, since we're not encoding the video, only the audio.

    st = avformat_new_stream(oc, *codec);
    codecContext = st->codec;
    if (!st) {
        LOGE("Could not create a new stream.");
        return NULL;
    }

    videoStreamIndex = 0;
    audioStreamIndex = 0;

    if (codec_id == VIDEO_CODEC_ID) {
        codecContext->width = metadata.videoWidth;
        codecContext->height = metadata.videoHeight;
        //codecContext->debug = 1;
    }


    //audioStream->codec->codec_tag = 0;
    //videoStream->codec->codec_tag = 0;

    /*st->id = 0;
    codecContext = st->codec;
    avcodec_get_context_defaults3(codecContext, *codec);
    st->time_base = flvDestTimebase;
    //  Add according stream parameters
    if (codec_id == VIDEO_CODEC_ID) {
        LOGI("Video codec id is %s\n.", avcodec_get_name(VIDEO_CODEC_ID));
        videoStreamIndex = 0;
        codecContext->codec_id = VIDEO_CODEC_ID;
        codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
        codecContext->bit_rate = metadata.videoBitrate;
        codecContext->width = metadata.videoWidth;
        codecContext->height = metadata.videoHeight;
        codecContext->pix_fmt = VIDEO_PIX_FMT;
        codecContext->framerate = (AVRational){30,1};
        //codecContext->debug = 1;
    } else if (codec_id == AUDIO_CODEC_ID) {
        audioStreamIndex = 1;
        codecContext->codec_type = AVMEDIA_TYPE_AUDIO;
        codecContext->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL; // for native aac support
        codecContext->sample_fmt = AUDIO_SAMPLE_FMT;
        codecContext->sample_rate = metadata.audioSampleRate;
        codecContext->channels = metadata.numAudioChannels;
        codecContext->debug = 1;
    } else {
        LOGE("Ignoring unknown codec type %d", codec_id);
        return NULL;
    }*/

    // Some formats want stream headers to be separate.
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        codecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
    LOGE("RETURNIN");
    return st;
}

/**
 * Get the underlying message from an AVError.
 */
char *get_error_string(int errorNum, char *errBuf) {
    int ret = AVERROR(errorNum);
    av_strerror(ret, errBuf, 1024 * sizeof(char));
    LOGE("%s", errBuf);
    return errBuf;
}

/**
 * This function is called when an exception is thrown and we want to quit everything, or
 * when stop is called. It will try to release all the allocated resources, even if we're in a
 * bad state.
 */
void release_resources() {
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(outputFormatContext);

    if (videoStream) {
        LOGI("Closing video stream.");
        avcodec_close(videoStream->codec);
    }
    if (audioStream) {
        LOGI("Closing audio stream.");
        avcodec_close(audioStream->codec);
    }
    if (outputFormatContext) {
        LOGI("Freeing output context.");
        avformat_free_context(outputFormatContext);
    }
    if (packet) {
        LOGI("Freeing packet.");
        av_free_packet(packet);
    }
    LOGI("De-initializing network.");
    avformat_network_deinit();
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_init(JNIEnv *env, jobject  __unused instance,
                                                          jobject jOpts) {
    LOGI("Init() started.");
// Get the values passed in from java side and populate the struct.
    populate_metadata_from_java(env, jOpts);

//  Do the heavy duty stuff.
    initConnection(env);
    LOGI("Init() finished.");
    isConnectionOpen = 0;
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_start(JNIEnv  __unused *env,
                                                           jobject  __unused instance,
                                                    jint ptrAudioStream, jint ptrVideoStream) {
    LOGI("start() started.");
    AVStream *videoStreamIn = (AVStream*)ptrVideoStream, *audioStreamIn = (AVStream*)ptrAudioStream;

    // Add the audio and video streams to the codec.
    AVOutputFormat *fmt = outputFormatContext->oformat;

    videoStream = avformat_new_stream(outputFormatContext, videoStreamIn->codec->codec);
    //  Copy the input stream context to output file.
    int ret = avcodec_copy_context(videoStream->codec, videoStreamIn->codec);
    if (ret < 0) {
        LOGE("Failed to copy context from input to output audio stream codec context\n");
        releaseOutputContext();
    }

    videoStream->codec->width = metadata.videoWidth;
    videoStream->codec->height = metadata.videoHeight;
    videoStream->codec->codec_tag = 0;
    videoStream->codec->bit_rate = metadata.videoBitrate;
    videoStream->codec->framerate = (AVRational){30,1};

    //videoStream->time_base = videoStream->codec->time_base;

    if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        videoStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    /*audioStream = avformat_new_stream(outputFormatContext, audioStreamIn->codec->codec);

    if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        audioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_copy_context(audioStream->codec, audioStreamIn->codec);
    if (ret < 0) {
        LOGE("Failed to copy context from input to output audio stream codec context\n");
        releaseOutputContext();
    }
    audioStream->time_base = audioStream->codec->time_base;
    audioStream->codec->codec_tag = 0;*/

    // Debug the output format
    LOGI("Dumping outputFormat.");
    av_dump_format(outputFormatContext, 0, NULL, 1);

    if (isConnectionOpen == 0) {
        LOGI("Connection isn't opened yet.");
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (!avio_open(&outputFormatContext->pb, metadata.outputFile, AVIO_FLAG_WRITE)){
                LOGI("Opened connection success.");
                isConnectionOpen = 1;
            }
            else{
                LOGE("Can't connect to the internet.");
                jclass exc = (*env)->FindClass(env, "java/lang/Exception");
                (*env)->ThrowNew(env, exc, "Internet connection is not available.");
                release_resources();
                return;
            }
        }
        else {
            LOGI("No need to open connection.");
            isConnectionOpen = 1;
        }

    }
    else{
        LOGI("Connection already opened, isConnectionOpen = %d.", isConnectionOpen);
    }
    // Write the header to the stream.
    if(avformat_write_header(outputFormatContext, NULL) < 0){
        LOGE("Couldn't write header to the file.");
        release_resources();
    }
    {
        LOGE("Wrote header to file!!!");
    }

//  Malloc the packet early for later use.
    if (!packet) {
        packet = av_malloc(sizeof(AVPacket));
        av_init_packet(packet);
    }
    LOGI("start() finished.");

}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_writePacketInterleaved(JNIEnv *env,
                                                                            jobject  __unused instance,
                                                                            jobject jData,
                                                                            jint jIsVideo,
                                                                            jint jSize,
                                                                            jlong jPts,
                                                                            jint jIsKeyFrame) {
    if (isConnectionOpen == 0) {
        LOGI("Connection is not open, trying to reconnect now.");
        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (!avio_open(&outputFormatContext->pb, metadata.outputFile, AVIO_FLAG_WRITE)) {
// Release the existing connection
                release_resources();
//  Re-connect the stream
                initConnection(env);
// Write the header to the stream.
                avformat_write_header(outputFormatContext, NULL);
//  Fire the callback that connection has been restored to java.
                jclass thisClass = (*env)->GetObjectClass(env, instance);
                jmethodID connectionCallback = (*env)->GetMethodID(env, thisClass,
                                                                   "onConnectionRestored", "()V");
                if (connectionCallback) {
                    (*env)->CallVoidMethod(env, instance, connectionCallback);
                }
            }
            else {
//  Connection is still not ready.
                return;
            }
        }
        else {
            LOGI("Reconnected.");
            isConnectionOpen = 1;
// Release the existing connection
            release_resources();
//  Re-connect the stream
            initConnection(env);
// Write the header to the stream.
            avformat_write_header(outputFormatContext, NULL);
//  Fire the callback that connection has been restored to java.
            jclass thisClass = (*env)->GetObjectClass(env, instance);
            jmethodID connectionCallback = (*env)->GetMethodID(env, thisClass,
                                                               "onConnectionRestored", "()V");
            if (connectionCallback) {
                (*env)->CallVoidMethod(env, thisClass, connectionCallback);
            }
        }
    }

    if (!frameCount) {
        LOGI("Writing interleaved.");
    }
    int error = 0;
// Get the Byte array backing the ByteBuffer.
    uint8_t *data = (*env)->GetDirectBufferAddress(env, jData);

// Put the necessary data from ByteBuffer into the AVPacket.
    packet->size = jSize;
    packet->data = data;
    if (jIsVideo == JNI_TRUE) {
        packet->stream_index = videoStreamIndex;
        packet->pts = av_rescale_q((int64_t) jPts, flvDestTimebase, androidSourceTimebase);
        packet->dts = packet->pts;
        packet->duration = packet->pts - lastVideoPts;
        lastVideoPts = packet->pts;
        if (jIsKeyFrame == 1){
            packet->flags |= AV_PKT_FLAG_KEY;
            foundKeyFrame = true;
            LOGI("KEYFRAME video Size = %d and pts is %lld.Stream num is %d.", jSize, packet->pts, packet->stream_index);
        }
        else if(!foundKeyFrame){
            LOGI("Dropping initial frame because not seen keyframe yet.");
            return;
        }
        else{
            LOGI("FRAME video Size = %d and pts is %lld.Stream num is %d.", jSize, packet->pts, packet->stream_index);
        }
    } else {
        if(!foundKeyFrame){
            return;
        }
        packet->stream_index = audioStreamIndex;
        packet->pts = av_rescale_q((int64_t) jPts, flvDestTimebase, androidSourceTimebase);
        packet->dts = packet->pts;
        packet->duration = packet->pts - lastAudioPts;
        lastAudioPts = packet->pts;
        LOGI("Audio writeInterleaved() called. Size = %d and pts is %lld. Stream num is %d.", jSize, packet->pts, packet->stream_index);
    }

// Send the packet to the stream.
    error = av_interleaved_write_frame(outputFormatContext, packet);
    if (error < 0) {
        LOGE("Couldn't write frame!");
        isConnectionOpen = 0;
//  Fire the callback that connection has been lost to java.
        jclass thisClass = (*env)->GetObjectClass(env, instance);
        jmethodID connectionCallback = (*env)->GetMethodID(env, thisClass, "onConnectionDropped",
                                                           "()V");
        if (connectionCallback) {
            (*env)->CallVoidMethod(env, instance, connectionCallback);
        }
    }

    if(!frameCount){
        LOGI("writeInterlaved() finish.");
    }

    frameCount++;
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_writeGoProFrame(JNIEnv  __unused *env,
        jobject  __unused instance, jint addy) {
    AVPacket *pkt = (AVPacket*)addy;
    int error;
    pkt->pts /= 100;
    pkt->dts /= 100;
    pkt->duration = pkt->pts - lastVideoPts;

    lastVideoPts = pkt->pts;

    pkt->stream_index = 0;

    LOGI("Writing interleaved %s frame. Pts = %llu and duration is %d",
         pkt->stream_index == audioStreamIndex ? "audio" : "video", pkt->pts, pkt->duration);
    error = av_interleaved_write_frame(outputFormatContext, pkt);
    if (error < 0) {
        LOGE("Couldn't write frame!");
    }

    LOGI("writeInterlaved() finish.");

    frameCount++;
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_FFmpegWrapper_stop(JNIEnv  __unused *env,
                                                          jobject  __unused instance) {
    LOGE("stop() started.");
    release_resources();
    LOGE("stop() finished.");
}
