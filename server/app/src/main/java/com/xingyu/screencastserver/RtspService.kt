package com.xingyu.screencastserver

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.media.projection.MediaProjection
import android.media.projection.MediaProjectionManager
import android.os.Binder
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Message
import android.util.Log
import androidx.annotation.RequiresApi
import com.xingyu.screencastserver.video.ScreenCapture
import com.xingyu.screencastserver.video.VideoCodec
import com.xingyu.screencastserver.video.VideoEncoder
import java.nio.ByteBuffer
import kotlin.concurrent.thread

class RtspService : Service() {

    inner class RtspServiceBinder : Binder() {
        fun getService(): RtspService = this@RtspService
    }

    //    private var serverThread:Thread? = null
    private var videoEncoder: VideoEncoder? = null
    private val binder = RtspServiceBinder()
    private var activityMsgHandler: Handler? = null

    companion object {
        private val TAG = RtspService::class.simpleName
        private const val CHANNEL_ID = "RtspService"
    }


    private external fun nativeStartServer(encoder: VideoEncoder)
    private external fun nativeStopServer(encoder: VideoEncoder)
    private external fun nativeQueueBuffer(
        encoder: VideoEncoder,
        buffer: ByteBuffer,
        size: Int,
        isCsd: Boolean,
        isKeyFrame: Boolean
    )

    override fun onCreate() {
        super.onCreate()
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.i(TAG, "onStartCommand")

        val notification = createNotification()
        startForeground(110, notification)

        intent?.let {
            val code = it.getIntExtra("code", -1)
            val data = it.getParcelableExtra("data", Intent::class.java) as Intent
            val mediaProjectionManager =
                getSystemService(MEDIA_PROJECTION_SERVICE) as MediaProjectionManager
            val mediaProjection = mediaProjectionManager.getMediaProjection(code, data)
            Log.i(TAG, "Success to create media projectiion")

            startServer(mediaProjection)
        } ?: {
            Log.e(TAG, "Null intent when start service")
        }


        return super.onStartCommand(intent, flags, startId)
    }

    private fun createNotification(): Notification {
        val serviceChannel =
            NotificationChannel(CHANNEL_ID, "RtspService", NotificationManager.IMPORTANCE_DEFAULT)
        val manager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        manager.createNotificationChannel(serviceChannel)

        val intent = Intent(this, MainActivity::class.java)
        val notification = Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("ScreencastServer")
            .setContentText("ScreencastServer service running")
            .setContentIntent(
                PendingIntent.getActivity(
                    this,
                    0,
                    intent,
                    PendingIntent.FLAG_IMMUTABLE
                )
            )
            .setOngoing(true)
            .build()

        return notification
    }

    private fun startServer(mediaProjection: MediaProjection) {
        val displayMetrics = resources.displayMetrics
        val width = displayMetrics.widthPixels
        val height = displayMetrics.heightPixels
        val size = android.util.Size(width, height)
        val dpi = displayMetrics.densityDpi

        Log.i(TAG, "width:$width height:$height dpi:$dpi")

        Log.i(TAG, "start server")

        mediaProjection.registerCallback(object : MediaProjection.Callback() {

        }, null)

        val videoCapture = ScreenCapture(mediaProjection, size, dpi)
        videoEncoder =
            VideoEncoder(videoCapture, VideoCodec.H265, 4_000_000, size, ::nativeQueueBuffer)

        videoEncoder?.let {
            it.start()
            thread(name = "ServerThread") {
                nativeStartServer(it)
            }
        }
    }

    fun stopServer() {
        Log.i(TAG, "stop service")

        videoEncoder?.let {
            it.stop()
            it.join()
            nativeStopServer(it)
        }

        stopSelf()
    }

    fun setActivityMsgHandler(handler: Handler) {
        activityMsgHandler = handler
    }

    fun announceStream(rtspAddr: String) {
        activityMsgHandler?.let {
            val msg = Message.obtain()
            msg.obj = rtspAddr
            it.sendMessage(msg)
        }
    }
}