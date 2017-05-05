//
// Created by tansinan on 5/4/17.
//

#include <jni.h>
#include <string>
#include "backend_main.h"

extern "C"
JNIEXPORT jstring JNICALL
Java_com_tinytangent_droidover6_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from JNI";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_tinytangent_droidover6_BackendWrapperThread_jniEntry(
        JNIEnv* env,
        jobject /* this */, jint tunDeviceFd, jint commandReadFd, jint responseWriteFd) {
    return backend_main(tunDeviceFd, commandReadFd, responseWriteFd);
}
