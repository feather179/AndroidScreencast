package com.xingyu.screencastclient

import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.media.MediaCodec
import android.media.MediaFormat
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.os.Message
import android.util.Log
import android.util.Size
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.Window
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.Toast
import androidx.activity.enableEdgeToEdge
import androidx.annotation.RequiresApi
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import java.nio.ByteBuffer
import java.util.concurrent.TimeUnit
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.thread
import kotlin.math.ceil

class VideoPlayActivity : AppCompatActivity() {
    private var isFullScreen = false
    private lateinit var flMainFrame: FrameLayout

    private val landscapeLayoutParam = LinearLayout.LayoutParams(
        LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT
    )
    private val portraitLayoutParam = LinearLayout.LayoutParams(
        LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT
    )
    private lateinit var landscapeScreenSize: Size
    private lateinit var portraitScreenSize: Size

    private val btnOnClickListener = View.OnClickListener { it ->
        when (it.id) {
            R.id.btnVideoBack -> {
                Log.i(TAG, "video back button clicked")
            }

            R.id.btnVideoFullScreen -> {
                Log.i(TAG, "video full screen button clicked")

                requestedOrientation =
                    if (resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE) {
                        ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                    } else {
                        ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                    }
            }
        }
    }

    private lateinit var surfaceView: SurfaceView
    private val surfaceViewCallback = object : SurfaceHolder.Callback {
        override fun surfaceCreated(holder: SurfaceHolder) {
            Log.i(TAG, "surfaceCreated")
            startPlay()
        }

        override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            Log.i(TAG, "surfaceChanged")
        }

