package com.xingyu.screencastserver.video

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Bundle
import android.util.Log
import android.util.Size
import com.xingyu.screencastserver.util.Codec
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.thread

class VideoEncoder(
    private val capture: VideoCapture,
    private val videoCodec: Codec,
    private val bitrate: Int,
    private val size: Size,
    private val onBufferAvailable: (VideoEncoder, ByteBuffer, Int, Boolean, Boolean) -> Unit,
    private val encoderName: String? = null,
) {

    private val mediaCodec = createMediaCodec(videoCodec, encoderName)

    private var encoderThread: Thread? = null
    private var stopped = AtomicBoolean(false)
    private var nativeContext: Long = 0
    private val nativeLock = ReentrantLock()

    companion object {

        private val TAG = VideoEncoder::class.simpleName

        private fun createMediaCodec(codec: Codec, encoderName: String?): MediaCodec {
            if (!encoderName.isNullOrEmpty()) {
                Log.d(TAG, "Creating encoder by name: $encoderName")

                return MediaCodec.createByCodecName(encoderName)
            }


            val mediaCodec = MediaCodec.createEncoderByType(codec.mimeType)
            Log.d(TAG, "Using video encoder:${mediaCodec.name}")
            return mediaCodec
        }


        private fun createMediaFormat(mimeType: String, bitrate: Int, size: Size): MediaFormat {
            val format = MediaFormat()
            val frameRate = 30

            format.setString(MediaFormat.KEY_MIME, mimeType)
            format.setInteger(MediaFormat.KEY_BIT_RATE, bitrate)
            format.setInteger(
                MediaFormat.KEY_BITRATE_MODE, MediaCodecInfo.EncoderCapabilities.BITRATE_MODE_VBR
            )
            format.setInteger(MediaFormat.KEY_FRAME_RATE, frameRate)
            format.setFloat(MediaFormat.KEY_MAX_FPS_TO_ENCODER, frameRate.toFloat())
            format.setInteger(
                MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface
            )
            format.setInteger(MediaFormat.KEY_COLOR_RANGE, MediaFormat.COLOR_RANGE_LIMITED)
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)  // 1s
            format.setLong(
                MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, (1000_000 / frameRate).toLong()
            )
            format.setInteger(MediaFormat.KEY_WIDTH, size.width)
            format.setInteger(MediaFormat.KEY_HEIGHT, size.height)

            return format
        }
    }

    fun lockAndGetContext(): Long {
        nativeLock.lock()
        return nativeContext
    }

    fun setAndUnlockContext(context: Long) {
        nativeContext = context
        nativeLock.unlock()
    }

    private fun streamCapture() {
        Log.d(TAG, "start streamCapture")

        val mediaFormat = createMediaFormat(videoCodec.mimeType, bitrate, size)

        Log.d(TAG, "media format:${mediaFormat}")

        capture.init()

        mediaCodec.configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
        val surface = mediaCodec.createInputSurface()
        capture.start(surface)
        mediaCodec.start()

        encode(mediaCodec)

        mediaCodec.stop()
        Log.d(TAG, "streamCapture finished")
        mediaCodec.release()
        surface.release()
        capture.release()
    }

    private fun encode(mediaCodec: MediaCodec) {
        Log.d(TAG, "enter encode")

        var eos = false
        val bufferInfo = MediaCodec.BufferInfo()

        while (!eos) {
            if (stopped.get()) {
                Log.d(TAG, "video encoder stopped")
                break
            }

            val bufferId = mediaCodec.dequeueOutputBuffer(bufferInfo, -1)
            eos = (bufferInfo.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0
            if (bufferId >= 0) {
                val codecBuffer = mediaCodec.getOutputBuffer(bufferId)
                val pts = bufferInfo.presentationTimeUs
                val isCsd = (bufferInfo.flags and MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0
                val isKeyFrame = (bufferInfo.flags and MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0

                val builder =
                    StringBuilder("Get video encoder output buffer:id$bufferId eos:$eos isCsd:$isCsd isKeyFrame:$isKeyFrame pts:$pts ")
                codecBuffer?.let {
                    builder.append("Buffer len:${it.remaining()}")
                    Log.d(TAG, builder.toString())
                    onBufferAvailable(this, it, it.remaining(), isCsd, isKeyFrame)
                } ?: {
                    builder.append("Codec buffer is NULL")
                    Log.e(TAG, builder.toString())
                }

                mediaCodec.releaseOutputBuffer(bufferId, false)
            }
        }
        Log.d(TAG, "exit encode")
    }


    fun start() {
        Log.d(TAG, "start VideoEncoder")
        stopped.set(false);
        encoderThread = thread(name = "VideoEncoder") {
            streamCapture()

            Log.d(TAG, "VideoEncoder thread stopped")
        }
    }

    fun stop() {
        encoderThread?.let {
            stopped.set(true)
        }
    }

    fun join() {
        encoderThread?.join()
    }

    fun requestSyncFrame() {
        Log.i(TAG, "requestSyncFrame")
        val params = Bundle()
        params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0)
        mediaCodec.setParameters(params)
    }
}