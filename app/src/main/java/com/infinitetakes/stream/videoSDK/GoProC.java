package com.infinitetakes.stream.videoSDK;

import java.io.IOException;

/**
 * This inner class will talk to the C side JNI code. Needs to be protected because
 * PreviewSurface will access the surfaceDraw, and surfaceResize.
 */
@SuppressWarnings({"JniMissingFunction", "unused"})
class GoProC {
    static {
        System.loadLibrary("GoProWrapper");
    }
    /**
     * These are callbacks fired from the C to the Java side.
     */
    protected interface InternalCallbacks {
        void onConnectionDropped();

        void onConnectionRestored();

        void onReceiveGoProFrame(int ptrData, int ptrAudioStream, int ptrVideoStream);
    }

    protected InternalCallbacks callback = null;

    //  Acquiring from GoPro
    protected native void startReading();

    protected native void init(GoProWrapper.Metadata jOpts) throws Exception;

    //  Streaming to RTMP
    protected native void startWriting(int audioStreamPtr, int videoStreamPtr) throws IOException;

    protected native void writeFrame(int addy);

    //  OpenGL Surface stuff (this is protected, because PreviewSurface will call these).
    protected native void surfaceResize(int w, int h);

    protected native void surfaceDraw();

    public void setCallback(InternalCallbacks callback){
        this.callback = callback;
    }

    /**
     * Don't touch the signatures of the four methods below, they are called from the C side and
     * are very sensitive to changes.
     */
    protected void onConnectionDropped() {
        if(callback != null){
            callback.onConnectionDropped();
        }
    }

    protected void onConnectionRestored() {
        if(callback != null){
            callback.onConnectionRestored();
        }
    }

    protected void onReceiveGoProFrame(int ptrData, int ptrAudioStream, int ptrVideoStream) {
        if(callback != null){
            callback.onReceiveGoProFrame(ptrData, ptrAudioStream, ptrVideoStream);
        }
    }
}
