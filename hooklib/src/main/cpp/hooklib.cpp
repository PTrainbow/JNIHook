#include <jni.h>
#include <string>
#include <android/log.h>
#include <sys/mman.h>
#include <unistd.h>


#define LOG(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "hook!", fmt, ##__VA_ARGS__)

jstring (*originNewString)(JNIEnv *env,  const char * obj);

jstring NewStringUTFOverride(JNIEnv *env,  const char * obj) {
    LOG("hook NewStringUTF: %s",  obj);
    return originNewString(env, obj);
}

int accessWrite(unsigned long address) {
    int page_size = getpagesize();
    // 获取 page size 整数倍的起始地址
    address -= (unsigned long)address % page_size;
    if(mprotect((void*)address, page_size, PROT_READ | PROT_WRITE) == -1) {
        LOG("mprotect failed!");
        return -1;
    }
    __builtin___clear_cache((char *)address, (char *)(address + page_size));
    return 0;
}

void hook(JNIEnv *env) {
    auto *functionTable = env->functions;
    // 指向函数指针的变量的地址
    void** funcPointerAddress = (void **)&(functionTable->NewStringUTF);
    accessWrite(reinterpret_cast<unsigned long>(funcPointerAddress));
    // 存原始指针
    originNewString = functionTable->NewStringUTF;
    // 获取新的函数指针的变量的地址
    void** newFuncPointerAddress = reinterpret_cast<void **>(&NewStringUTFOverride);
    // 替换地址
    *funcPointerAddress = newFuncPointerAddress;
}

jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    hook(env);
    return JNI_VERSION_1_6;
}