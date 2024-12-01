package com.xingyu.screencastserver.video

import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.projection.MediaProjection
import android.util.Log
import android.util.Size
import android.view.Surface

class ScreenCapture(
    private val mediaProjection: MediaProjection,
    override val size: Size,
    private val dpi: Int
) : VideoCapture() {

    private var virtualDisplay: VirtualDisplay? = null
    
    companion object {
        private val TAG = ScreenCapture::class.simpleName
    }


    override fun init() {
    }

    override fun start(surface: Surface) {
        virtualDisplay?.release()
        virtualDisplay = null

        virtualDisplay = mediaProjection.createVirtualDisplay(
            "ScreencastServer",
            size.width,
            size.height,
            dpi,
            DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
            surface,
            null,
            null
        )

        Log.i(TAG, "Success to create virtual display")
    }

    override fun release() {
        virtualDisplay?.release()
        virtualDisplay = null

        mediaProjection.stop()
    }
}