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
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.channels.Pipe;
import java.util.Arrays;

/**
 * Created by tansinan on 5/4/17.
 */

public class Over6VpnService extends VpnService {
    protected static byte BACKEND_IPC_COMMAND_STATUS = (byte)0x00;
    protected static byte BACKEND_IPC_COMMAND_STATISTICS = (byte)0x01;
    protected static byte BACKEND_IPC_COMMAND_CONFIGURATION = (byte)0x02;
    protected static byte BACKEND_IPC_COMMAND_SET_TUNNEL_FD = (byte)0x03;
    protected static byte BACKEND_IPC_COMMAND_TERMINATE = (byte)0xFF;
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
    public static final String BROADCAST_VPN_STATE = "com.tinytangent.droidover6.STATUS_CHANGED";
    protected ParcelFileDescriptor vpnInterface = null;
    protected PendingIntent pendingIntent;
    static protected Over6VpnService instance = null;
    static Over6VpnService getInstance()
    {
        return instance;
    }

    @Override
    public void onCreate() {
        instance = this;
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
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        super.onCreate();
        //LocalBroadcastManager.getInstance(this).sendBroadcast(new Intent(BROADCAST_VPN_STATE).putExtra("running", true));
        commandStream = new FileOutputStream(commandWriteFd.getFileDescriptor());
        responseStream = new FileInputStream(responseReadFd.getFileDescriptor());
        String hostName = intent.getStringExtra("host_name");
        int port = intent.getIntExtra("port", 0);
        backendThread = new Thread(new BackendWrapperThread(
            hostName, port,
            -1,
            commandReadFd.getFd(),
            responseWriteFd.getFd()
        ));
        backendThread.start();
        Log.d("Backend", "Backend started!");
        new CountDownTimer(100000, 1000) {
            int i = 0;

            public void onTick(long millisUntilFinished) {

                if(vpnInterface == null)
                {
                    byte[] data = new byte[20];
                    try {
                        commandStream.write(BACKEND_IPC_COMMAND_CONFIGURATION);
                        responseStream.read(data);
                    }
                    catch (IOException e) {
                        return;
                    }
                    try {
                        InetAddress address = InetAddress.getByAddress(Arrays.copyOfRange(data, 0, 4));
                        InetAddress dns = InetAddress.getByAddress(Arrays.copyOfRange(data, 8, 12));
                        vpnInterface = new Builder()
                                .addAddress(address, 24)
                                .addDnsServer(dns)
                                .addRoute("0.0.0.0", 0)
                                .addRoute("2400:cb00:2049:1::adf5:3b47", 128)
                                .setSession(getString(R.string.app_name))
                                .setConfigureIntent(pendingIntent)
                                .establish();
                        commandStream.write(BACKEND_IPC_COMMAND_SET_TUNNEL_FD);
                        commandStream.write(ByteBuffer.allocate(4).putInt(vpnInterface.getFd()).array());
                    } catch (UnknownHostException e) {
                        //This is impossible.
                    } catch (IOException e) {

                    }
                }
                else {
                    try {
                        commandStream.write(0);
                        commandStream.flush();
                    } catch (IOException e) {
                        return;
                    }
                    byte[] response = new byte[200];
                    try {
                        int temp = 0;
                        //int temp = responseStream.read(response);
                        Log.d("Backend", new String(Arrays.copyOfRange(response, 0, temp)));
                        Log.d("Backend", "" + temp);
                        Intent intent = new Intent(BROADCAST_VPN_STATE);
                        intent.putExtra("data", "Notice me " + i + "!");
                        LocalBroadcastManager.getInstance(Over6VpnService.this).sendBroadcast(intent);
                    } catch (Exception e) {
                        return;
                    }
                }
            }

            public void onFinish() {
            }
        }.start();
        return START_REDELIVER_INTENT;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    public void reliableStop() {
        try {
            commandStream.write(BACKEND_IPC_COMMAND_TERMINATE);
            Over6VpnService.getInstance().vpnInterface.close();
            Over6VpnService.getInstance().commandStream.close();
            Over6VpnService.getInstance().responseStream.close();
        } catch (Exception e){}
        Over6VpnService.getInstance().stopSelf();
    }
}
