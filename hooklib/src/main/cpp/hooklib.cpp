#include <jni.h>
#include <string>
#include <android/log.h>
#include <sys/mman.h>
#include <unistd.h>


#define LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "hook!", fmt, ##__VA_ARGS__)

jstring (*originNewString)(JNIEnv *env, const char *obj);

jstring NewStringUTFOverride(JNIEnv *env, const char *obj) {
//    LOG("hook NewStringUTF: %s",  obj);
    return originNewString(env, obj);
}


jint (*originCallStaticIntMethod)(JNIEnv *env, jclass obj, jmethodID methodId, ...);

jint CallStaticIntMethodOverride(JNIEnv *env, jclass javaclass, jmethodID methodId, void *array) {
    LOG("CallStaticIntMethodOverride hook success! just call origin method");
    return originCallStaticIntMethod(env, javaclass, methodId, array);
}

int accessWrite(unsigned long address) {
    int page_size = getpagesize();
    // 获取 page size 整数倍的起始地址
    address -= (unsigned long) address % page_size;
    if (mprotect((void *) address, page_size, PROT_READ | PROT_WRITE) == -1) {
        LOG("mprotect failed!");
        return -1;
    }
    __builtin___clear_cache((char *) address, (char *) (address + page_size));
    return 0;
}

void hookStaticIntMethod(JNIEnv *env) {
    auto *functionTable = env->functions;
    void **funcPointerAddress = (void **) &(functionTable->CallStaticIntMethod);
    accessWrite(reinterpret_cast<unsigned long>(funcPointerAddress));
    originCallStaticIntMethod = functionTable->CallStaticIntMethod;
    void **newFuncPointerAddress = reinterpret_cast<void **>(&CallStaticIntMethodOverride);
    *funcPointerAddress = newFuncPointerAddress;

    // Test
    jclass testClazz = env->FindClass("com/zzy/hooklib/TestJava");
    jclass stringClazz = env->FindClass("java/lang/String");

    jmethodID method1 = env->GetStaticMethodID(testClazz, "method", "([I)I");
    jmethodID method2 = env->GetStaticMethodID(testClazz, "method2", "([Ljava/lang/String;)I");

    jintArray array = env->NewIntArray(4);
    jint buf[4];
    buf[0] = 0;
    buf[1] = 1;
    buf[2] = 2;
    buf[3] = 3;
    env->SetIntArrayRegion(array, 0, 4, buf);


    LOG("call int array param hook start");
    functionTable->CallStaticIntMethod(env, testClazz, method1, array);
    LOG("call int array param hook end");

    LOG("\n");


    LOG("call origin int array param start");
    originCallStaticIntMethod(env, testClazz, method1, array);
    LOG("call origin int array param end");

    LOG("\n");

    jobjectArray stringArray = env->NewObjectArray(4, stringClazz, env->NewStringUTF("test"));

    LOG("call string array param hook start");
    functionTable->CallStaticIntMethod(env, testClazz, method2, stringArray);
    LOG("call string array param hook end");

    LOG("\n");

    LOG("call origin string array param start");
    originCallStaticIntMethod(env, testClazz, method2, stringArray);
    LOG("call origin string array param end");

    LOG("\n");

}

void hook(JNIEnv *env) {
    auto *functionTable = env->functions;
    // 指向函数指针的变量的地址
    void **funcPointerAddress = (void **) &(functionTable->NewStringUTF);
    accessWrite(reinterpret_cast<unsigned long>(funcPointerAddress));
    // 存原始指针
    originNewString = functionTable->NewStringUTF;
    // 获取新的函数指针的变量的地址
    void **newFuncPointerAddress = reinterpret_cast<void **>(&NewStringUTFOverride);
    // 替换地址
    *funcPointerAddress = newFuncPointerAddress;
}

jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }
    hook(env);
    hookStaticIntMethod(env);
    return JNI_VERSION_1_6;
}