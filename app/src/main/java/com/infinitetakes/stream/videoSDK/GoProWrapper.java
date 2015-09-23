package com.infinitetakes.stream.videoSDK;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.math.BigInteger;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.URL;
import java.net.UnknownHostException;
import java.nio.ByteOrder;

import stream.infinitetakes.com.testgopro2.R;

/**
 * This is a wrapper for communication, streaming, and encoding of the GoPro.
 * <p/>
 * Tested Devices:
 * -GoPro Hero 4
 */
@SuppressWarnings("ThrowableInstanceNeverThrown")
public class GoProWrapper {
    private static final String TAG = "GoProWrapper";
    private static final double MIN_GOPRO_VERSION = 3;

    //  Errors that we will throw from Callbacks
    public static Error NO_WIFI = new Error("Your phone is not conencted to a wifi network.");
    public static Error NOT_GOPRO = new Error("Your phone is not connected to the GoPro wifi network.");
    public static Error IN_SETTINGS = new Error("Streaming does not work when GoPro is in settings page.");
    public static Error UPDATE_GOPRO = new Error("You need to update your GoPro to version " +
            MIN_GOPRO_VERSION + " in order to stream.");
    //  State Variables
    boolean mIsCameraOn = false;
    boolean mNetworksReady = false;
    boolean mReadingCamera = false;
    boolean mStreamingStarted = false;
    boolean mIsError = false;
    final Object mSyncObject = new Object();

    //  Misc variables
    SearchThread mSearchThread = null;
    KeepAliveThread mKeepAliveThread = null;
    Context mContext;
    PreviewSurface mSurface = null;
    Callbacks mCallbacks = null;
    private Network network;
    protected GoProC mReadProcess = new GoProC();
    protected GoProC mWriteProcess = new GoProC();
    int ptrAudioStream;
    int ptrVideoStream;
    int mFramesRx = 0;
    int mFramesTx = 0;
    String mUrl;


    public GoProWrapper(Context context, String url, PreviewSurface surfaceView, Callbacks callbacks) {
        this.mContext = context;
        this.mSurface = surfaceView;
        this.mCallbacks = callbacks;
        this.mUrl = url;
        surfaceView.setNativeWrapper(this);
    }

    /**
     * These are callbacks fired from GoProWrapper to external Java code.
     */
    public interface Callbacks {
        void onError(Error e);

        void onFoundGoPro(String name);
    }

    /**
     * Public API methods.
     */
    public void startSearchGoPro() {
        if (mSearchThread == null || mSearchThread.isShutdown()) {
            mSearchThread = new SearchThread(mContext);
            Thread newThread = new Thread(mSearchThread);
            newThread.start();
        }
    }

    public void stopSearchGoPro() {
        if (mSearchThread != null) {
            mSearchThread.stop();
        }
    }

    public void beginStreaming() {
        stopSearchGoPro();
        turnOnCamera();
        keepAlive();
        switchToLTE();
        startReadingGoPro();
        startStreamingFrames();
    }

