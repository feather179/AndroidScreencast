
#include "H265LiveVideoServerMediaSubsession.h"
#include "LiveVideoSource.h"
#include <H265VideoRTPSink.hh>
#include <H265VideoStreamFramer.hh>

#include "Backtrace.h"

#include <android/log.h>

#define LOG_TAG "MediaSubsession"

H265LiveVideoServerMediaSubsession *
H265LiveVideoServerMediaSubsession::createNew(UsageEnvironment &env,
                                              std::shared_ptr<Encoder> encoder) {
    return new H265LiveVideoServerMediaSubsession(env, encoder);
}

H265LiveVideoServerMediaSubsession::H265LiveVideoServerMediaSubsession(UsageEnvironment &env,
                                                                       std::shared_ptr<Encoder> encoder)
        : OnDemandServerMediaSubsession(env, true), mEncoder(encoder) {

}

H265LiveVideoServerMediaSubsession::~H265LiveVideoServerMediaSubsession() {

}

FramedSource *
H265LiveVideoServerMediaSubsession::createNewStreamSource(unsigned int clientSessionId,
                                                          unsigned int &estBitrate) {
    estBitrate = 8000;

    LiveVideoSource *source = LiveVideoSource::createNew(envir(), mEncoder);
    if (source == nullptr) return nullptr;

    return H265VideoStreamFramer::createNew(envir(), source);
}

RTPSink *H265LiveVideoServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock,
                                                              unsigned char rtpPayloadTypeIfDynamic,
                                                              FramedSource *inputSource) {
    std::vector<uint8_t> csd;
    mEncoder->getCsd(csd);

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "createNewRTPSink csd size:%zu", csd.size());

    if (csd.empty()) {
        return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    }

    std::vector<uint8_t> vps, sps, pps;
    // parse csd
    [&csd, &vps, &sps, &pps]() {
        std::vector<uint8_t> startCode = {0x00, 0x00, 0x01};
        auto it = std::search(csd.begin(), csd.end(), startCode.begin(), startCode.end());

        while (it != csd.end()) {
            auto start = it + startCode.size();

            it = std::search(start, csd.end(), startCode.begin(), startCode.end());
            auto end = it;
            int pos = end - csd.begin();
            if (pos > 0 && csd[pos - 1] == 0)
                end--;

            uint8_t naluType = (*start & 0x7E) >> 1;

            std::vector<uint8_t> *v = nullptr;
            if (naluType == 32) {   // vps
                v = &vps;
            } else if (naluType == 33) {    // sps
                v = &sps;
            } else if (naluType == 34) {    // pps
                v = &pps;
            }

            if (v != nullptr) {
                int index = 0;
                for (auto i = start; i != end; i++) {
                    if (index >= 2 && *i == 3 && *(i - 1) == 0 && *(i - 2) == 0) {

                    } else {
                        v->push_back(*i);
                        index++;
                    }
                }
            }
        }

    }();

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                        "parse csd done, vps size:%zu sps size:%zu pps size:%zu",
                        vps.size(), sps.size(), pps.size());

    return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, vps.data(),
                                       vps.size(), sps.data(), sps.size(), pps.data(), pps.size());

}

static void checkForAuxSDPLine(void *clientData) {
    auto subsess = (H265LiveVideoServerMediaSubsession *) clientData;
    subsess->checkForAuxSDPLineInternal();
}


void H265LiveVideoServerMediaSubsession::checkForAuxSDPLineInternal() {
    nextTask() = NULL;

    char const *dasl;
    if (fAuxSDPLine != NULL) {
        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (fDummyRTPSink != NULL && (dasl = fDummyRTPSink->auxSDPLine()) != NULL) {
        fAuxSDPLine = strDup(dasl);
        fDummyRTPSink = NULL;

        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (!fDoneFlag) {
        // try again after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                                 (TaskFunc *) checkForAuxSDPLine,
                                                                 this);
    }
}


static void afterPlayingDummy(void *clientData) {
    auto subsess = (H265LiveVideoServerMediaSubsession *) clientData;
    subsess->afterPlayingDummyInternal();
}

void H265LiveVideoServerMediaSubsession::afterPlayingDummyInternal() {
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}


char const *
H265LiveVideoServerMediaSubsession::getAuxSDPLine(RTPSink *rtpSink, FramedSource *inputSource) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "getAuxSDPLine");

    if (fAuxSDPLine != nullptr) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "getAuxSDPLine: fAuxSDPLine %s",
                            fAuxSDPLine);
        return fAuxSDPLine; // it's already been set up (for a previous client)
    }

    char const *dasl = rtpSink->auxSDPLine();
    if (dasl != nullptr) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "getAuxSDPLine directly: %s", dasl);
        fAuxSDPLine = strDup(dasl);
        return fAuxSDPLine;
    }

    if (fDummyRTPSink ==
        nullptr) { // we're not already setting it up for another, concurrent stream
        // Note: For H265 video files, the 'config' information (used for several payload-format
        // specific parameters in the SDP description) isn't known until we start reading the file.
        // This means that "rtpSink"s "auxSDPLine()" will be NULL initially,
        // and we need to start reading data from our file until this changes.
        fDummyRTPSink = rtpSink;

        // Start reading the file:
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

        // Check whether the sink's 'auxSDPLine()' is ready:
        checkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&fDoneFlag);

    return fAuxSDPLine;
}


