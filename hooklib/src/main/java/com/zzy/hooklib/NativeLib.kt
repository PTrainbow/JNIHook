package com.zzy.hooklib

import android.util.Log

class NativeLib {

    companion object {
        // Used to load the 'hooklib' library on application startup.
        init {
            System.loadLibrary("hooklib")
        }

        fun init(){
            Log.i("NativeLib", "empty init function")
        }
    }
}