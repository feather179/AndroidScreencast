package com.xingyu.screencastserver

import android.Manifest
import android.content.ComponentName
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.Message
import android.util.Log
import android.view.Menu
import android.view.MenuItem
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat

class MainActivity : AppCompatActivity() {

    private lateinit var mediaProjectionManager: MediaProjectionManager
    private val mediaProjectionLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode == RESULT_OK) {
                Toast.makeText(
                    this,
                    "Success to get screen record permission",
                    Toast.LENGTH_SHORT
                ).show()

                val intent = Intent(this, RtspService::class.java)
                intent.putExtra("code", result.resultCode)
                intent.putExtra("data", result.data)
                startForegroundService(intent)

            } else {
                Toast.makeText(
                    this,
                    "Failed to get screen record permission",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }

    private val requestPermissions = buildList {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            add(Manifest.permission.POST_NOTIFICATIONS)
        }
    }.toTypedArray()

    private val requestPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { it ->
            if (it.all { it.value }) {
                Toast.makeText(this, "All permissions granted", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(
                    this,
                    "Please grant all required permissions firstly",
                    Toast.LENGTH_SHORT
                ).show()
            }
        }

    private var rtspService: RtspService? = null
    private var rtspServiceBound = false

    private val serviceMsgHandler = object : Handler(Looper.getMainLooper()) {
        override fun handleMessage(msg: Message) {
            Log.i(TAG, "Received message:${msg.obj}")
            tvRtspAddr.text = msg.obj.toString()
        }
    }

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            val binder = service as RtspService.RtspServiceBinder
            rtspService = binder.getService()
            rtspServiceBound = true
            rtspService?.setActivityMsgHandler(serviceMsgHandler)
            Log.i(TAG, "Success to get service connection")
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            rtspServiceBound = false
            rtspService = null
        }

    }

    private lateinit var tvRtspAddr: TextView

    private external fun nativeInit()

    companion object {
        private val TAG = MainActivity::class.simpleName

        init {
            System.loadLibrary("screencastserver")
        }
    }


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)

        nativeInit()

        bindService(Intent(this, RtspService::class.java), serviceConnection, BIND_AUTO_CREATE)

        mediaProjectionManager =
            getSystemService(MEDIA_PROJECTION_SERVICE) as MediaProjectionManager


        findViewById<Button>(R.id.btnGrantPermissions).setOnClickListener {
            grantPermissions()
        }

        findViewById<Button>(R.id.btnStartServer).setOnClickListener {
            startServer()
        }

        findViewById<Button>(R.id.btnStopServer).setOnClickListener {
            stopServer()
        }

        tvRtspAddr = findViewById(R.id.tvRtspAddr)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        // Inflate the menu; this adds items to the action bar if it is present.
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        return when (item.itemId) {
            R.id.action_settings -> true
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun allPermissionsGranted(): Boolean {
        requestPermissions.forEach {
            if (ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED) {
                return false
            }
        }

        return true
    }

    private fun grantPermissions() {
        if (allPermissionsGranted()) {
            Toast.makeText(this, "All permissions granted", Toast.LENGTH_SHORT).show()
        } else {
            requestPermissionLauncher.launch(requestPermissions)
        }
    }

    private fun startServer() {
        Toast.makeText(this, "Start screencast server...", Toast.LENGTH_SHORT).show()

        mediaProjectionLauncher.launch(mediaProjectionManager.createScreenCaptureIntent())
    }

    private fun stopServer() {
        rtspService?.stopServer()
    }
}