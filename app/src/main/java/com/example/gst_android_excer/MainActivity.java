package com.example.gst_android_excer;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.NetworkInfo;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pGroup;
import android.net.wifi.p2p.WifiP2pInfo;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;
import android.widget.ToggleButton;

import org.freedesktop.gstreamer.GStreamer;

import java.util.Objects;

public class MainActivity extends AppCompatActivity {
    private native String gst_version_from_jni();

    private native void element_list();

    private native void nativeInit(String pipeline);

    private native void nativeFinalize();

    private static native boolean nativeClassInit();

    private native void nativePaused();

    private native void nativePlay();

    private native void nativeSurfaceInit(Object surface);

    private native void nativeSurfaceFinalize();


    private long native_custom_data;      // Native code will use this to keep private data

    String[] neededPermissions = new String[]{
            Manifest.permission.CAMERA,
            Manifest.permission.INTERNET,
            Manifest.permission.ACCESS_NETWORK_STATE,
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_WIFI_STATE,
            Manifest.permission.CHANGE_WIFI_STATE,
            Manifest.permission.READ_EXTERNAL_STORAGE
    };
    static final int PERMISSION_REQUEST_CODE = 0x1000;

    static public String TAG = "MYTAG";
    SurfaceView surfaceview;
    ToggleButton tglbtn_action2;
    EditText edit_gstpipeline;

