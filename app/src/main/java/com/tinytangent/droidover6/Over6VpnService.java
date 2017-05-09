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
    static CountDownTimer timer = null;
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
        timer = new CountDownTimer(1000000, 1000) {
            int i = 0;

            public void onTick(long millisUntilFinished) {

                if(vpnInterface == null)
                {
                    byte[] data = new byte[20];
                    try {
                        commandStream.write(BACKEND_IPC_COMMAND_CONFIGURATION);
                        int bytesRead = 0;
                        while(bytesRead < data.length)
                        {
                            int ret = responseStream.read(data, bytesRead, data.length - bytesRead);
                            if(ret > 0)
                                bytesRead += ret;
                        }
                    }
                    catch (IOException e) {
                        Log.d("DroidOver6 VPN", "IO error");
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
                                //.setConfigureIntent(pendingIntent)
                                .establish();
                        commandStream.write(BACKEND_IPC_COMMAND_SET_TUNNEL_FD);
                        commandStream.write(ByteBuffer.allocate(4).putInt(vpnInterface.getFd()).array());
                    } catch (UnknownHostException e) {
                        //This is impossible.
                    } catch (IllegalArgumentException e) {
                        //TODO: handle illegal IP/DNS Configuration.
                    }
                    catch (IOException e) {

                    }
                }
                else {
                    try {
                        commandStream.write(0);
                        commandStream.flush();
                        int response = responseStream.read();
                        Intent intent = new Intent(BROADCAST_VPN_STATE);
                        intent.putExtra("status_code", response);
                        LocalBroadcastManager.getInstance(Over6VpnService.this).sendBroadcast(intent);
                        if(response == BackendIPC.BACKEND_STATE_DISCONNECTED)
                        {
                            Over6VpnService.this.reliableStop();
                        }
                    } catch (Exception e) {
                        return;
                    }
                }
            }

            public void onFinish() {
            }
        };
        timer.start();
        return START_REDELIVER_INTENT;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    public void reliableStop() {
        try {
            commandStream.write(BACKEND_IPC_COMMAND_TERMINATE);
            if(vpnInterface != null) {
                Over6VpnService.getInstance().vpnInterface.close();
            }
            Over6VpnService.getInstance().commandStream.close();
            Over6VpnService.getInstance().responseStream.close();
            timer.cancel();
        } catch (Exception e) {

        }
        Over6VpnService.getInstance().stopSelf();
    }
}
