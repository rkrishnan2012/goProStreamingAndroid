package stream.infinitetakes.com.testgopro2;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.widget.TextView;

import com.infinitetakes.stream.videoSDK.GoProWrapper;
import com.infinitetakes.stream.videoSDK.PreviewSurface;

public class MainActivity extends AppCompatActivity {
    TextView mCameraName;
    TextView mNeedsUpdate;
    PreviewSurface mPreviewSurface;

    GoProWrapper wrapper;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mCameraName = (TextView)findViewById(R.id.txtName);
        mNeedsUpdate = (TextView)findViewById(R.id.txtNeedUpdate);
        mPreviewSurface = (PreviewSurface)findViewById(R.id.previewScreen);
    }


    @Override
    protected void onResume(){
        super.onResume();
        wrapper = new GoProWrapper(this, getString(R.string.rtmp_url), mPreviewSurface, new GoProWrapper.Callbacks(){
            @Override
            public void onError(Error e) {
            }

            @Override
            public void onFoundGoPro(String name) {
                mCameraName.setText(name);
            }
        });
        wrapper.beginStreaming();
    }
}