    protected void switchToLTE(){
        new Thread(new Runnable() {
            @Override
            public void run() {
                final ConnectivityManager cm = (ConnectivityManager)
                        mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
                NetworkRequest.Builder req = new NetworkRequest.Builder();
                req.addCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET);
                req.addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);
                NetworkRequest netreq = req.build();
                ConnectivityManager.NetworkCallback callback = new ConnectivityManager.NetworkCallback() {
                    @Override
                    public void onAvailable(final Network network) {
                        if(!mNetworksReady){
                            ConnectivityManager.setProcessDefaultNetwork(network);
                            GoProWrapper.this.network = network;
                            synchronized (mSyncObject){
                                mNetworksReady = true;
                                mSyncObject.notify();
                            }
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
                mReadProcess.setCallback(new GoProC.InternalCallbacks() {
                    @Override
                    public void onConnectionDropped() {

                    }

                    @Override
                    public void onConnectionRestored() {

                    }

                    @Override
                    public void onReceiveGoProFrame(int ptrData, final int ptrAudioStream, final int ptrVideoStream) {
                        mFramesRx++;
                        if (!mReadingCamera) {
                            synchronized (mSyncObject) {
                                GoProWrapper.this.ptrAudioStream = ptrAudioStream;
                                GoProWrapper.this.ptrVideoStream = ptrVideoStream;
                                mReadingCamera = true;
                                mSyncObject.notify();
                            }
                        }
                        if (mStreamingStarted) {
                            mWriteProcess.writeFrame(ptrData);
                        }
                    }
                });
                mReadProcess.startReading();
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
                        GoProWrapper.Metadata meta = new GoProWrapper.Metadata();
                        meta.outputFormatName = "flv";
                        meta.outputFile = mContext.getString(R.string.rtmp_url);
                        mWriteProcess.init(meta);
                        mWriteProcess.startWriting(ptrAudioStream, ptrVideoStream);
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

    private void turnOnCamera() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                URL url;
                try {
                    // Sanity check to see if we're connected to wifi and the GoPro
                    WifiManager wm = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
                    WifiInfo wi = wm.getConnectionInfo();
                    SupplicantState supState = wi.getSupplicantState();
                    String ip = getLocalIpAddress();
                    if (ip == null || supState.equals(SupplicantState.DISCONNECTED)
                            || supState.equals(SupplicantState.UNINITIALIZED)
                            || supState.equals(SupplicantState.INACTIVE)
                            || supState.equals(SupplicantState.INTERFACE_DISABLED)) {
                        mIsError = true;
                        new Handler(Looper.getMainLooper()).post(new Runnable() {
                            @Override
                            public void run() {
                                mCallbacks.onError(NO_WIFI);
                            }
                        });
                    } else if (!ip.startsWith("10.5.5")) {
                        mIsError = true;
                        new Handler(Looper.getMainLooper()).post(new Runnable() {
                            @Override
                            public void run() {
                                mCallbacks.onError(NOT_GOPRO);
                            }
                        });
                    } else {
                        //  Check the name and version of the GoPro connected.
                        url = new URL(mContext.getString(R.string.gopro_info_url));
                        HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
                        InputStream in = new BufferedInputStream(urlConnection.getInputStream());
                        BufferedReader inr = new BufferedReader(new InputStreamReader(in));
                        final StringBuilder cameraName = new StringBuilder();
                        String line;
                        while ((line = inr.readLine()) != null) {
                            cameraName.append(line);
                        }
                        urlConnection.disconnect();
                        if(mSearchThread == null || !mSearchThread.foundGoPro){
                            new Handler(Looper.getMainLooper()).post(new Runnable() {
                                @Override
                                public void run() {
                                    new Handler(Looper.getMainLooper()).post(new Runnable() {
                                        @Override
                                        public void run() {
                                            mCallbacks.onFoundGoPro(cameraName.toString());
                                        }
                                    });
                                }
                            });
                        }
                        Log.d(TAG, "Found camera " + cameraName);
                        if (!cameraName.toString().contains("HERO4 Session") && Double.parseDouble(cameraName.toString().split("\\.")[2]) < MIN_GOPRO_VERSION) {
                            mIsError = true;
                            new Handler(Looper.getMainLooper()).post(new Runnable() {
                                @Override
                                public void run() {
                                    mCallbacks.onError(UPDATE_GOPRO);
                                }
                            });
                        } else {
                            //  Send a restart signal to the GoPro before we begin streaming.
                            url = new URL(mContext.getString(R.string.gopro_restart_url));
                            urlConnection = (HttpURLConnection) url.openConnection();
                            in = new BufferedInputStream(urlConnection.getInputStream());
                            inr = new BufferedReader(new InputStreamReader(in));
                            String signalResponse = inr.readLine();
                            urlConnection.disconnect();
                            Log.i("Restart signal", signalResponse);
                            mIsCameraOn = true;
                        }
                    }
                } catch (IOException e) {
                    mIsError = true;
                    Log.e("GoProWrapper", "Error connecting to the GoPro", e);
                } finally {
                    synchronized (mSyncObject) {
                        mSyncObject.notify();
                    }
                }
            }
        }).start();
    }



    public static boolean isConnected(Context context) {
        ConnectivityManager cm = (ConnectivityManager)context
                .getSystemService(Context.CONNECTIVITY_SERVICE);

        NetworkInfo activeNetwork = cm.getActiveNetworkInfo();
        if (activeNetwork != null && activeNetwork.isConnected()) {
            try {
                URL url = new URL("http://www.google.com/");
                HttpURLConnection urlc = (HttpURLConnection)url.openConnection();
                urlc.setRequestProperty("User-Agent", "test");
                urlc.setRequestProperty("Connection", "close");
                urlc.setConnectTimeout(1000); // mTimeout is in seconds
                urlc.connect();
                if (urlc.getResponseCode() == 200) {
                    return true;
                } else {
                    return false;
                }
            } catch (IOException e) {
                Log.i("warning", "Error checking internet connection", e);
                return false;
            }
        }

        return false;

    }

    private void keepAlive() {
        if(mKeepAliveThread == null || mKeepAliveThread.isInterrupted()){
            mKeepAliveThread = new KeepAliveThread();
            mKeepAliveThread.start();
        }
    }



    private String getLocalIpAddress() {
        WifiManager wifiManager = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        int ipAddress = wifiManager.getConnectionInfo().getIpAddress();
        // Convert little-endian to big-endian if needed
        if (ByteOrder.nativeOrder().equals(ByteOrder.LITTLE_ENDIAN)) {
            ipAddress = Integer.reverseBytes(ipAddress);
        }
        byte[] ipByteArray = BigInteger.valueOf(ipAddress).toByteArray();
        String ipAddressString;
        try {
            ipAddressString = InetAddress.getByAddress(ipByteArray).getHostAddress();
        } catch (UnknownHostException ex) {
            ipAddressString = null;
        }
        return ipAddressString;
    }

    class KeepAliveThread extends Thread {
        private DatagramSocket mOutgoingUdpSocket;

        private void sendUdpCommand() throws IOException {
            byte[] arrayOfByte = "_GPHD_:0:0:2:0.000000".getBytes();
            String cameraIp = "10.5.5.9";
            int port = 8554;
            DatagramPacket localDatagramPacket =
                    new DatagramPacket(arrayOfByte, arrayOfByte.length,
                            new InetSocketAddress(cameraIp, port));
            mOutgoingUdpSocket.send(localDatagramPacket);
        }

        public void run() {
            try {
                Thread.currentThread().setName("GoPro-KeepAlive");
                if (mOutgoingUdpSocket == null) {
                    mOutgoingUdpSocket = new DatagramSocket();
                }
                synchronized (mSyncObject) {
                    while (!mReadingCamera) {
                        mSyncObject.wait(300);
                    }
                }
                while ((!Thread.currentThread().isInterrupted()) && (mOutgoingUdpSocket != null)) {
                    sendUdpCommand();
                    Thread.sleep(3000L);
                    System.out.println("keep alive udp");
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    /**
     * Simple thread the silently searches for a GoPro in the background every few ms and keeps
     * track of if one was found.
     */
    private class SearchThread implements Runnable {
        private volatile boolean shutdown = false;
        public boolean foundGoPro = false;
        private Context context;

        public SearchThread(Context context) {
            this.context = context;
        }

        @Override
        public void run() {
            try {
                while (!shutdown) {
                    foundGoPro = checkConnected();
                    if (foundGoPro) {
                        stop();
                    } else {
                        Thread.sleep(300);
                    }
                }
            } catch (InterruptedException ignored) {
                stop();
                Thread.interrupted();
            }
        }

        public void stop() {
            shutdown = true;
        }

        public boolean isShutdown() {
            return shutdown;
        }

        private boolean checkConnected() {
            // Sanity check to see if we're connected to wifi and the GoPro
            String ip = getLocalIpAddress();
            if (ip == null || ip.startsWith("10.5.5")) {
                try {
                    //  Let's try to get the name of the GoPro device that's connected
                    URL url = new URL(context.getString(R.string.gopro_info_url));
                    HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
                    InputStream in = new BufferedInputStream(urlConnection.getInputStream());
                    BufferedReader inr = new BufferedReader(new InputStreamReader(in));
                    String line;
                    final StringBuilder cameraName = new StringBuilder();
                    while ((line = inr.readLine()) != null) {
                        cameraName.append(line);
                    }
                    Log.i("Found GoPro: ", cameraName.toString());
                } catch (IOException ignored) {
                    return false;
                }
            }
            return false;
        }
    }

    @SuppressWarnings("unused")
    public static class Metadata {
        //  Output codec options (Configured specific to the GoPro, don't change this)
        public int videoWidth = 432;
        public int videoHeight = 240;
        public int videoBitrate = 4000000;
        public int audioSampleRate = 96000;
        public int numAudioChannels = 2;
        //  Format options
        public String outputFormatName = "flv";
        public String outputFile;
    }
}
