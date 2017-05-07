package com.tinytangent.droidover6;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.VpnService;
import android.os.CountDownTimer;
import android.os.ParcelFileDescriptor;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.channels.Pipe;
import java.util.Arrays;

/**
 * Created by tansinan on 5/4/17.
 */

class BackendWrapperVpnService extends VpnService {
    ParcelFileDescriptor commandReadFd;
    ParcelFileDescriptor commandWriteFd;
    ParcelFileDescriptor responseReadFd;
    ParcelFileDescriptor responseWriteFd;
    Pipe communicationPipe = null;
    Pipe.SourceChannel commandChannel = null;
    Pipe.SinkChannel responseChannel = null;
    FileOutputStream commandStream = null;
    FileInputStream responseStream = null;
    Thread backendThread = null;
    protected static final String VPN_ADDRESS = "10.10.10.2";
    protected static final String VPN_ROUTE = "0.0.0.0";
    public static final String BROADCAST_VPN_STATE = "com.tinytangent.droidover6.VPN_STATE";
    protected ParcelFileDescriptor vpnInterface = null;
    protected PendingIntent pendingIntent;

    @Override
    public void onCreate() {
        super.onCreate();
        vpnInterface = new Builder()
                .addAddress(VPN_ADDRESS, 32)
                .addRoute(VPN_ROUTE, 0)
                .addRoute("2400:cb00:2049:1::adf5:3b47", 128)
                .setSession(getString(R.string.app_name))
                .setConfigureIntent(pendingIntent)
                .establish();
        try {
            ParcelFileDescriptor[] pipeFds = ParcelFileDescriptor.createPipe();
            commandWriteFd = pipeFds[1];
            commandReadFd = pipeFds[0];
            pipeFds = ParcelFileDescriptor.createPipe();
            responseWriteFd = pipeFds[1];
            responseReadFd = pipeFds[0];
            communicationPipe = Pipe.open();
            commandChannel = communicationPipe.source();
            responseChannel = communicationPipe.sink();
        }
        catch (IOException e) {
            return;
        }
        LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(BROADCAST_VPN_STATE).putExtra("running", true));
        commandStream = new FileOutputStream(commandWriteFd.getFileDescriptor());
        responseStream = new FileInputStream(responseReadFd.getFileDescriptor());
        backendThread = new Thread(new BackendWrapperThread(vpnInterface.getFd(), commandReadFd.getFd(), responseWriteFd.getFd()));
        backendThread.start();
        Log.d("Backend", "Backend started!");
        new CountDownTimer(10000, 1000) {
            int i = 0;

            public void onTick(long millisUntilFinished) {
                i++;
                Log.d("Backend", "" + i);
                byte b = (byte)i;
                try {
                    commandStream.write(b);
                    commandStream.flush();
                }
                catch (IOException e) {
                    return;
                }
                byte[] response = new byte[200];
                try {
                    int temp = responseStream.read(response);
                    Log.d("Backend", new String(Arrays.copyOfRange(response, 0, temp)));
                    Log.d("Backend", "" + temp);
                }
                catch (IOException e) {
                    return;
                }
            }

            public void onFinish() {
            }
        }.start();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }
}
