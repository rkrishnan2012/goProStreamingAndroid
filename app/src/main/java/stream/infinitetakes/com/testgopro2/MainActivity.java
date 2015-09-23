package stream.infinitetakes.com.testgopro2;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import com.infinitetakes.stream.videoSDK.GoProWrapper;
import com.infinitetakes.stream.videoSDK.PreviewSurface;

public class MainActivity extends Activity {
    TextView mCameraName;
    TextView mNeedsUpdate;
    Button mStartButton;
    PreviewSurface mPreviewSurface;
    GoProWrapper wrapper = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.content_main);
        mCameraName = (TextView)findViewById(R.id.txtName);
        mNeedsUpdate = (TextView)findViewById(R.id.txtNeedUpdate);
        mPreviewSurface = (PreviewSurface)findViewById(R.id.previewScreen);
        mStartButton = (Button)findViewById(R.id.btnStart);
        mStartButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if(wrapper.isStreaming()){
                    wrapper.pauseStreaming();
                    mStartButton.setText("Start Streaming");
                }
                else{
                    wrapper.beginStreaming();
                    mStartButton.setText("Stop Streaming");
                }
            }
        });
    }


    @Override
    protected void onResume(){
        super.onResume();
        if(wrapper == null){
            wrapper = new GoProWrapper(this, getString(R.string.rtmp_url), mPreviewSurface, new GoProWrapper.Callbacks(){
                @Override
                public void onError(Error e) {
                }

                @Override
                public void onFoundGoPro(String name) {
                    mCameraName.setText(name);
                    mStartButton.setVisibility(View.VISIBLE);
                }
            });
            wrapper.connect();
        }
    }
}