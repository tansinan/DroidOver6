package com.tinytangent.droidover6;

import android.app.ActivityManager;
import android.content.Context;

/**
 * Created by tansinan on 5/8/17.
 */

public class ServiceUtil {
    // The code is based on http://stackoverflow.com/questions/600207/how-to-check-if-a-service-is-running-on-android
    public static boolean isServiceRunning(Context context, Class<?> serviceClass) {
        ActivityManager activityManager =
                (ActivityManager)context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.RunningServiceInfo serviceInfo :
                activityManager.getRunningServices(Integer.MAX_VALUE)) {
            if (serviceClass.getName().equals(serviceInfo.service.getClassName())) {
                return true;
            }
        }
        return false;
    }
}
