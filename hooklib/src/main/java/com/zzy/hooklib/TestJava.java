package com.zzy.hooklib;

import android.util.Log;

import java.util.Arrays;

public class TestJava {
    public static int method(int... args) {
        Log.i("Test", "method1 is called, receive params " + Arrays.toString(args));
        return  0;
    }

    public static int method2(String... args) {
        Log.i("Test", "method2 is called, receive params " + Arrays.toString(args));
        return  0;
    }
}
