package com.tinytangent.droidover6;

/**
 * Created by tansinan on 5/4/17.
 */

public class BackendWrapperThread implements Runnable {
    protected int tunDeviceFd;
    protected int commandReadFd;
    protected int responseWriteFd;

    @Override
    public void run() {
        jniEntry(tunDeviceFd, commandReadFd, responseWriteFd);
    }

    BackendWrapperThread(int tunDeviceFd, int commandReadFd, int responseWriteFd)
    {
        this.tunDeviceFd = tunDeviceFd;
        this.commandReadFd = commandReadFd;
        this.responseWriteFd = responseWriteFd;
    }

    public native int jniEntry(int tunDeviceFd, int commandReadFd, int responseWriteFd);

    static {
        System.loadLibrary("native-lib");
    }
}
