package com.mgcs.mgcs_frontend

import android.os.Bundle
import io.flutter.embedding.android.FlutterActivity

class MainActivity : FlutterActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        MGCUsbSerialManager.initialize(applicationContext)
    }

    override fun onDestroy() {
        MGCUsbSerialManager.cleanup(applicationContext)
        super.onDestroy()
    }

    companion object {
        init {
            System.loadLibrary("mgcore")
        }
    }
}
