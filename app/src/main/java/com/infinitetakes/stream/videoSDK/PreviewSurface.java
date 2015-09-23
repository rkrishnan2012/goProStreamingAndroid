package com.infinitetakes.stream.videoSDK;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * -This is a custom GLSurfaceView that will communicate with the GoProWrapper and render the
 * preview texture to the view. Make sure to call setNativeWrapper() to actually begin rendering
 * the frames on to the screen. The size of the view can be anything, and it will automatically
 * resize itself accordingly.
 * -Make sure OpenGL 2 is enabled in the Manifest as well.
 * Rohit Krishnan - September, 2015
 */
public class PreviewSurface extends GLSurfaceView {
    public GoProWrapper mWrapper;
    boolean needsSurfaceChange = false;
    int currWidth, currHeight;
    public static final double GOPRO_H_W_RATIO = 5/9;

    public void setNativeWrapper(GoProWrapper wrapper){
        this.mWrapper = wrapper;
    }

    public PreviewSurface(Context context, AttributeSet attrs) {
        super(context, attrs);
        setRenderer(new MyRenderer());
        requestFocus();
        setFocusableInTouchMode(true);
    }

    class MyRenderer implements GLSurfaceView.Renderer {
        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig c) {}

        @Override
        public void onSurfaceChanged(GL10 gl, final int width, int height) {
            currWidth = width;
            currHeight = height;
            //  Let's fix the height to be the proper aspect ratio from the GoPro.
            final int aspectRatioHeight = (int)(width * GOPRO_H_W_RATIO);
            if(height != aspectRatioHeight){
                new Handler(Looper.getMainLooper()).post(new Runnable() {
                    @Override
                    public void run() {
                        getHolder().setFixedSize(width, aspectRatioHeight);
                    }
                });
            }
            else{
                if(mWrapper != null && mWrapper.mReadProcess != null){
                    mWrapper.mReadProcess.surfaceResize(width, height);
                }
                else{
                    needsSurfaceChange = true;
                }
            }
        }

        @Override
        public void onDrawFrame(GL10 gl) {
            time = SystemClock.uptimeMillis();

            if (time >= (frameTime + 1000.0f)) {
                frameTime = time;
                avgFPS += framerate;
                framerate = 0;
            }

            if (time >= (fpsTime + 3000.0f)) {
                fpsTime = time;
                avgFPS /= 3.0f;
                Log.d("PreviewSurface", "FPS: " + Float.toString(avgFPS));
                avgFPS = 0;
            }
            framerate++;
            if(mWrapper!= null && mWrapper.mReadProcess != null){
                if(needsSurfaceChange){
                    needsSurfaceChange = false;
                    mWrapper.mReadProcess.surfaceResize(currWidth, currHeight);
                }
                mWrapper.mReadProcess.surfaceDraw();
            }
        }

        public long time = 0;
        public short framerate = 0;
        public long fpsTime = 0;
        public long frameTime = 0;
        public float avgFPS = 0;
    }
}
