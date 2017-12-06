
#include <iostream>
#include "native-lib.h"



extern "C"
JNIEXPORT jstring JNICALL
Java_com_uxfac_koreapassing_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    int r;
    ssize_t cnt;
//    fxload();
    return env->NewStringUTF(hello.c_str());
}
