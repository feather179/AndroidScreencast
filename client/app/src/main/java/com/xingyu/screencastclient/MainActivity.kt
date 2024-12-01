package com.xingyu.screencastclient

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.RadioButton
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {

    private lateinit var etRtspAddr: EditText
    private lateinit var rbRtspTCP: RadioButton

    companion object {
        private val TAG = MainActivity::class.simpleName

        init {
            System.loadLibrary("screencastclient")
        }
    }

    private external fun nativeInit()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)

        nativeInit()

        etRtspAddr = findViewById(R.id.etRtspAddr)
        rbRtspTCP = findViewById(R.id.rbRtspTCP)
        findViewById<Button>(R.id.btnOpenRtsp).setOnClickListener {
            openRtsp()
        }
    }

    private fun openRtsp() {
        val rtspAddr = etRtspAddr.text.toString().trim()
        val isTcp = rbRtspTCP.isChecked

        val intent = Intent(this, VideoPlayActivity::class.java)
        intent.putExtra("RtspAddr", rtspAddr)
        intent.putExtra("IsTcp", isTcp)
        startActivity(intent)
    }
}
