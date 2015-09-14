package stream.infinitetakes.com.testgopro2;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.widget.TextView;

import com.infinitetakes.stream.videoSDK.FFmpegWrapper;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.InetSocketAddress;
import java.net.SocketException;
import java.net.URL;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {
    TextView mStatusText;
    TextView mFrameCountText;

    //  State Variables
    boolean mIsCameraOn = false;
    boolean mNetworksReady = false;
    boolean mReadingCamera = false;
    boolean mStreamingStarted = false;
    final Object mSyncObject = new Object();

    //  FFMpeg Stuff
    final FFmpegWrapper wrapperRead = new FFmpegWrapper();
    final FFmpegWrapper wrapperWrite = new FFmpegWrapper();
    int frameCountReceive = 0;
    int ptrAudioStream;
    int ptrVideoStream;

    //  Talking to the GoPro
    private static final String CAMERA_IP = "10.5.5.9";
    private static int PORT = 8554;
    private static DatagramSocket mOutgoingUdpSocket;
    private Process streamingProcess;
    private KeepAliveThread mKeepAliveThread;
    private Network network;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mStatusText = (TextView)findViewById(R.id.txtStatus);
        mFrameCountText = (TextView)findViewById(R.id.txtFrameCount);
    }


    @Override
    protected void onResume(){
        super.onResume();
        turnOnCamera();
        keepAlive();
        switchToLTE();
        startReadingGoPro();
        startStreamingFrames();
    }

    protected void turnOnCamera(){
        new Thread(new Runnable() {
            @Override
            public void run() {
                URL url;
                try {
                    /**
                     * To begin streaming, we need to send a "restart" signal to the goPro first,
                     * that will tell it to go into preview mode.
                     */
                    url = new URL("http://10.5.5.9:8080/gp/gpControl/execute?p1=gpStream&c1=restart");
                    HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
                    InputStream in = new BufferedInputStream(urlConnection.getInputStream());
                    BufferedReader inr = new BufferedReader(new InputStreamReader(in));
                    String line;
                    StringBuilder responseData = new StringBuilder();
                    while ((line = inr.readLine()) != null) {
                        responseData.append(line);
                    }
                    Log.i("Restart signal", responseData.toString());
                    urlConnection.disconnect();
                    MainActivity.this.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            mStatusText.setText("Camera is on.");
                        }
                    });
                    synchronized (mSyncObject){
                        mIsCameraOn = true;
                        mSyncObject.notify();
                    }

                } catch (IOException e) {
                    Log.e("Restart signal", "error sending signal", e);
                }
            }
        }).start();
    }

    protected void switchToLTE(){
        new Thread(new Runnable() {
            @Override
            public void run() {
                synchronized (mSyncObject){
                    while(!mIsCameraOn){
                        try {
                            mSyncObject.wait(300);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                }
                final ConnectivityManager cm = (ConnectivityManager)
                        MainActivity.this.getSystemService(Context.CONNECTIVITY_SERVICE);
                NetworkRequest.Builder req = new NetworkRequest.Builder();
                req.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
                req.addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);
                NetworkRequest netreq = req.build();
                ConnectivityManager.NetworkCallback callback = new ConnectivityManager.NetworkCallback() {
                    @Override
                    public void onAvailable(final Network network) {
                        if(!mNetworksReady){
                            ConnectivityManager.setProcessDefaultNetwork(network);
                            MainActivity.this.network = network;
                            MainActivity.this.runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    synchronized (mSyncObject){
                                        mNetworksReady = true;
                                        mSyncObject.notify();
                                    }
                                    mStatusText.setText("Both networks ready.");
                                }
                            });
                        }
                    }
                };
                cm.requestNetwork(netreq, callback);
                cm.registerNetworkCallback(netreq, callback);
            }
        }).start();
    }

    protected void startReadingGoPro(){
        new Thread(new Runnable() {
            @Override
            public void run() {
                synchronized (mSyncObject){
                    while(!mNetworksReady){
                        try {
                            mSyncObject.wait(300);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                }
                wrapperRead.setCallback(new FFmpegWrapper.Callbacks() {
                    @Override
                    public void onConnectionDropped() {

                    }

                    @Override
                    public void onConnectionRestored() {

                    }

                    @Override
                    public void onReceiveGoProFrame(int ptrData, final int ptrAudioStream, final int ptrVideoStream) {
                        frameCountReceive++;
                        MainActivity.this.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (!mReadingCamera) {
                                    synchronized (mSyncObject) {
                                        MainActivity.this.ptrAudioStream = ptrAudioStream;
                                        MainActivity.this.ptrVideoStream = ptrVideoStream;
                                        mReadingCamera = true;
                                        mSyncObject.notify();
                                    }
                                    mStatusText.setText("Camera frames started.");
                                }
                                mFrameCountText.setText(Integer.toString(frameCountReceive));
                            }
                        });
                        if (mStreamingStarted) {
                            wrapperWrite.writeGoProFrame(ptrData);
                        }
                    }
                });
                wrapperRead.startGoPro();
            }
        }).start();
    }

    protected void startStreamingFrames(){
        new Thread(new Runnable() {
            @Override
            public void run() {
                synchronized (mSyncObject){
                    while(!mReadingCamera){
                        try {
                            mSyncObject.wait(300);
                        } catch (InterruptedException e) {
                            e.printStackTrace();
                        }
                    }
                }
                try {
                    if(!mStreamingStarted){
                        ConnectivityManager.setProcessDefaultNetwork(network);
                        FFmpegWrapper.Metadata meta = new FFmpegWrapper.Metadata();
                        meta.outputFormatName = "flv";
                        meta.outputFile = "rtmp://media-servers-v2dev.stre.am:1935/live/rohit";
                        Log.e("ffmpeg", "right before init");
                        wrapperWrite.init(meta);
                        Log.e("ffmpeg", "right before start");
                        wrapperWrite.start(ptrAudioStream, ptrVideoStream);
                        synchronized (mSyncObject) {
                            mStreamingStarted = true;
                            mSyncObject.notify();
                        }
                    }
                } catch (Exception e) {
                    e.printStackTrace();
                }

            }
        }).start();
    }

    /**
     * Every few seconds, we need to send a UDP packet to the goPro to tell it to stay alive.
     */
    private void sendUdpCommand(int paramInt)throws IOException
    {
        Locale localLocale = Locale.US;
        Object[] arrayOfObject = new Object[4];
        arrayOfObject[0] = Integer.valueOf(0);
        arrayOfObject[1] = Integer.valueOf(0);
        arrayOfObject[2] = Integer.valueOf(paramInt);
        arrayOfObject[3] = Double.valueOf(0.0D);
        byte[] arrayOfByte = "_GPHD_:0:0:2:0.000000".getBytes();
        String str = CAMERA_IP;
        int i = PORT;
        DatagramPacket localDatagramPacket =
                new DatagramPacket(arrayOfByte, arrayOfByte.length, new InetSocketAddress(str, i));
        mOutgoingUdpSocket.send(localDatagramPacket);
    }

    private void keepAlive() {
        mKeepAliveThread = new KeepAliveThread();
        mKeepAliveThread.start();
    }

    class KeepAliveThread extends Thread {
        public void run() {
            try {
                Thread.currentThread().setName("keepalive");
                if (mOutgoingUdpSocket == null) {
                    mOutgoingUdpSocket = new DatagramSocket();
                }
                synchronized (mSyncObject){
                    while(!mReadingCamera){
                        mSyncObject.wait(300);
                    }
                }
                while ((!Thread.currentThread().isInterrupted()) && (mOutgoingUdpSocket != null)) {
                    sendUdpCommand(2);
                    Thread.sleep(3000L);
                    System.out.println("keep alive udp");
                }
            }
            catch (SocketException e) {
                e.printStackTrace();
            }
            catch (InterruptedException e) {
                e.printStackTrace();
            }
            catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}