    WifiP2pManager.Channel p2pChannel;
    WifiP2pManager p2pManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Toast.makeText(this, "GStreamer Not Initialized", Toast.LENGTH_SHORT).show();
            e.printStackTrace();
            finish();
        }
        setContentView(R.layout.activity_main);

        tglbtn_action2 = findViewById(R.id.tglbtn_action2);
        Objects.requireNonNull(getSupportActionBar()).setTitle(gst_version_from_jni());
        edit_gstpipeline = findViewById(R.id.edit_gstpipeline);

        checkpermission();

        surfaceview = findViewById(R.id.surfaceview);
        SurfaceHolder surfaceHolder = surfaceview.getHolder();
        surfaceHolder.addCallback(shcallback);

        element_list();
        nativeInit("videotestsrc ! autovideosink");
    }

    void checkpermission() {
        if (!permission_check_each())
            requestPermissions(neededPermissions, PERMISSION_REQUEST_CODE);
        else initial_process();
    }

    boolean permission_check_each() {
        for (String each : neededPermissions)
            if (checkSelfPermission(each) != PackageManager.PERMISSION_GRANTED)
                return false;
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            boolean grant = true;
            for (int n : grantResults)
                if (n != PackageManager.PERMISSION_GRANTED) {
                    grant = false;
                    break;
                }
            if (!grant)
                finish();
            else
                initial_process();
        }
    }

    void initial_process() {
        Log.d(TAG, "All Permission Granted");

        p2pManager = (WifiP2pManager) getSystemService(Context.WIFI_P2P_SERVICE);
        p2pChannel = p2pManager.initialize(this, getMainLooper(), null);

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            Toast.makeText(this, "permission issue", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        p2pManager.requestGroupInfo(p2pChannel, new WifiP2pManager.GroupInfoListener() {
            @Override
            public void onGroupInfoAvailable(WifiP2pGroup wifiP2pGroup) {
                if (wifiP2pGroup == null) return;
                for (WifiP2pDevice device : wifiP2pGroup.getClientList()) {
                    Log.d(TAG, "clients : " + device.deviceName + ", " + device.deviceAddress);
                }
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION);
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION);
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION);
        intentFilter.addAction(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION);
//        intentFilter.addAction(WifiP2pManager.WIFI_P2P_STATE_CHANGED_ACTION);
        registerReceiver(broadcastReceiver, intentFilter);
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();
        nativeFinalize();
    }

    private void onGStreamerInitialized() {
        Log.d(TAG, "onGStreamerInitialized");
        nativePlay();
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(MainActivity.this, "Entered Main Loop", Toast.LENGTH_SHORT).show();
            }
        });
    }

    int pattern = 0;
    public void OnBtnClick(View v) {
        int id = v.getId();
        if (id == R.id.btn_set_state_play) {
            nativePlay();
        } else if (id == R.id.btn_set_state_stop) {
            nativePaused();
        } else if (id == R.id.btn_action1) {
            if(!edit_gstpipeline.getEditableText().toString().isEmpty()) {
                nativeFinalize();
                nativeInit(edit_gstpipeline.getEditableText().toString());
                nativeSurfaceInit(surfaceview.getHolder().getSurface());
                edit_gstpipeline.setText("");
            }
        } else if (id == R.id.tglbtn_action2) {
            if (((ToggleButton) tglbtn_action2).isChecked()) {
                wifi_direct_discovery_start();
            } else {
                wifi_direct_discovery_stop();
            }
        }
    }

    void wifi_direct_discovery_start() {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "request permissions before action");
            requestPermissions(neededPermissions, PERMISSION_REQUEST_CODE);
            return;
        }
        p2pManager.discoverPeers(p2pChannel, new WifiP2pManager.ActionListener() {
            @Override
            public void onSuccess() {
                Log.d(TAG, "discoverpeers successed");
                tglbtn_action2.setChecked(true);
            }

            @Override
            public void onFailure(int i) {
                tglbtn_action2.setChecked(false);
            }
        });

    }

    void wifi_direct_discovery_stop() {
        p2pManager.stopPeerDiscovery(p2pChannel, new WifiP2pManager.ActionListener() {
            @Override
            public void onSuccess() {
                Log.d(TAG, "stop discoverpeers successed");
                tglbtn_action2.setChecked(false);
            }
            @Override
            public void onFailure(int i) {
                tglbtn_action2.setChecked(true);
            }
        });
    }

    SurfaceHolder.Callback shcallback = new SurfaceHolder.Callback() {

        @Override
        public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int format, int width, int height) {
            Log.d(TAG, "surfaceChanged");
            nativeSurfaceInit(surfaceHolder.getSurface());
        }

        @Override
        public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
            Log.d(TAG, "surfaceCreated");
        }

        @Override
        public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
            Log.d(TAG, "surfaceDestroyed");
            nativeSurfaceFinalize();
        }
    };


    BroadcastReceiver broadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(WifiP2pManager.WIFI_P2P_PEERS_CHANGED_ACTION)) {
                Log.d(TAG, "WIFI_P2P_PEERS_CHANGED_ACTION");
            }
            else if (intent.getAction().equals(WifiP2pManager.WIFI_P2P_CONNECTION_CHANGED_ACTION)) {
                NetworkInfo networkInfo = intent.getParcelableExtra(WifiP2pManager.EXTRA_NETWORK_INFO);
                assert networkInfo != null;
                if (networkInfo.isConnected()) {
                    p2pManager.requestConnectionInfo(p2pChannel, new WifiP2pManager.ConnectionInfoListener() {
                        @Override
                        public void onConnectionInfoAvailable(WifiP2pInfo wifiP2pInfo) {
                            Log.d(TAG, wifiP2pInfo.isGroupOwner ? "owner - " : "none-owner - " + wifiP2pInfo.groupOwnerAddress.getHostName());
                        }
                    });
                    if (ActivityCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) return;
                    p2pManager.requestGroupInfo(p2pChannel, new WifiP2pManager.GroupInfoListener() {
                        @Override
                        public void onGroupInfoAvailable(WifiP2pGroup wifiP2pGroup) {
                            for(WifiP2pDevice device : wifiP2pGroup.getClientList())
                                Log.d(TAG, "device list : " + device.deviceAddress);
                        }
                    });
                }
            }
            else if (intent.getAction().equals(WifiP2pManager.WIFI_P2P_THIS_DEVICE_CHANGED_ACTION)) {
                WifiP2pDevice device = (WifiP2pDevice) intent.getParcelableExtra(WifiP2pManager.EXTRA_WIFI_P2P_DEVICE);
                assert device != null;
                switch (device.status) {
                    case WifiP2pDevice.AVAILABLE:
                        Log.d(TAG, "WIFI_P2P_THIS_DEVICE_CHANGED_ACTION - AVAILABLE");
                        break;
                    case WifiP2pDevice.INVITED:
                        Log.d(TAG, "WIFI_P2P_THIS_DEVICE_CHANGED_ACTION - INVITED");
                        break;
                    case WifiP2pDevice.CONNECTED:
                        Log.d(TAG, "WIFI_P2P_THIS_DEVICE_CHANGED_ACTION - CONNECTED");
                        break;
                    case WifiP2pDevice.FAILED:
                        Log.d(TAG, "WIFI_P2P_THIS_DEVICE_CHANGED_ACTION - FAILED");
                        break;
                    case WifiP2pDevice.UNAVAILABLE:
                        Log.d(TAG, "WIFI_P2P_THIS_DEVICE_CHANGED_ACTION - UNAVAILABLE");
                        break;
                    default:
                        break;
                }
            }
            else if (intent.getAction().equals(WifiP2pManager.WIFI_P2P_DISCOVERY_CHANGED_ACTION)) {
                Log.d(TAG, "WIFI_P2P_DISCOVERY_CHANGED_ACTION");
            }
        }
    };


    static {
        System.loadLibrary("gst-worker");
        System.loadLibrary("gstreamer_android");
        nativeClassInit();
    }
}