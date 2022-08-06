# JNIHook

## 简介

JNI hook 是指: hook JNIEnv 提供的众多方法

正常来说，是没有这方面的需求的。但是，对于低版本的 Android 存在一些 JNI Local Reference 的溢出，超过 512 个便会触发 crash

所以，最好有一种办法可以检测出：创建了但是没有释放的 local reference

在研究这一块内容时，发现了很多有意思的事情！


## Hook 思路

从网上查阅了一些 JNI Hook 的相关文章，似乎写这方面的人不太多，但是有幸找到了两篇，做了一些参考。

https://bbs.pediy.com/thread-200398.htm

https://blog.csdn.net/taohongtaohuyiwei/article/details/105204579

他们的基本原理都是`同一种方式`：修改 JNIEnv 提供的众多方法的内存内容(就是修改了函数指针指向的内存内容)。

因为之前我也 hook 过 pthread 相关函数，当时使用的是 xhook(bhook 或者其他 plthook)。

xhook 可以 hook 外部符号，而 pthread 的实现位于 bionic/libc.so 中，是一个外部符号，所以完全起效。

所以，一开始去寻找 jni 函数的一些符号名，可是发现自己打出来的 so，调用 env->NewStringUTF() 这种函数的符号居然是内部符号。

所以，只能采用和搜寻到的文章的相同思路，直接替换函数指针。

### 为什么是内部符号，又怎么调用成功？

当我发现像 NewStringUTF 是内部符号时，有点奇怪，这个函数的具体实现肯定是在系统 so 中，但是调用的时候又不是外部符号，那又是怎么调用成功的呢？

答案是：`JNI 直接使用的函数指针`

感兴趣可以查看 art/runtime/jni/jni_env_ext.cc 和 art/runtime/jni/jni_internal.cc

其实，你会发现 JNIEnv 其实还是依赖于 JNINativeInterface 这个结构体，其内部全是指针，我们调用的 NewStringUTF 也在其中，是一个`未被绑定的函数指针`

为什么说是未被绑定呢？这就得回头去看 JNIEnv 的创建了(jni_env_ext.cc)！

JNIEnv 是一个 `thread 绑定` 的变量，每个 thread 对应不同的 JNIEnv。但是，看了源码你就会发现，在创建之时，他会调用一个方法 GetFunctionTable()

```c
const JNINativeInterface* JNIEnvExt::GetFunctionTable(bool check_jni) {
  const JNINativeInterface* override = JNIEnvExt::table_override_;
  if (override != nullptr) {
    return override;
  }
  return check_jni ? GetCheckJniNativeInterface() : GetJniNativeInterface();
}
```
check_jni 似乎是帮助 debug 的，看代码似乎就是包含了一些检测，这块我也不太清楚。

不过不影响分析流程，无论 check_jni 是什么值，之后被调用的两个方法中，都含有 JNIEnv 提供的所有方法的实现 且 存储在了一个结构体中 且 `存储的是函数指针` 且 是编译时常量

那也就是说，我们的 JNIEnv 提供的方法，其实是在 JNIEnv 创建的时候，GetFunctionTable 拿到了一堆`函数指针`，然后`赋值给了 JNIEnv 底层的 JNINativeInterface`。

这个可以理解为一个`绑定的过程`！
>JNIEnv 提前声明了一堆函数指针，但是这些指针指向哪片内存呢？不知道！当 JNIEnv 创建的时候，将这些提前声明的指针一一赋值到对应的函数地址。这些函数地址又是怎么来的呢？当然是系统库的源码里实现的，已经声明声明好了的函数指针。

### 如何 hook

由上面的内容，我们已经知道 JNIEnv 的一些函数都是直接通过函数指针调用，所以并没有外部符号，所以 xhook 应该是行不通了。

那我们就可以采用其他文章中提供的一些思路：`直接替换函数指针`

但是替换函数指针也有两个层面，所以也有两种方式

由之前我们已经知道 JNIEnv 其实是靠内部的 JNINativeInterface，代码中叫做 functions

```c
// jni.h
struct _JNIEnv {
    // 全部依靠 JNINativeInterface
    const struct JNINativeInterface* functions;

    jint GetVersion()
    { return functions->GetVersion(this); }

    // 无限多的 JNIEnv 方法，如 NewString、NewStringUTF 等等

```
那这里其实就有两种方式了:
+ 替换 functions 这个 JNINativeInterface 指针，将这个结构整体替换，那么间接的就达到了 hook 的目的。
+  直接替换 JNINativeInterface 内部调用的各个函数指针的地址，也能达到 hook 的目的

