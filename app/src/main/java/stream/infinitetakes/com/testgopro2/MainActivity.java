package stream.infinitetakes.com.testgopro2;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.widget.TextView;

import com.infinitetakes.stream.videoSDK.GoProWrapper;
import com.infinitetakes.stream.videoSDK.PreviewSurface;


public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    TextView mStatusText;
    TextView mFrameCountText;
    TextView mCameraName;
    TextView mNeedsUpdate;
    PreviewSurface mPreviewSurface;
    GoProWrapper mWrapper;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mStatusText = (TextView) findViewById(R.id.txtStatus);
        mFrameCountText = (TextView) findViewById(R.id.txtFrameCount);
        mCameraName = (TextView) findViewById(R.id.txtName);
        mNeedsUpdate = (TextView) findViewById(R.id.txtNeedUpdate);
        mPreviewSurface = (PreviewSurface) findViewById(R.id.previewScreen);
        mWrapper = new GoProWrapper(this, getString(R.string.rtmp_url), mPreviewSurface,
                new GoProWrapper.Callbacks() {
            @Override
            public void onError(Error e) {
                Log.e(TAG, e.getMessage());
            }

            @Override
            public void onFoundGoPro(String name) {
                mCameraName.setText(name);
            }
        });
    }


    @Override
    protected void onResume() {
        super.onResume();
        mWrapper.beginStreaming();
    }
}
