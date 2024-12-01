package com.xingyu.screencastclient

import java.nio.ByteBuffer

data class MediaCodecBuffer(var buffer: ByteBuffer, var size: Int = 0, var timestampUs: Long = 0)