两者各有利弊！但是，其实代码实现起来差异不是很大。

#### 替换 functions 结构本身

这种方式因为和 JNIEnv(JNINativeInterface) 绑定，所以`只能 hook 同一个 JNIEnv 对象`。

比如，此时我们都在主线程调用一些 JNI 的方法，那么此时所有的 JNIEnv 是同一个对象，那么此时我们替换了 functions 的话，只是对于主线程调用的方法生效。此时子线程执行 JNI 调用，是另一个 JNIEnv 对象，所以 hook 不生效。

这就是这种 hook 方式的`弊端`！无法一次 hook 所有的 JNI 方法调用。

大致代码如下(示例 hook 单个 NewStringUTF 方法)：：
```c
// 定义一个函数指针，用于存放原始 NewStringUTF 函数
jstring (*originNewString)(JNIEnv *env,  const char * obj);
// 定义一个新的 NewStringUTF 函数
jstring NewStringUTFOverride(JNIEnv *env,  const char * obj) {
    LOGGER("hook new string:%s",  obj);
    return originNewString(env, obj);
}

// 创建一个 JNINativeInterface
JNINativeInterface *newTable = new JNINativeInterface;
// 这里不是指针类型，会促成一次拷贝，所以 oldTable 会 copy 一份原始的 functions 的各个函数指针
JNINativeInterface oldTable = *env->functions;
// 解引用赋值，此时 newTable 就有了一摸一样的 functions
*newTable = oldTable;
// 存之前的指针
originNewString = oldTable.NewStringUTF;
// 替换新的 NewStringUTF
newTable->NewStringUTF = NewStringUTFOverride;
// 替换当前 env 的所有 functions
env->functions = newTable;
```

为什么要 copy 一份？是因为原始的内存区间不可写，所以 copy 了一份。

这种方式就是不用修改内存属性，但是你也看到了需要把每个 env 的 functions 都替换掉，才能实现全局 hook，略有不足。

#### 替换 functions 内部的函数内容

这种方式，直接修改函数指针的地址，但是因为这片内存区间只是可读的，不可写，所以需要修改内存区域的属性为可写(使用 `mprotect`)，这种方式可以 `全局生效`。

先来介绍一下 mprotect:

```c
// 修改内存属性，addr 是 NewStringUTF 地址，page_size 为一页大小，不过最好修改完了以后，再恢复为 PROT_READ
int prot = PROT_READ | PROT_WRITE;
mprotect((void*)addr, page_size, prot);
```
mprotect 需要注意的是 addr ·一定要是内存页起始地址(·页大小的整数倍)，后面是要修改的内存大小，我这里传入的就是一页的大小。

总体 hook 的大致代码如下(示例 hook 单个 NewStringUTF 方法)：
```c
// 定义一个函数指针，用于存放原始 NewStringUTF 函数
jstring (*originNewString)(JNIEnv *env,  const char * obj);

// 定义一个新的 NewStringUTF 函数
jstring NewStringUTFOverride(JNIEnv *env,  const char * obj) {
    LOGGER("hook new string :%s",  obj);
    return originNewString(env, obj);
}

// 获取 JNINativeInterface
JNINativeInterface *funcTable = (JNINativeInterface *)env->functions;
// 获取原始 NewStringUTF 函数指针本身的地址，本身是一个地址值，也可以用一个二级指针代表，无所谓什么类型接受
void** funcPointerAddress = (void **)&(funcTable->NewStringUTF);
// 修改对应地址的内存空间属性为可写
// 需要将 funcPointerAddress 按照 long 值解析，算出一个页大小整数倍的起始地址，调用 mprotect

// 将老的函数指针保存
originNewString = funcTable->NewStringUTF;
// 获取自己写的替换方法的函数指针的地址
void** newFuncPointerAddress = reinterpret_cast<void **>(&NewStringUTFOverride);
// 替换
*funcPointerAddress = newFuncPointerAddress;
```

## 总结

经过上述的一通学习，也觉得 JNI `使用函数指针十分合理`。

这样，我们在编写 so 库时，根本不需要考虑添加任何系统的动态链接库。

在平时的底层开发中，CMakeList 里，我们从没有添加过诸如 libart 等动态链接库， 但是我们照样能成功调用 JNI 提供的一些接口方法，这里就是函数指针的魔法了！

## 参考
https://bbs.pediy.com/thread-200398.htm

https://blog.csdn.net/taohongtaohuyiwei/article/details/105204579