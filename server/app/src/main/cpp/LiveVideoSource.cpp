
#include "LiveVideoSource.h"

#include <android/log.h>

#define LOG_TAG "LiveVideoSource"

// static
LiveVideoSource *
LiveVideoSource::createNew(UsageEnvironment &env, std::shared_ptr<Encoder> encoder) {
    return new LiveVideoSource(env, encoder);
}


LiveVideoSource::LiveVideoSource(UsageEnvironment &env, std::shared_ptr<Encoder> encoder)
        : FramedSource(env), mEncoder(encoder) {
    mEventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame);
}

LiveVideoSource::~LiveVideoSource() {
    mEncoder->stop();
    mEncoder->setOnBufferAvailCB(nullptr);
    envir().taskScheduler().deleteEventTrigger(mEventTriggerId);
}

void LiveVideoSource::doGetNextFrame() {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "doGetNextFrame");

    if (mEncoder->getState() != Encoder::ENCODER_STATE_STARTED) {
        mEncoder->setOnBufferAvailCB(
                [this](std::shared_ptr<std::vector<uint8_t>> buffer, bool isCsd, bool isKeyFrame) {
                    queueBuffer(buffer, isCsd, isKeyFrame);
                });
        mEncoder->start();

        // already queued csd buffer before every IDR
//        std::vector<uint8_t> csd;
//        mEncoder->getCsd(csd);
//        auto csdBuffer = std::make_shared<std::vector<uint8_t>>(csd);
//        queueBuffer(csdBuffer, true, false);
    }

    if (0) {
        handleClosure();
        return;
    }

    deliverFrameInternal();

}

// static
void LiveVideoSource::deliverFrame(void *clientData) {
    ((LiveVideoSource *) clientData)->deliverFrameInternal();
}

void LiveVideoSource::deliverFrameInternal() {
    if (!isCurrentlyAwaitingData()) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "we're not ready for the data yet");
        return;     // we're not ready for the data yet
    }

    {
        std::unique_lock<std::mutex> lk(mBufferQueueMutex);
        if (mBufferQueue.empty()) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "empty buffer queue");
            return;
        }


        auto buffer = mBufferQueue.front();
        mBufferQueue.pop();
        auto len = buffer->size();
        if (len > fMaxSize) {
            fFrameSize = fMaxSize;
            fNumTruncatedBytes = len - fMaxSize;
        } else {
            fFrameSize = len;
            fNumTruncatedBytes = 0;
        }

        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "get buffer size:%zu fMaxSize:%d fFrameSize:%d fNumTruncatedBytes:%d",
                            len, fMaxSize, fFrameSize, fNumTruncatedBytes);

        gettimeofday(&fPresentationTime, nullptr);
        memmove(fTo, buffer->data(), fFrameSize);
    }

    // After delivering the data, inform the reader that it is now available:
    FramedSource::afterGetting(this);
}

void LiveVideoSource::queueBuffer(std::shared_ptr<std::vector<uint8_t>> buffer, bool isCsd,
                                  bool isKeyFrame) {

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "queueBuffer size:%zu isCsd:%d isKeyFrame:%d mReceivedKeyFrame:%d",
                        buffer->size(), isCsd, isKeyFrame, mReceivedKeyFrame);

    if (!mReceivedKeyFrame && !isKeyFrame && !isCsd)
        return;

    if (isKeyFrame)
        mReceivedKeyFrame = true;

    std::unique_lock<std::mutex> lk(mBufferQueueMutex);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "queue buffer size:%zu", buffer->size());
    mBufferQueue.emplace(buffer);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "after queue BQ size:%zu", mBufferQueue.size());
    envir().taskScheduler().triggerEvent(mEventTriggerId, this);
}

unsigned LiveVideoSource::maxFrameSize() const {
    return 1000000;
}

