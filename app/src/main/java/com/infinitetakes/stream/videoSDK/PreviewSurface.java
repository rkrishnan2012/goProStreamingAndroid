package com.infinitetakes.stream.videoSDK;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * Created by rkrishnan on 9/17/15.
 */
public class PreviewSurface extends GLSurfaceView {
    public GoProWrapper mWrapper;
    boolean needsSurfaceChange = false;
    int currWidth, currHeight;

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
        public void onSurfaceCreated(GL10 gl, EGLConfig c) {
            //  Do nothing.
            gl.glClearColor(0.0f, 0.0f, 1.0f, 0.0f);
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int w, int h) {
            currWidth = w;
            currHeight = h;
            if(mWrapper != null){
              mWrapper.surfaceResize(w, h);
            }
            else{
                needsSurfaceChange = true;
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
                Log.d("GLBUFEX", "FPS: " + Float.toString(avgFPS));
                avgFPS = 0;
            }
            framerate++;
            if(mWrapper!= null){
                if(needsSurfaceChange){
                    needsSurfaceChange = false;
                    mWrapper.surfaceResize(currWidth, currHeight);
                }
                mWrapper.surfaceDraw();
            }
        }

        public long time = 0;
        public short framerate = 0;
        public long fpsTime = 0;
        public long frameTime = 0;
        public float avgFPS = 0;
    }
}
