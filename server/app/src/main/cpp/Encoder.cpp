
#include "Encoder.h"

#include <sstream>
#include <iomanip>
#include <android/log.h>

#define LOG_TAG "Encoder"

Encoder::EncoderState Encoder::getState() {
    return mEncoderState;
}

void Encoder::setOnBufferAvailCB(
        std::function<void(std::shared_ptr<std::vector<uint8_t>>, bool, bool)> cb) {
    mBufferAvailCB = cb;
}

void
Encoder::queueBuffer(std::shared_ptr<std::vector<uint8_t>> buffer, bool isCsd, bool isKeyFrame) {
    if (isCsd) {
        mCsd.clear();
        mCsd.assign(buffer->begin(), buffer->end());

        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "queue csd buffer, size:%zu", mCsd.size());

        std::stringstream ss;
        ss << std::hex << std::setw(2) << std::setfill('0');
        for (int i = 1; i <= mCsd.size(); i++) {
            ss << (int) mCsd.at(i - 1) << " ";
            if (i % 16 == 0 || i == mCsd.size()) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "csd data: %s", ss.str().c_str());
                ss.str("");
            }
        }
    }

    if (mBufferAvailCB) {
        if (isKeyFrame && mCsd.size() > 0) {
            auto csdBuffer = std::make_shared<std::vector<uint8_t>>(mCsd);
            mBufferAvailCB(csdBuffer, true, false);
        }
        mBufferAvailCB(buffer, isCsd, isKeyFrame);
    }
}

void Encoder::getCsd(std::vector<uint8_t> &csd) {
    csd.assign(mCsd.begin(), mCsd.end());
}

