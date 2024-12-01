package com.xingyu.screencastserver.video

import android.util.Size
import android.view.Surface

abstract class VideoCapture {
    abstract fun init()
    abstract fun start(surface: Surface)
    abstract fun release()
    abstract val size: Size
}