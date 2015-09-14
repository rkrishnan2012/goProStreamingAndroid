/*
 * Copyright (c) 2013, David Brodsky. All rights reserved.
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *	
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.infinitetakes.stream.videoSDK;

import android.util.Log;

import java.io.IOException;
import java.nio.ByteBuffer;

public class FFmpegWrapper {
    private Callbacks callback = null;

    static {
        System.loadLibrary("FFmpegWrapper");
    }

    /**
     * Setup FFmpeg to begin streaming later.
     *
     * @throws IllegalArgumentException if not all the parameters are passed properly.
     * @throws Exception                if some other error happens during allocating context.
     */
    public native void init(Metadata jOpts) throws Exception;

    public native void start(int audioStreamPtr, int videoStreamPtr) throws IOException;

    public native void writePacketInterleaved(ByteBuffer jData, int jIsVideo, int jSize, long jPts,
                                              int isKeyFrame);

    public native void stop();

    public native void muxFiles(String[] filesArray);

    public native void startGoPro();

    public native void testConnection();

    public native void writeGoProFrame(int addy);

    private void onConnectionDropped() {
        if (callback != null) {
            callback.onConnectionDropped();
        }
    }

    private void onConnectionRestored() {
        if (callback != null) {
            callback.onConnectionRestored();
        }
    }

    private void onReceiveGoProFrame(int ptrData, int ptrAudioStream, int ptrVideoStream) {
        if (callback != null) {
            callback.onReceiveGoProFrame(ptrData, ptrAudioStream, ptrVideoStream);
        }
    }

    public void setCallback(Callbacks callback) {
        this.callback = callback;
    }

    public static class Metadata {
        //  Video codec options
        public int videoWidth = 432;
        public int videoHeight = 240;
        public int videoBitrate = 4000000;
        //  Audio codec options
        public int audioSampleRate = 96000;
        public int numAudioChannels = 2;
        //  Format options
        public String outputFormatName = "flv";
        public String outputFile;
    }

    public interface Callbacks {
        void onConnectionDropped();

        void onConnectionRestored();

        void onReceiveGoProFrame(int ptrData, int ptrAudioStream, int ptrVideoStream);
    }
}
