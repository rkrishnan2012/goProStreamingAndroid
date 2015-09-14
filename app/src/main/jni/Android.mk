LOCAL_PATH := $(call my-dir)

APP_PLATFORM := android-10

include $(CLEAR_VARS)

# Prebuilt FFmpeg

LOCAL_MODULE:= libvlcjni
ifeq ($(APP_ABI),armeabi-v7a)
    LOCAL_SRC_FILES:= ../../../../libvlc/jni/libs/armeabi-v7a/libvlcjni.so
else
    LOCAL_SRC_FILES:= ../jniLibs/x86/libvlcjni.so
endif
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := FFmpegWrapper
LOCAL_LDLIBS += -llog -lz
LOCAL_SHARED_LIBRARIES := libvlcjni
LOCAL_SRC_FILES := \
    FFmpegMuxer.c \
    FFmpegRtmp.c

LOCAL_CFLAGS := -O3

ifeq ($(TARGET_ARCH),armeabi-v7a)
    LOCAL_CFLAGS += -march=armv7-a -mfloat-abi=hardfp -mfpu=neon
endif

include $(BUILD_SHARED_LIBRARY)
