package com.zzy.jnihook

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.zzy.hooklib.NativeLib
import com.zzy.jnihook.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        NativeLib.init()
        binding.testBtn.setOnClickListener {
            stringFromJNI()
        }
    }

    /**
     * A native method that is implemented by the 'jnihook' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    companion object {
        // Used to load the 'jnihook' library on application startup.
        init {
            System.loadLibrary("jnihook")
        }
    }
}