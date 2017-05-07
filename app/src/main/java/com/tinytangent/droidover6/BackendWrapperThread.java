package com.tinytangent.droidover6;

/**
 * Created by tansinan on 5/4/17.
 */

public class BackendWrapperThread implements Runnable {
    protected int tunDeviceFd;
    protected int commandReadFd;
    protected int responseWriteFd;
    protected String hostName;
    protected int port;

    @Override
    public void run() {
        jniEntry(hostName, port, tunDeviceFd, commandReadFd, responseWriteFd);
    }

    BackendWrapperThread(String hostName, int port, int tunDeviceFd, int commandReadFd, int responseWriteFd)
    {
        this.hostName = hostName;
        this.port = port;
        this.tunDeviceFd = tunDeviceFd;
        this.commandReadFd = commandReadFd;
        this.responseWriteFd = responseWriteFd;
    }

    public native int jniEntry(String hostName, int port, int tunDeviceFd, int commandReadFd, int responseWriteFd);

    static {
        System.loadLibrary("native-lib");
    }
}
