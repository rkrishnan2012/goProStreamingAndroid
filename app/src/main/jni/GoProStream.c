#include "GoProStream.h"

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
Java_com_infinitetakes_stream_videoSDK_GoProWrapper_init(JNIEnv *env, jobject  __unused instance,
                                                          jobject jOpts) {
    // Get the values passed in from java side and populate the struct.
    populate_metadata_from_java(env, jOpts);
    //  Connect to the network URL.
    initConnection(env);
    isConnectionOpen = 0;
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_GoProWrapper_startWriting(JNIEnv  __unused *env,
                                                           jobject  __unused instance,
                                                    jint ptrAudioStream, jint ptrVideoStream) {
    // The input audio and video stream are passed in from the other C file as pointers.
    AVStream *videoStreamIn = (AVStream*)ptrVideoStream, *audioStreamIn = (AVStream*)ptrAudioStream;
    if(videoStreamIn != NULL){
        // Create an output video stream
        videoStream = avformat_new_stream(outputFormatContext, videoStreamIn->codec->codec);
        //  Copy codec parameters from input video stream to output video stream.
        int ret = avcodec_copy_context(videoStream->codec, videoStreamIn->codec);
        if (ret < 0) {
        LOGE("Failed to copy context from input to output audio stream codec  context\n");
        releaseOutputContext();
        }
        //  Some things are not set in the input stream, but required for output stream.
        videoStream->codec->width = metadata.videoWidth;
        videoStream->codec->height = metadata.videoHeight;
        videoStream->codec->codec_tag = 0;
        videoStream->codec->bit_rate = metadata.videoBitrate;
        videoStream->codec->framerate = (AVRational){30,1};
        videoStream->time_base = videoStream->codec->time_base;
        if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        videoStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }
    if(audioStreamIn != NULL){
        //  Create an output audio stream.
        audioStream = avformat_new_stream(outputFormatContext, audioStreamIn->codec->codec);
        //  Copy codec parameters from input audio stream to output audio stream.
        int ret = avcodec_copy_context(audioStream->codec, audioStreamIn->codec);
        if (ret < 0) {
        LOGE("Failed to copy context from input to output audio stream codec context\n");
        releaseOutputContext();
        }
        //  Some things are not set in the input stream, but required for output stream.
        audioStream->time_base = audioStream->codec->time_base;
        audioStream->codec->codec_tag = 0;
        if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        audioStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }
    //  If the connection was not able to be opened earlier, try again now.
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
    // Write the header to the stream.
    if(avformat_write_header(outputFormatContext, NULL) < 0){
        LOGE("Couldn't write header to the file.");
        release_resources();
    }
    //  Malloc the packet early for later use.
    if (!packet) {
        packet = av_malloc(sizeof(AVPacket));
        av_init_packet(packet);
    }
}

JNIEXPORT void JNICALL
Java_com_infinitetakes_stream_videoSDK_GoProWrapper_writeFrame(JNIEnv  __unused *env,
        jobject  __unused instance, jint addy) {
    //  Reference to AVPacket is passed in from other C process.
    AVPacket *pkt = (AVPacket*)addy;
    int error;
    if(pkt->stream_index == videoStreamIndex){
        //  Input video stream packets don't have a duration, so detect that value.
        pkt->pts /= 100;
        pkt->dts /= 100;
        pkt->duration = pkt->pts - lastVideoPts;
        lastVideoPts = pkt->pts;
    }
    else{
        //  Input audio stream packets don't have a pts+dts, so detect that.
        pkt->pts = lastAudioPts;
        pkt->dts = lastAudioPts;
        pkt->pts /= 100;
        pkt->dts /= 100;
        lastAudioPts += pkt->duration;
    }
    error = av_interleaved_write_frame(outputFormatContext, pkt);
    if (error < 0) {
        LOGE("Couldn't write frame!");
    }
    frameCount++;
}
