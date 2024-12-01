
#include "AndroidMediaSink.h"

#include <android/log.h>

#define LOG_TAG "AndroidMediaSink"

// static
AndroidMediaSink *AndroidMediaSink::createNew(UsageEnvironment &env,
                                              MediaSubsession &subsession,
                                              const char *streamId) {
    return new AndroidMediaSink(env, subsession, streamId);
}

#define RECEIVE_BUFFER_SIZE 1000000

AndroidMediaSink::AndroidMediaSink(UsageEnvironment &env,
                                   MediaSubsession &subsession,
                                   const char *streamId)
        : MediaSink(env), mSubsession(subsession), mStreamId(streamId) {

    mReceiveBuffer.resize(RECEIVE_BUFFER_SIZE);
}

AndroidMediaSink::~AndroidMediaSink() {

}

// static
void AndroidMediaSink::afterGettingFrame(void *clientData,
                                         unsigned int frameSize,
                                         unsigned int numTruncatedBytes,
                                         struct timeval presentationTime,
                                         unsigned int durationInMicroseconds) {
    auto sink = reinterpret_cast<AndroidMediaSink *>(clientData);
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void AndroidMediaSink::afterGettingFrame(unsigned int frameSize,
                                         unsigned int numTruncatedBytes,
                                         struct timeval presentationTime,
                                         unsigned int durationInMicroseconds) {

    static std::vector<uint8_t> startCode = {0x00, 0x00, 0x01};

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        R"(Stream "%s": %s/%s: Received %d bytes (with %d bytes truncated) Presentation time:%ld.%06ld NPT:%f)",
                        mStreamId.c_str(),
                        mSubsession.mediumName(),
                        mSubsession.codecName(),
                        frameSize,
                        numTruncatedBytes,
                        presentationTime.tv_sec,
                        presentationTime.tv_usec,
                        mSubsession.getNormalPlayTime(presentationTime));

    [this, frameSize, presentationTime]() {
        std::unique_lock<std::mutex> lk(mBufferQueueMutex);
        if (!mStartGetFrame) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "mStartGetFrame = false, skip queue buffer");
            return;
        }

        auto buffer = std::make_shared<MediaCodecBuffer>();
        auto &v = buffer->buffer;
        v.insert(v.end(), startCode.begin(), startCode.end());
        v.insert(v.end(), mReceiveBuffer.data(), mReceiveBuffer.data() + frameSize);
        auto pts = mSubsession.getNormalPlayTime(presentationTime);
        buffer->timestampUs = (uint64_t) (pts * 1000000);
        mBufferQueue.emplace(buffer);

        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "queue buffer size:%zu pts:%lu BufferQueue size:%zu",
                            buffer->buffer.size(),
                            buffer->timestampUs,
                            mBufferQueue.size());

        mBufferQueueCv.notify_all();
    }();

    // Then continue, to request the next frame of data:
    continuePlaying();
}

Boolean AndroidMediaSink::continuePlaying() {
    if (fSource == nullptr) return False; // sanity check (should not happen)

    // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
    fSource->getNextFrame(mReceiveBuffer.data(),
                          mReceiveBuffer.size(),
                          afterGettingFrame, this,
                          onSourceClosure, this);
    return True;
}

std::shared_ptr<MediaCodecBuffer> AndroidMediaSink::getNextFrame() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "enter getNextFrame");

    std::unique_lock<std::mutex> lk(mBufferQueueMutex);
    mStartGetFrame = true;
    while (mBufferQueue.empty()) {
        mBufferQueueCv.wait(lk);
    }
    auto buffer = mBufferQueue.front();
    mBufferQueue.pop();

    if (buffer == nullptr) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "exit getNextFrame, get null buffer");
    } else {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "exit getNextFrame, get buffer size:%zu pts:%lu BufferQueue size:%zu",
                            buffer->buffer.size(),
                            buffer->timestampUs,
                            mBufferQueue.size());
    }

    return buffer;
}

void AndroidMediaSink::stopGetFrame() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "stopGetFrame");

    std::unique_lock<std::mutex> lk(mBufferQueueMutex);
    mStartGetFrame = false;
    mBufferQueue.emplace(nullptr);
    mBufferQueueCv.notify_all();
}