        override fun surfaceDestroyed(holder: SurfaceHolder) {
            Log.i(TAG, "surfaceDestroyed")
            stopPlay()
        }
    }

    private var nativeContext: Long = 0
    private val nativeContextLock = ReentrantLock()
    private val nativeContextCondition = nativeContextLock.newCondition()

    private var videoDecoderIsRunning = false
    private val videoDecoderLock = ReentrantLock()
    private val videoDecoderCondition = videoDecoderLock.newCondition()
    private val videoDecoderHandlerThread = HandlerThread("VideoDecoder")
    private lateinit var videoDecoderHandler: Handler

    @RequiresApi(Build.VERSION_CODES.R)
    private val videoDecoderHandlerCallback = Handler.Callback { it ->
        when (it.what) {
            START_DECODER -> {
                Log.i(TAG, "start decoder")

                // wait another thread to create RTSPClient
                nativeContextLock.lock()
                while (nativeContext == 0.toLong()) {
                    if (!nativeContextCondition.await(1, TimeUnit.SECONDS)) {
                        nativeContextLock.unlock()
                        Log.e(TAG, "Wait to create RTSPClient timeout")
                        runOnUiThread {
                            Toast.makeText(this, "Failed to create RTSPClient", Toast.LENGTH_SHORT)
                                .show()
                        }
                        finish()    // exit activity
                        return@Callback true
                    }
                }
                nativeContextLock.unlock()

                // wait RTSPClient start to play done
                if (nativeWaitRtspStart(3000)) {
                    Log.i(TAG, "Wait RRTSPClient start to play done")
                } else {
                    Log.e(TAG, "Wait RRTSPClient start to play timeout")
                    nativeCloseRtsp()
                    runOnUiThread {
                        Toast.makeText(this, "RTSPClient connect timeout", Toast.LENGTH_SHORT)
                            .show()
                    }
                    finish()    // exit activity
                    return@Callback true
                }

                if (videoDecoder == null) {
                    val mimeType = getMimeType()
                    Log.i(TAG, "RTSP mime type:$mimeType")

                    val mediaCodec = MediaCodec.createDecoderByType(mimeType)
                    Log.d(TAG, "Using video decoder: ${mediaCodec.name}")

                    val format = MediaFormat.createVideoFormat(mimeType, 1920, 1080)
                    format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
                    videoDecoderFormat = format
                    videoDecoder = mediaCodec
                }

                videoDecoder?.let {
                    it.setCallback(videoDecoderCallback)
                    it.configure(videoDecoderFormat!!, surfaceView.holder.surface, null, 0)
                    it.start()
                }

                videoDecoderIsRunning = true
            }

            STOP_DECODER -> {
                Log.i(TAG, "stop decoder")
                videoDecoder?.let {
                    it.stop()
                    it.reset()
                }

                videoDecoderLock.lock()
                videoDecoderIsRunning = false
                videoDecoderCondition.signalAll()
                videoDecoderLock.unlock()
            }

            else -> {

            }
        }
        return@Callback true
    }

    private var videoDecoder: MediaCodec? = null
    private var videoDecoderFormat: MediaFormat? = null
    private val videoDecoderCallback = object : MediaCodec.Callback() {
        override fun onInputBufferAvailable(codec: MediaCodec, index: Int) {
            Log.i(TAG, "onInputBufferAvailable index:$index")

            val buffer = codec.getInputBuffer(index) as ByteBuffer
            val mediaCodecBuffer = MediaCodecBuffer(buffer)

            Log.i(TAG, "start get native buffer")
            if (nativeGetInputBuffer(mediaCodecBuffer)) {
                val size = mediaCodecBuffer.size
                val pts = mediaCodecBuffer.timestampUs
                Log.i(TAG, "get native buffer done: size:$size pts:$pts")
                codec.queueInputBuffer(index, 0, size, pts, 0)
            } else {
                Log.w(TAG, "failed to get native buffer")
            }
        }

        override fun onOutputBufferAvailable(
            codec: MediaCodec,
            index: Int,
            info: MediaCodec.BufferInfo
        ) {
            Log.i(TAG, "onOutputBufferAvailable index:$index pts:${info.presentationTimeUs}")

            codec.releaseOutputBuffer(index, true)
        }

        override fun onError(codec: MediaCodec, e: MediaCodec.CodecException) {
            Log.e(TAG, "onError: $e")
        }

        override fun onOutputFormatChanged(codec: MediaCodec, format: MediaFormat) {
            Log.i(TAG, "onOutputFormatChanged format:$format")

            val width = format.getInteger(MediaFormat.KEY_WIDTH)
            val height = format.getInteger(MediaFormat.KEY_HEIGHT)

            // min scale factor
            val landscapeScale =
                (landscapeScreenSize.width.toFloat() / width.toFloat()).coerceAtMost(
                    landscapeScreenSize.height.toFloat() / height.toFloat()
                )
            landscapeLayoutParam.width = ceil(width * landscapeScale).toInt()
            landscapeLayoutParam.height = ceil(height * landscapeScale).toInt()

            Log.i(
                TAG,
                "onOutputFormatChanged landscape: screen(${landscapeScreenSize.width}x${landscapeScreenSize.height}) " +
                        "param(${landscapeLayoutParam.width}x${landscapeLayoutParam.height})"
            )

            val portraitScale =
                (portraitScreenSize.width.toFloat() / width.toFloat()).coerceAtMost(
                    portraitScreenSize.height.toFloat() / height.toFloat()
                )
            portraitLayoutParam.width = ceil(width * portraitScale).toInt()
            portraitLayoutParam.height = ceil(height * portraitScale).toInt()

            Log.i(
                TAG,
                "onOutputFormatChanged portrait: screen(${portraitScreenSize.width}x${portraitScreenSize.height}) " +
                        "param(${portraitLayoutParam.width}x${portraitLayoutParam.height})"
            )

            runOnUiThread {
                if (resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE) {
                    flMainFrame.layoutParams = landscapeLayoutParam
                } else {
                    flMainFrame.layoutParams = portraitLayoutParam
                }
            }
        }

    }

    companion object {
        private val TAG = VideoPlayActivity::class.simpleName

        private const val START_DECODER = 1
        private const val STOP_DECODER = 2
    }

    private external fun nativeOpenRtsp(rtspAddr: String, isTcp: Boolean)
    private external fun nativeWaitRtspStart(milliseconds: Int): Boolean
    private external fun nativeGetMimeType(): String
    private external fun nativeGetInputBuffer(buffer: MediaCodecBuffer): Boolean
    private external fun nativeStopGetInputBuffer()
    private external fun nativeCloseRtsp()

    private fun hideSystemUI() {
        val windowInsetsController = WindowCompat.getInsetsController(window, window.decorView)
        // Configure the behavior of the hidden system bars
        windowInsetsController.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE

        // Hide status bar and navigation bar
        windowInsetsController.hide(WindowInsetsCompat.Type.statusBars())
        windowInsetsController.hide(WindowInsetsCompat.Type.navigationBars())

        // Set light status bar and navigation bar appearance
        windowInsetsController.isAppearanceLightStatusBars = true
        windowInsetsController.isAppearanceLightNavigationBars = true
    }

    private fun getMimeType(): String {
        val nativeMimeType = nativeGetMimeType()
        Log.i(TAG, "native mime type:$nativeMimeType")

        return when (nativeMimeType) {
            "video/H265" -> MediaFormat.MIMETYPE_VIDEO_HEVC
            "video/H264" -> MediaFormat.MIMETYPE_VIDEO_AVC
            else -> MediaFormat.MIMETYPE_VIDEO_HEVC
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_video_play)

        if (resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            landscapeScreenSize = Size(
                windowManager.currentWindowMetrics.bounds.width(),
                windowManager.currentWindowMetrics.bounds.height()
            )
            portraitScreenSize = Size(
                landscapeScreenSize.height, landscapeScreenSize.width
            )
        } else {
            portraitScreenSize = Size(
                windowManager.currentWindowMetrics.bounds.width(),
                windowManager.currentWindowMetrics.bounds.height()
            )
            landscapeScreenSize = Size(
                portraitScreenSize.height, portraitScreenSize.width
            )
        }

        flMainFrame = findViewById(R.id.flMainFrame)
        findViewById<Button>(R.id.btnVideoBack).setOnClickListener(btnOnClickListener)
        findViewById<Button>(R.id.btnVideoFullScreen).setOnClickListener(btnOnClickListener)

        surfaceView = findViewById(R.id.svVideoContent)
        surfaceView.holder.setKeepScreenOn(true)
        surfaceView.holder.addCallback(surfaceViewCallback)

        hideSystemUI()

        val rtspAddr = intent.getStringExtra("RtspAddr") as String
        val isTcp = intent.getBooleanExtra("IsTcp", true)
        openRtsp(rtspAddr, isTcp)

        videoDecoderHandlerThread.start()
        videoDecoderHandler = Handler(videoDecoderHandlerThread.looper, videoDecoderHandlerCallback)
    }

    override fun onDestroy() {
        Log.i(TAG, "onDestroy")

        nativeCloseRtsp()
        videoDecoder?.release()

        super.onDestroy()
    }

    // screen orientation changed
    override fun onConfigurationChanged(newConfig: Configuration) {
        if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            flMainFrame.layoutParams = landscapeLayoutParam
        } else {
            flMainFrame.layoutParams = portraitLayoutParam
        }

        super.onConfigurationChanged(newConfig)
    }

    private fun lockAndGetContext(): Long {
        nativeContextLock.lock()
        return nativeContext
    }

    private fun setAndUnlockContext(context: Long) {
        nativeContext = context
        if (nativeContext != 0.toLong()) {
            nativeContextCondition.signalAll()
        }
        nativeContextLock.unlock()
    }


    private fun openRtsp(rtspAddr: String, isTcp: Boolean) {
        Log.i(TAG, "openRtsp: rtsp address: $rtspAddr iisTcp:$isTcp")
        thread(name = "RtspClientThread") {
            nativeOpenRtsp(rtspAddr, isTcp)
        }
    }

    private fun startPlay() {
        val msg = Message.obtain()
        msg.what = START_DECODER
        videoDecoderHandler.sendMessage(msg)
    }

    private fun stopPlay() {
        if (!videoDecoderIsRunning) {
            Log.i(TAG, "videoDecoderIsRunning = false, skip stopPlay")
            return
        }

        val msg = Message.obtain()
        msg.what = STOP_DECODER
        videoDecoderHandler.sendMessageAtFrontOfQueue(msg)

        nativeStopGetInputBuffer()

        Log.i(TAG, "Waiting for decoder stopped")
        videoDecoderLock.lock()
        while (videoDecoderIsRunning) {
            videoDecoderCondition.await()
        }
        videoDecoderLock.unlock()
        Log.i(TAG, "Decoder stopped")

    }
}